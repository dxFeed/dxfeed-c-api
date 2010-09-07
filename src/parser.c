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
#include "EventSubscription.h"
#include "EventRecordBuffers.h"


//////////////////////////////////////////////////////////////////////////////////
//// ========== Implementation Details ==========
//////////////////////////////////////////////////////////////////////////////////
//

// should be at least two times bigger max socket buffer size
#define INITIAL_BUFFER_SIZE 16000  
//#define BUFFER_TOO_BIG 64000
static dx_byte_t* buffer      = 0;
static dx_int_t   buffer_size  = 0;
static dx_int_t   buffer_pos   = 0;
//static dx_int_t   buffer_limit = 0;

#define SYMBOL_BUFFER_LEN 64
static dx_char_t   symbol_buffer[SYMBOL_BUFFER_LEN];
static dx_string_t symbol_result;

static dx_int_t    lastCipher;
static dx_string_t lastSymbol;

#define MIN_FIELD_TYPE_ID 0x00
#define MAX_FIELD_TYPE_ID 0xFF

/* -------------------------------------------------------------------------- */

bool dx_is_message_type_valid (dx_message_type_t type) {
   return type >= MESSAGE_HEARTBEAT && type <= MESSAGE_TEXT_FORMAT_COMMENT;
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
* @throws CorruptedException if stream is corrupted.
*/
dx_result_t dx_parse_type(OUT dx_int_t* ltype) {
    dx_long_t type;
    if (dx_read_compact_long(&type) != R_SUCCESSFUL) {
        return setParseError(dx_pr_buffer_corrupt);
    }
    if (*ltype < 0 || *ltype > INT_MAX) {
        return setParseError(dx_pr_buffer_corrupt); // stream is corrupted
    }

    *ltype = (dx_int_t)(type);
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
/**
* Parses message length and sets up position and limit for parsing of the message contents.
* Returns false when message is not complete yet and its parsing cannot be started.
* @throws CorruptedException if stream is corrupted.
*/
static dx_result_t dx_parse_length_and_setup_input(dx_int_t position, dx_int_t limit ) {
    dx_long_t length;
	dx_int_t endPosition;

    if (dx_read_compact_long(&length) != R_SUCCESSFUL) {
        return setParseError(dx_pr_message_not_complete);
    }
    if (length < 0 || length > INT_MAX - dx_get_in_buffer_position()) {
        return setParseError(dx_pr_buffer_corrupt);
    }

    endPosition = dx_get_in_buffer_position() + (dx_int_t)length;
    if (endPosition > limit) {
        return setParseError(dx_pr_message_not_complete);
    }

    dx_set_in_buffer_limit(endPosition); // set limit for this message
    return parseSuccessful();
}

static dx_result_t dx_read_symbol() {
    dx_int_t r;
    if (dx_codec_read_symbol(symbol_buffer, SYMBOL_BUFFER_LEN, &symbol_result, &r) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    if ((r & dx_get_codec_valid_chipher()) != 0) {
        lastCipher = r;
        lastSymbol = NULL;
	} else 
		if (r > 0) {
        lastCipher = 0;
        //if (symbolResolver == null || (lastSymbol = symbolResolver.getSymbol(symbol_buffer, 0, r)) == null)
        lastSymbol = dx_calloc(r + 1, sizeof(dx_char_t));
        wcscpy(lastSymbol, symbol_buffer);
		} else {
        if (symbol_result != NULL) {
            lastSymbol = dx_calloc(wcslen(symbol_result) + 1, sizeof(dx_char_t));
            wcscpy(lastSymbol, symbol_result);
            lastCipher = dx_encode_symbol_name(lastSymbol);
        }
        if (lastCipher == 0 && lastSymbol == NULL)
            return setParseError(dx_pr_undefined_symbol);
    }

    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_records (const dx_record_info_t* record_info, void* record_buffer) {
	size_t i = 0;
	dx_int_t read_int;
	dx_char_t read_utf_char;
	dx_double_t read_double;
	
	for (; i < record_info->field_count; ++i) {
		switch (record_info->fields[i].type) {
			case dx_fid_compact_int:
				CHECKED_CALL(dx_read_compact_int, &read_int);
				
				record_info->fields[i].setter(record_buffer, &read_int);
				
				break;
			case dx_fid_utf_char:
				CHECKED_CALL (dx_read_utf_char, &read_int);
				
				read_utf_char = read_int;
				record_info->fields[i].setter(record_buffer, &read_int);
				
				break;
			case dx_fid_compact_int | dx_fid_flag_decimal: 
				CHECKED_CALL (dx_read_compact_int, &read_int);
				CHECKED_CALL_2(dx_int_to_double, read_int, &read_double);
				
				record_info->fields[i].setter(record_buffer, &read_double);
				
				break;
			case dx_fid_byte:  
			case dx_fid_short:
			case dx_fid_int:  
			case dx_fid_byte_array: 
			case dx_fid_utf_char_array: 
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

dx_result_t dx_parse_data (void) {
    dx_string_t symbol;
	
	const dx_record_info_t* record_info;
	void* record_buffer = NULL;
	dx_event_id_t event_id;
	int record_count = 0;

    CHECKED_CALL_0(dx_read_symbol);
    
    {
        dx_int_t id;
        
        CHECKED_CALL(dx_read_compact_int, &id);

        if ((event_id = dx_get_event_id(id)) == dx_eid_invalid) {
            return setParseError(dx_pr_wrong_record_id);
        }
    }
    
    record_info = dx_get_event_record_by_id(event_id);

    while (dx_get_in_buffer_position() < dx_get_in_buffer_limit()) {
		record_buffer = g_buffer_managers[event_id].record_getter(record_count++);
		
		if (record_buffer == NULL) {
		    return R_FAILED;
		}
		
		if (dx_read_records(record_info, record_buffer) != R_SUCCESSFUL) {
			return setParseError(dx_pr_record_reading_failed);
		}
    }
	
	//todo: maybe move to another file?
	if (lastSymbol == NULL) {
		CHECKED_CALL_2 (dx_decode_symbol_name, lastCipher, &symbol); 
    }
	
	if (!dx_process_event_data(DX_EVENT_BIT_MASK(event_id), symbol, lastCipher, g_buffer_managers[event_id].record_buffer_getter(), record_count)) {
	    return R_FAILED;
	}
	
	return parseSuccessful();
}

///* -------------------------------------------------------------------------- */
//
dx_result_t dx_parse_describe_records() {
    while (dx_get_in_buffer_position() < dx_get_in_buffer_limit()) {
        dx_int_t record_id;
        dx_string_t record_name;
        dx_int_t field_count;
        dx_record_info_t* record_info = NULL;
        dx_int_t i;
        
        CHECKED_CALL(dx_read_compact_int, &record_id);
        CHECKED_CALL(dx_read_utf_string, &record_name);
        CHECKED_CALL(dx_read_compact_int, &field_count);
		
		if (record_id < 0 || record_name == NULL || wcslen(record_name) == 0 || field_count < 0) {
            return setParseError(dx_pr_record_info_corrupt);
        }
        
        if ((record_info = (dx_record_info_t*)dx_get_event_record_by_name(record_name)) == NULL) {
            return setParseError(dx_pr_unknown_record_name);
        }
        
        record_info->protocol_level_id = record_id;

        for (i = 0; i < field_count; ++i) {
            dx_string_t field_name;
            dx_int_t field_type;
            
            CHECKED_CALL(dx_read_utf_string, &field_name);
            CHECKED_CALL(dx_read_compact_int, &field_type)
            
            if (field_name == NULL || wcslen(field_name) == 0 ||
                field_type < MIN_FIELD_TYPE_ID || field_type > MAX_FIELD_TYPE_ID) {
                
                return setParseError(dx_pr_field_info_corrupt);
            }
			
			if (!dx_move_record_field(record_info, field_name, field_type, i)) {
			    return setParseError(dx_pr_unknown_record_field);
			}
        }
    }
	
	return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

void dx_process_other_message(dx_int_t type, dx_byte_t* new_buffer, dx_int_t size, dx_int_t buffer_position, dx_int_t len) {
    
}

////////////////////////////////////////////////////////////////////////////////
/**
* Parses message of the specified type. Some messages are processed immedetely, but data and subscription messages
* are just parsed into buffers and pendingMessageType is set.
*/
dx_result_t dx_parse_message(dx_int_t type) {
    dx_message_type_t messageType = (dx_message_type_t)type;
    if (dx_is_message_type_valid(messageType)) {
        if (dx_is_data_message(messageType)) {
            if (dx_parse_data() != R_SUCCESSFUL) {
                return R_FAILED;
            } return parseSuccessful();
        } else if (dx_is_subscription_message(messageType)) {
            //parseSubscription(type); TODO: error subscription message doesn't valid here
            return parseSuccessful();
        }
    
    switch (type) {
        case MESSAGE_DESCRIBE_RECORDS:
                  // todo: error handling is not atomic now -- partially parsed message is still being processed.
                dx_parse_describe_records();//TODO: errors 
			break;
            // falls through to ignore this message
        case MESSAGE_HEARTBEAT:
        case MESSAGE_DESCRIBE_PROTOCOL:
        case MESSAGE_DESCRIBE_RESERVED:
        case MESSAGE_TEXT_FORMAT_COMMENT:
        case MESSAGE_TEXT_FORMAT_SPECIAL:
            // just ignore those messages without any processing
            break;
        default:
            {
                dx_int_t size;
                dx_byte_t* new_buffer = dx_get_in_buffer(&size);
//                dx_process_other_message(type, new_buffer, size, dx_get_in_buffer_position(), dx_get_in_buffer_limit() - dx_get_in_buffer_position());
            }
    }
	}
    return parseSuccessful();

}

////////////////////////////////////////////////////////////////////////////////

dx_result_t dx_combine_buffers( const dx_byte_t* new_buffer, dx_int_t new_buffer_size  ) {
	if (buffer_size != buffer_pos ){ // copy all unprocessed data to beginning of buffer
		if ( dx_memcpy(buffer, buffer + buffer_pos, buffer_size) == NULL)
			return R_FAILED;
		buffer_pos = 0;
		buffer_size = buffer_size - buffer_pos;
	} else {
		buffer_pos = buffer_size = 0;
	}

	// we have to combine two buffers - new data and old unprocessed data (if present)
	// it's possible if logic message was diveded into two network packets
	if (dx_memcpy(buffer + buffer_size, new_buffer, new_buffer_size) == NULL )
			return R_FAILED;

	buffer_size = buffer_size + new_buffer_size;

	dx_set_in_buffer(buffer, buffer_size);
	dx_set_in_buffer_position(buffer_pos);
	 
	return parseSuccessful();
}
////////////////////////////////////////////////////////////////////////////////
// ========== external interface ==========
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_parse( const dx_byte_t* new_buffer, dx_int_t new_buffer_size  ) {
	if ( buffer == NULL ) {// allocate memory for all program lifetime
		buffer = dx_malloc(INITIAL_BUFFER_SIZE);
		if (buffer == NULL)
			return R_FAILED;
		buffer_size = 0; // size of data in buffer
		buffer_pos = 0; 
	}
	// TODO: skip wrong buffer 
	if (dx_combine_buffers (new_buffer, new_buffer_size) != R_SUCCESSFUL) 
		return R_FAILED;

	//TODO: heartbeat messages ???
	// Parsing loop
    while (dx_get_in_buffer_position() < buffer_size) {
        dx_int_t messageType = MESSAGE_HEARTBEAT; // zero-length messages are treated as just heartbeats

        if (dx_parse_length_and_setup_input(buffer_pos, buffer_size) != R_SUCCESSFUL) {
            if (dx_get_parser_last_error() == dx_pr_message_not_complete) {
                break; // message is incomplete -- just stop parsing
				// TODO: reset position back to message start
            }
        }
        if (dx_get_in_buffer_limit() > dx_get_in_buffer_position()) { // only if not empty message, empty message is heartbeat ???
            if (dx_parse_type(&messageType) != R_SUCCESSFUL) {
                return R_FAILED; // cannot continue parsing on corrupted stream TODO: buffer state
            }
	    }

		CHECKED_CALL(dx_parse_message, messageType);
    }
	buffer_pos = dx_get_in_buffer_position();
	
	return parseSuccessful();
}
