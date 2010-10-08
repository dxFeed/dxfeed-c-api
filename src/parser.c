/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is Devexperts LLC.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 */

#include "parser.h"
#include "BufferedInput.h"
#include "Subscription.h"
#include "DXMemory.h"
#include "SymbolCodec.h"
#include "DataStructures.h"
#include "Decimal.h"
#include "EventData.h"
#include "RecordTranscoder.h"
#include "RecordBuffers.h"
#include "Logger.h"
#include "DXAlgorithms.h"
#include "ConnectionContextData.h"

/* -------------------------------------------------------------------------- */
/*
 *	Common data
 */
/* -------------------------------------------------------------------------- */

#define SYMBOL_BUFFER_LEN 64

#define MIN_FIELD_TYPE_ID 0x00
#define MAX_FIELD_TYPE_ID 0xFF

#define DEFAULT_BUFFER_CAPACITY     16000

/* -------------------------------------------------------------------------- */
/*
 *	Client-server data overlapping structures and stuff
 */
/* -------------------------------------------------------------------------- */

typedef struct {
    int type;
    dx_record_field_setter_t setter;
    dx_record_field_def_val_getter def_val_getter;
} dx_field_digest_t, *dx_field_digest_ptr_t;

typedef struct {
    dx_field_digest_ptr_t* elements;
    int size;
    bool in_sync_with_server;
} dx_record_digest_t;

/* -------------------------------------------------------------------------- */
/*
 *	Parser connection context
 */
/* -------------------------------------------------------------------------- */

typedef struct {
    dx_byte_t* buffer;
    dx_int_t buffer_size;
    dx_int_t buffer_pos;
    dx_int_t buffer_capacity;
    
    dx_char_t symbol_buffer[SYMBOL_BUFFER_LEN + 1];
    dx_string_t symbol_result;

    dx_int_t lastCipher;
    dx_string_t lastSymbol;
    
    dx_record_server_support_info_t* record_server_support_states;
    dx_record_digest_t record_digests[dx_rid_count];
    
    void* bicc;
} dx_parser_connection_context_t;

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_parser) {
    dx_parser_connection_context_t* context = dx_calloc(1, sizeof(dx_parser_connection_context_t));
    
    if (context == NULL) {
        return false;
    }
    
    if ((context->record_server_support_states = dx_get_record_server_support_states(connection)) == NULL ||
        (context->bicc = dx_get_buffered_input_connection_context(connection)) == NULL) {
        dx_free(context);
        
        return false;
    }
    
    context->buffer = dx_malloc(DEFAULT_BUFFER_CAPACITY);
    
    if (context->buffer == NULL) {
        dx_free(context);
        
        return false;
    }
    
    context->buffer_capacity = DEFAULT_BUFFER_CAPACITY;
    
    if (!dx_set_subsystem_data(connection, dx_ccs_parser, context)) {
        dx_free(context->buffer);
        dx_free(context);
        
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

void dx_clear_record_digests (dx_parser_connection_context_t* context);

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_parser) {
    dx_parser_connection_context_t* context = (dx_parser_connection_context_t*)dx_get_subsystem_data(connection, dx_ccs_parser);
    
    if (context == NULL) {
        return true;
    }
    
    CHECKED_FREE(context->buffer);
    CHECKED_FREE(context->symbol_result);
    CHECKED_FREE(context->lastSymbol);
    
    dx_clear_record_digests(context);
    dx_free(context);
    
    return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Field digest functions
 */
/* -------------------------------------------------------------------------- */

dx_result_t dx_create_field_digest (dx_parser_connection_context_t* context,
                                    dx_record_id_t record_id, const dx_record_info_t* record_info,
                                    OUT dx_field_digest_ptr_t* field_digest) {
    static dx_const_string_t s_field_to_ignore = L"Symbol";

    dx_string_t field_name;
    dx_int_t field_type;
    int field_index = g_invalid_index;

    CHECKED_CALL_2(dx_read_utf_string, context->bicc, &field_name);
    CHECKED_CALL_2(dx_read_compact_int, context->bicc, &field_type);

    dx_logging_verbose_info(L"Field name: %s, field type: %d", field_name, field_type);

    if (dx_compare_strings(field_name, s_field_to_ignore) == 0) {
        return parseSuccessful();
    }

    if (field_name == NULL || dx_string_length(field_name) == 0 ||
        field_type < MIN_FIELD_TYPE_ID || field_type > MAX_FIELD_TYPE_ID) {

        dx_free(field_name);

        return setParseError(dx_pr_field_info_corrupt);
    }

    field_index = dx_find_record_field(record_info, field_name, field_type);

    dx_free(field_name);

    *field_digest = (dx_field_digest_ptr_t)dx_calloc(1, sizeof(dx_field_digest_t));

    if (field_digest == NULL) {
        return R_FAILED;
    }

    (*field_digest)->type = field_type;

    if (field_index != g_invalid_index) {
        (*field_digest)->setter = record_info->fields[field_index].setter;
        context->record_server_support_states[record_id].fields[field_index] = true;			    			    
    }
    
    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

dx_result_t dx_digest_unsupported_fields (dx_parser_connection_context_t* context,
                                          dx_record_id_t record_id, const dx_record_info_t* record_info) {
    dx_record_digest_t* record_digest = &(context->record_digests[record_id]);
    int field_index = 0;

    for (; field_index < record_info->field_count; ++field_index) {
        dx_field_digest_ptr_t field_digest = NULL;

        if (context->record_server_support_states[record_id].fields[field_index]) {
            /* the field is supported by server, skipping */

            continue;
        }

        field_digest = (dx_field_digest_ptr_t)dx_calloc(1, sizeof(dx_field_digest_t));

        if (field_digest == NULL) {
            return R_FAILED;
        }

        field_digest->setter = record_info->fields[field_index].setter;
        field_digest->def_val_getter = record_info->fields[field_index].def_val_getter;

        record_digest->elements[(record_digest->size)++] = field_digest;
    }

    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

void dx_clear_record_digests (dx_parser_connection_context_t* context) {
    dx_record_id_t record_id;
    
    for (record_id = dx_rid_begin; record_id < dx_rid_count; ++record_id) {
        int i = 0;

        for (; i < context->record_digests[record_id].size; ++i) {
            dx_free(context->record_digests[record_id].elements[i]);
        }

        if (context->record_digests[record_id].elements != NULL) {
            dx_free(context->record_digests[record_id].elements);
        }

        context->record_digests[record_id].elements = NULL;
        context->record_digests[record_id].size = 0;
    }
}

/* -------------------------------------------------------------------------- */

dx_result_t dx_clear_records_server_support_states (dxf_connection_t connection) {
    dx_record_id_t eid;
    dx_parser_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_parser);
    
    if (context == NULL) {
        return setParseError(dx_pr_connection_context_not_initialized);
    }

    /* stage 1 - resetting all the field flags */
    for (eid = dx_rid_begin; eid < dx_rid_count; ++eid) {
        int i = 0;

        for (; i < context->record_server_support_states[eid].field_count; ++i) {
            context->record_server_support_states[eid].fields[i] = false;
        }

        context->record_digests[eid].in_sync_with_server = false;
    }

    /* stage 2 - freeing all the memory allocated by previous synchronization */
    dx_clear_record_digests(context);
    
    return R_SUCCESSFUL;
}

/* -------------------------------------------------------------------------- */
/*
 *	Parser functions implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_is_message_type_valid (dx_long_t type) {
    return type == MESSAGE_HEARTBEAT
        || type == MESSAGE_DESCRIBE_PROTOCOL
        || type == MESSAGE_DESCRIBE_RECORDS
        || type == MESSAGE_DESCRIBE_RESERVED
        || type == MESSAGE_RAW_DATA
        || type == MESSAGE_TICKER_DATA
        || type == MESSAGE_TICKER_ADD_SUBSCRIPTION
        || type == MESSAGE_TICKER_REMOVE_SUBSCRIPTION
        || type == MESSAGE_STREAM_DATA
        || type == MESSAGE_STREAM_ADD_SUBSCRIPTION
        || type == MESSAGE_STREAM_REMOVE_SUBSCRIPTION
        || type == MESSAGE_HISTORY_DATA
        || type == MESSAGE_HISTORY_ADD_SUBSCRIPTION
        || type == MESSAGE_HISTORY_REMOVE_SUBSCRIPTION
        || type == MESSAGE_TEXT_FORMAT_COMMENT
        || type == MESSAGE_TEXT_FORMAT_SPECIAL;
}

/* -------------------------------------------------------------------------- */

bool dx_is_data_message(dx_message_type_t type) {
    return type == MESSAGE_RAW_DATA
        || type == MESSAGE_TICKER_DATA
        || type == MESSAGE_STREAM_DATA
        || type == MESSAGE_HISTORY_DATA;
}

/* -------------------------------------------------------------------------- */

bool dx_is_subscription_message(dx_message_type_t type) {
    return  type == MESSAGE_TICKER_ADD_SUBSCRIPTION
			|| type == MESSAGE_TICKER_REMOVE_SUBSCRIPTION
			|| type == MESSAGE_STREAM_ADD_SUBSCRIPTION
			|| type == MESSAGE_STREAM_REMOVE_SUBSCRIPTION
			|| type == MESSAGE_HISTORY_ADD_SUBSCRIPTION
			|| type == MESSAGE_HISTORY_REMOVE_SUBSCRIPTION;

}

/* -------------------------------------------------------------------------- */
/**
* Parses and return message type.
*/
dx_result_t dx_parse_type (dx_parser_connection_context_t* context, OUT dx_int_t* ltype) {
    dx_long_t type;

    if (dx_read_compact_long(context->bicc, &type) != R_SUCCESSFUL) {
        if (dx_get_parser_last_error() == dx_pr_out_of_buffer) {
            return setParseError(dx_pr_message_not_complete);
        } else {
            return setParseError(dx_pr_buffer_corrupt);
        }
    }

    if (!dx_is_message_type_valid(type)) {
        return setParseError(dx_pr_unexpected_message_type);
    }

    dx_logging_verbose_info(L"Parse type: %d", type);

    *ltype = (dx_int_t)(type);
    
    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */
/**
* Parses message length and sets up position and limit for parsing of the message contents.
* Returns false when message is not complete yet and its parsing cannot be started.
*/
static dx_result_t dx_parse_length_and_setup_input (dx_parser_connection_context_t* context, dx_int_t position, dx_int_t limit) {
    dx_long_t length;
	dx_int_t endPosition;

    if (dx_read_compact_long(context->bicc, &length) != R_SUCCESSFUL) {
        return setParseError(dx_pr_message_not_complete);
    }
    
    if (length < 0 || length > INT_MAX - dx_get_in_buffer_position(context->bicc)) {
        return setParseError(dx_pr_buffer_corrupt);
    }

    endPosition = dx_get_in_buffer_position(context->bicc) + (dx_int_t)length;
    
    if (endPosition > limit) {
        return setParseError(dx_pr_message_not_complete);
    }

    dx_logging_info(L"Length of message: %d", length);

    dx_set_in_buffer_limit(context->bicc, endPosition); // set limit for this message
    
    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

static dx_result_t dx_read_symbol (dx_parser_connection_context_t* context) {
    dx_int_t r;
    
    if (dx_codec_read_symbol(context->bicc, context->symbol_buffer, SYMBOL_BUFFER_LEN, &(context->symbol_result), &r) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    
    if ((r & dx_get_codec_valid_chipher()) != 0) {
        context->lastCipher = r;
        context->lastSymbol = NULL;
	} else if (r > 0) {
        context->lastCipher = 0;            
        context->lastSymbol = dx_create_string_src(context->symbol_buffer);
        
        if (context->lastSymbol == NULL) {
            return R_FAILED;
        }
	} else {
        if (context->symbol_result != NULL) {
            context->lastSymbol = dx_create_string_src(context->symbol_result);
            
            if (context->lastSymbol == NULL) {
                return R_FAILED;
            }

            dx_free(context->symbol_result);
            
            context->lastCipher = dx_encode_symbol_name(context->lastSymbol);
        }
        
        if (context->lastCipher == 0 && context->lastSymbol == NULL) {
            return setParseError(dx_pr_undefined_symbol);
        }
    }

    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

#define CHECKED_SET_VALUE(setter, buffer, value) \
    if (setter != NULL) { \
        setter(buffer, value); \
    }

dx_result_t dx_read_records (dxf_connection_t connection, dx_parser_connection_context_t* context,
                             dx_record_id_t record_id, void* record_buffer) {
	int i = 0;
	dx_int_t read_int;
	dx_char_t read_utf_char;
	dx_double_t read_double;
	dx_string_t read_string;
	const dx_record_digest_t* record_digest = &(context->record_digests[record_id]);
	
    dx_logging_info(L"Read records");

	for (; i < record_digest->size; ++i) {
		switch (record_digest->elements[i]->type) {
		case 0:
			/* 0 here means that we're dealing with the field the server does not support;
			    using the default value for it */
			    
			record_digest->elements[i]->setter(record_buffer, record_digest->elements[i]->def_val_getter());
			
			break;
		case dx_fid_compact_int:
			CHECKED_CALL_2(dx_read_compact_int, context->bicc, &read_int);
			CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_int);
			
			break;
		case dx_fid_utf_char:
			CHECKED_CALL_2(dx_read_utf_char, context->bicc, &read_int);
			
			read_utf_char = read_int;
			
			CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_int);
			
			break;
		case dx_fid_compact_int | dx_fid_flag_decimal: 
			CHECKED_CALL_2(dx_read_compact_int, context->bicc, &read_int);
			CHECKED_CALL_2(dx_int_to_double, read_int, &read_double);
			CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_double);
			
			break;
		case dx_fid_utf_char_array:
			CHECKED_CALL_2(dx_read_utf_string, context->bicc, &read_string);
			
			dx_store_string_buffer(connection, read_string);
			
			CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_string);
			
			break;
		case dx_fid_byte:
		case dx_fid_short:
		case dx_fid_int:
		case dx_fid_byte_array:
		case dx_fid_compact_int | dx_fid_flag_short_string:
		case dx_fid_byte_array | dx_fid_flag_string:
		case dx_fid_byte_array | dx_fid_flag_custom_object:
		case dx_fid_byte_array | dx_fid_flag_serial_object:
		default:
			return setParseError (dx_pr_type_not_supported);
		}
	}

    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

dx_result_t dx_parse_data (dxf_connection_t connection, dx_parser_connection_context_t* context) {
    dx_string_t symbol = NULL;
	
    void* record_buffer = NULL;
	dx_record_id_t record_id;
	int record_count = 0;
    
    dx_logging_info(L"Parse data");

	while (dx_get_in_buffer_position(context->bicc) < dx_get_in_buffer_limit(context->bicc)) {
		CHECKED_CALL(dx_read_symbol, context);
	    
		{
			dx_int_t id;
	        
			CHECKED_CALL_2(dx_read_compact_int, context->bicc, &id);

			if (id < 0 || id >= dx_rid_count) {
				return setParseError(dx_pr_wrong_record_id);
			}
	        
			record_id = dx_get_record_id(connection, id);
		}
	    
		if (!context->record_digests[record_id].in_sync_with_server) {
			return setParseError(dx_pr_record_description_not_received);
		}
	    

		record_buffer = g_buffer_managers[record_id].record_getter(connection, record_count++);
		
		if (record_buffer == NULL) {
		    dx_free_string_buffers(connection);
		    
		    return R_FAILED;
		}
		
		if (dx_read_records(connection, context, record_id, record_buffer) != R_SUCCESSFUL) {
			dx_free_string_buffers(connection);
			
			return setParseError(dx_pr_record_reading_failed);
		}
    }
	
	if (context->lastSymbol == NULL) {
		if (dx_decode_symbol_name(context->lastCipher, &symbol) != R_SUCCESSFUL) {
		    dx_free_string_buffers(connection);
		    
		    return R_FAILED;
		}
        
        dx_store_string_buffer(connection, symbol);
    }
	
	if (!dx_transcode_record_data(connection, record_id, symbol, context->lastCipher,
	                              g_buffer_managers[record_id].record_buffer_getter(connection), record_count)) {
	    dx_free_string_buffers(connection);
	    
	    return R_FAILED;
	}
	
	dx_free_string_buffers(connection);
	
	return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

dx_result_t dx_parse_describe_records (dxf_connection_t connection, dx_parser_connection_context_t* context) {
    dx_logging_info(L"Parse describe records");

    while (dx_get_in_buffer_position(context->bicc) < dx_get_in_buffer_limit(context->bicc)) {
        dx_int_t record_id;
        dx_string_t record_name;
        dx_int_t field_count;
        const dx_record_info_t* record_info = NULL;
        dx_record_digest_t* record_digest = NULL;
        int i = 0;
        dx_record_id_t rid;
        
        CHECKED_CALL_2(dx_read_compact_int, context->bicc, &record_id);
        CHECKED_CALL_2(dx_read_utf_string, context->bicc, &record_name);
        CHECKED_CALL_2(dx_read_compact_int, context->bicc, &field_count);
		
		if (record_id < 0 || record_name == NULL || dx_string_length(record_name) == 0 || field_count < 0) {
            dx_free(record_name);
            
            return setParseError(dx_pr_record_info_corrupt);
        }
        
        dx_logging_info(L"Record ID: %d, record name: %s, field count: %d", record_id, record_name, field_count);

        if (record_id >= dx_rid_count) {
            dx_free(record_name);

            return setParseError(dx_pr_wrong_record_id);
        }
        
        rid = dx_get_record_id_by_name(record_name);
        
        dx_free(record_name);
        
        if (rid == dx_rid_invalid) {
            return setParseError(dx_pr_unknown_record_name);
        }
        
        dx_assign_protocol_id(connection, rid, record_id);
        
        record_info = dx_get_record_by_id(rid);
        record_digest = &(context->record_digests[rid]);
        
        if (record_digest->elements != NULL) {
            return setParseError(dx_pr_internal_error);
        }
        
        /* a generous memory allocation, to allow the maximum possible amount of pointers to be stored,
           but the overhead is insignificant */
        if ((record_digest->elements = dx_calloc(field_count + record_info->field_count, sizeof(dx_field_digest_ptr_t))) == NULL) {
            return R_FAILED;
        }
        
        for (i = 0; i != field_count; ++i) {
            dx_field_digest_ptr_t field_digest = NULL;
            
            CHECKED_CALL_4(dx_create_field_digest, context, rid, record_info, &field_digest);
            
            if (field_digest != NULL) {
                record_digest->elements[(record_digest->size)++] = field_digest;
            }
        }
        
        CHECKED_CALL_3(dx_digest_unsupported_fields, context, rid, record_info);
        
        context->record_digests[rid].in_sync_with_server = true;
    }
	
	return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

void dx_process_other_message(dx_int_t type, dx_byte_t* new_buffer, dx_int_t size, dx_int_t buffer_position, dx_int_t len) {
    
}

////////////////////////////////////////////////////////////////////////////////
/**
* Parses message of the specified type. Some messages are processed immediately, but data and subscription messages
* are just parsed into buffers and pendingMessageType is set.
*/
dx_result_t dx_parse_message (dxf_connection_t connection, dx_parser_connection_context_t* context, dx_int_t type) {
    dx_message_type_t messageType = (dx_message_type_t)type;
    
    if (dx_is_message_type_valid(messageType)) {
        if (dx_is_data_message(messageType)) {
            if (dx_parse_data(connection, context) != R_SUCCESSFUL) {

                if (dx_get_parser_last_error() == dx_pr_out_of_buffer) {
                    return setParseError(dx_pr_message_not_complete);
                }
                return R_FAILED;
            }

            return parseSuccessful();

        } else if (dx_is_subscription_message(messageType)) {
            return setParseError(dx_pr_unexpected_message_type);
        }
    
        switch (type) {
            case MESSAGE_DESCRIBE_RECORDS:
                if (dx_parse_describe_records(connection, context) != R_SUCCESSFUL) {
                    if (dx_get_parser_last_error() == dx_pr_out_of_buffer) {
                        return setParseError(dx_pr_message_not_complete);
                    }
                    return R_FAILED;
                }
			    break;
            case MESSAGE_HEARTBEAT: // no break, falls through
            case MESSAGE_DESCRIBE_PROTOCOL:
            case MESSAGE_DESCRIBE_RESERVED:
            case MESSAGE_TEXT_FORMAT_COMMENT:
            case MESSAGE_TEXT_FORMAT_SPECIAL:
                // just ignore those messages without any processing
                return setParseError(dx_pr_message_type_not_supported);
            default:
                {
                    dx_int_t size;
                    dx_byte_t* new_buffer = dx_get_in_buffer(context->bicc, &size);
    //                dx_process_other_message(type, new_buffer, size, dx_get_in_buffer_position(), dx_get_in_buffer_limit() - dx_get_in_buffer_position());
                }
        }
	}
    else {
        return setParseError(dx_pr_unexpected_message_type);
    }

    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

dx_result_t dx_combine_buffers (dx_parser_connection_context_t* context,
                                const dx_byte_t* new_buffer, dx_int_t new_buffer_size) {
	if (context->buffer_size != context->buffer_pos) { // copy all unprocessed data to beginning of buffer
		if (dx_memcpy(context->buffer, context->buffer + context->buffer_pos, context->buffer_size) == NULL) {
			return R_FAILED;
		}

        context->buffer_size = context->buffer_size - context->buffer_pos;
		context->buffer_pos = 0;
	} else {
		context->buffer_pos = context->buffer_size = 0;
	}

	// we have to combine two buffers - new data and old unprocessed data (if present)
	// it's possible if logic message was divided into two network packets
    if (context->buffer_capacity < context->buffer_size + new_buffer_size) {
        dx_byte_t* larger_buffer;
        
        context->buffer_capacity = context->buffer_size + new_buffer_size;
        larger_buffer = dx_malloc(context->buffer_capacity);

        if (dx_memcpy(larger_buffer, context->buffer, context->buffer_size) == NULL) {
            return R_FAILED;
        }

        dx_logging_verbose_info(L"Main buffer reallocated");

        dx_free(context->buffer);
        
        context->buffer = larger_buffer;
    }
	
	if (dx_memcpy(context->buffer + context->buffer_size, new_buffer, new_buffer_size) == NULL) {
	    return R_FAILED;
	}

	context->buffer_size = context->buffer_size + new_buffer_size;

    dx_logging_verbose_info(L"Buffers combined, new size: %d", context->buffer_size);

	dx_set_in_buffer(context->bicc, context->buffer, context->buffer_size);
	dx_set_in_buffer_position(context->bicc, context->buffer_pos);
	 
	return parseSuccessful();
}

/* -------------------------------------------------------------------------- */
/*
 *	Main functions
 */
/* -------------------------------------------------------------------------- */

dx_result_t dx_parse (dxf_connection_t connection, const dx_byte_t* new_buffer, int new_buffer_size) {
    dx_result_t parse_result = R_SUCCESSFUL;
    dx_parser_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_parser);
    
    if (context == NULL) {
        return setParseError(dx_pr_connection_context_not_initialized);
    }

    dx_logging_gap();
    dx_logging_info(L"Begin parsing...");
    dx_logging_verbose_info(L"buffer length: %d", new_buffer_size);
	

    if (new_buffer_size <= 0) {
        return R_FAILED;
    }

	if (dx_combine_buffers(context, new_buffer, new_buffer_size) != R_SUCCESSFUL) {
		return R_FAILED;
	}

    dx_logging_verbose_info(L"buffer position: %d", dx_get_in_buffer_position(context->bicc));

    // Parsing loop
    while (dx_get_in_buffer_position(context->bicc) < context->buffer_size) {
        dx_int_t messageType = MESSAGE_HEARTBEAT; // zero-length messages are treated as just heartbeats
        const dx_int_t message_start_pos = dx_get_in_buffer_position(context->bicc);

        dx_logging_info(L"Parsing message...");
        dx_logging_verbose_info(L"buffer position: %d", message_start_pos);

        // read length of a message and prepare an input buffer
        if (dx_parse_length_and_setup_input(context, context->buffer_pos, context->buffer_size) != R_SUCCESSFUL) {
            if (dx_get_parser_last_error() == dx_pr_message_not_complete) {
                // message is incomplete
                // just reset position back to message start and stop parsing
                dx_set_in_buffer_position(context->bicc, message_start_pos);
                dx_logging_last_error();
                break;
            } else { // buffer is corrupt
                dx_set_in_buffer_position(context->bicc, new_buffer_size); // skip the whole buffer
                dx_logging_last_error();
                parse_result = R_FAILED;
                break;
            }
        }

        if (dx_get_in_buffer_limit(context->bicc) > dx_get_in_buffer_position(context->bicc)) { // only if not empty message, empty message is heartbeat ???
            if (dx_parse_type(context, &messageType) != R_SUCCESSFUL) {

                if (dx_get_parser_last_error() == dx_pr_message_not_complete) {
                    // message is incomplete
                    // just reset position back to message start and stop parsing
                    dx_set_in_buffer_position(context->bicc, message_start_pos);
                    dx_logging_last_error();
                    break;
                } else if (dx_get_parser_last_error() == dx_pr_unexpected_message_type) {
                    // skip this message
                    dx_set_in_buffer_position(context->bicc, dx_get_in_buffer_limit(context->bicc));
                    continue;
                } else {
                    //dx_set_in_buffer_position(dx_get_in_buffer_limit());
                    //dx_logging_last_error();
                    //continue; // cannot continue parsing this message on corrupted buffer, so go to the next one
                    dx_set_in_buffer_position(context->bicc, new_buffer_size); // skip the whole buffer
                    dx_logging_last_error();
                    parse_result = R_FAILED;
                    break;
                }
            }
	    }

        if (dx_parse_message(connection, context, messageType) != R_SUCCESSFUL) {
            dx_parser_result_t res = dx_get_parser_last_error();
            
            switch (res) {
                case dx_pr_message_not_complete:
                    // message is incomplete
                    // just reset position back to message start and stop parsing
                    dx_set_in_buffer_position(context->bicc, message_start_pos);
                    dx_logging_last_error();
                    break;
                //case dx_pr_record_info_corrupt:
                //case dx_pr_unknown_record_name:
                //case dx_pr_field_info_corrupt:
                case dx_pr_message_type_not_supported:
                    // skip the whole message
                    dx_set_in_buffer_position(context->bicc, dx_get_in_buffer_limit(context->bicc));
                    dx_logging_last_error();
                    continue;
                default:
                    // skip the whole message
                    //dx_set_in_buffer_position(dx_get_in_buffer_limit());
                    dx_set_in_buffer_position(context->bicc, new_buffer_size); // skip the whole buffer
                    dx_logging_last_error();
                    parse_result = R_FAILED;
                    break;
                    //continue;
            }
            //dx_set_in_buffer_position(buffer_pos);
            break;
        }

        dx_logging_info(L"Parsing message complete. Buffer position: %d", dx_get_in_buffer_position(context->bicc));
    }
	
	context->buffer_pos = dx_get_in_buffer_position(context->bicc);

    dx_logging_info(L"End parsing. Buffer position: %d. Result: %s", dx_get_in_buffer_position(context->bicc)
                                                                   , parse_result == R_SUCCESSFUL ? L"R_SUCCESSFUL" : L"R_FAILED");

	return parse_result;
}

/* -------------------------------------------------------------------------- */
/*
 *	Low level socket data receiver implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_socket_data_receiver (dxf_connection_t connection, const void* buffer, int buffer_size) {
    dx_logging_info(L"Data received: %d bytes", buffer_size);

    return (dx_parse(connection, buffer, buffer_size) == R_SUCCESSFUL);
}
