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

#define INITIAL_RECORDS_BUFFER_SIZE 1000
static dx_byte_t* records_buffer      = 0;
static dx_int_t   records_buffer_size  = 0;
static dx_int_t   records_buffer_position  = 0;

#define MIN_FIELD_TYPE_ID 0x00
#define MAX_FIELD_TYPE_ID 0xFF


//
///* -------------------------------------------------------------------------- */
//
bool dx_is_message_type_valid(enum dx_message_type_t type) {
   return type >= MESSAGE_HEARTBEAT && type <= MESSAGE_TEXT_FORMAT_COMMENT;
}
//
///* -------------------------------------------------------------------------- */
//
bool dx_is_data_message(enum dx_message_type_t type) {
    return type == MESSAGE_RAW_DATA
        || type == MESSAGE_TICKER_DATA
        || type == MESSAGE_STREAM_DATA
        || type == MESSAGE_HISTORY_DATA;
}

/* -------------------------------------------------------------------------- */

bool dx_is_subscription_message(enum dx_message_type_t type) {
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
enum dx_result_t dx_parse_type(OUT dx_int_t* ltype) {
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
static enum dx_result_t dx_parse_length_and_setup_input(dx_int_t position, dx_int_t limit ) {
    dx_long_t length;
	dx_int_t endPosition;

    if (dx_read_compact_long(&length) != R_SUCCESSFUL) {
        return R_FAILED; // need more bytes TODO: process this situation
    }
    if (length < 0 || length > INT_MAX - dx_get_in_buffer_position()) {
        return setParseError(dx_pr_buffer_corrupt);
    }

    endPosition = dx_get_in_buffer_position() + (dx_int_t)length;
    if (endPosition > limit)
        return setParseError(dx_pr_message_not_complete);

    dx_set_in_buffer_limit(endPosition); // set limit for this message
    return parseSuccessful();
}

static enum dx_result_t dx_read_symbol() {
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
            lastCipher = dx_encode(lastSymbol);
        }
        if (lastCipher == 0 && lastSymbol == NULL)
            return setParseError(dx_pr_undefined_symbol);
    }

    return parseSuccessful();
}

enum dx_result_t dx_enlarge_record_buffer(){
	//TODO:
}
#pragma pack(1) 
enum dx_result_t dx_read_records(dx_int_t id) {
	struct dx_record_info_t record;
	dx_int_t i = 0;
	dx_int_t read_int;
	dx_char_t read_utf_char;
	dx_double_t read_double;
	CHECKED_CALL_2(dx_get_record_by_id, id, &record);   

	for ( ; i < record.fields_count; ++i ){ // read records into records buffer
		switch (record.fields[i].id){ // polymorphism by hands :)
			case dx_fid_compact_int:
				if ( records_buffer_size - records_buffer_position < sizeof(read_int) ) CHECKED_CALL_0( dx_enlarge_record_buffer );// if there are not enough place in buffer. does anyone knows haw to make it better?
				CHECKED_CALL (dx_read_compact_int, &read_int);
				if ( dx_memcpy (records_buffer + records_buffer_position, &read_int, sizeof(read_int) ) == NULL ) return R_FAILED;
				records_buffer_position +=  sizeof(read_int);
				break;
			case dx_fid_utf_char:
				if ( records_buffer_size - records_buffer_position < sizeof(read_utf_char) ) CHECKED_CALL_0( dx_enlarge_record_buffer );// if there are not enough place in buffer. does anyone knows haw to make it better?										
				CHECKED_CALL (dx_read_utf_char, &read_int);
				read_utf_char = read_int;
				if (dx_memcpy( records_buffer + records_buffer_position, &read_utf_char, sizeof(read_utf_char) ) == NULL ) return R_FAILED;
				records_buffer_position +=  sizeof(read_utf_char);
				break;
			case dx_fid_compact_int | dx_fid_flag_decimal: 
				if ( records_buffer_size - records_buffer_position < sizeof(read_int) ) CHECKED_CALL_0( dx_enlarge_record_buffer );// if there are not enough place in buffer. does anyone knows haw to make it better?
				CHECKED_CALL (dx_read_compact_int, &read_int);
				CHECKED_CALL_2(dx_int_to_double, read_int, &read_double);
				if (dx_memcpy( records_buffer + records_buffer_position, &read_double, sizeof(read_double) ) == NULL ) return R_FAILED;
				records_buffer_position +=  sizeof(read_double);
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

struct dxf_quote{
	dx_char_t bid_exchange;
	dx_double_t bid_price;
	dx_int_t bid_size;
	dx_char_t ask_exchange;
	dx_double_t ask_price;
	dx_int_t ask_size;	
	dx_int_t bid_time;	
	dx_int_t ask_time;
};

enum dx_result_t dx_parse_data() {
    //lastCipher = 0;
    //lastSymbol = NULL;
    dx_int_t start_position = dx_get_in_buffer_position();
    dx_int_t last_rec_position = start_position;

    dx_int_t id;
	dx_int_t records_count = 0;

    CHECKED_CALL_0(dx_read_symbol);
    CHECKED_CALL(dx_read_compact_int, &id);
	if (lastSymbol != NULL) wprintf(L"Symbol: %s\n", lastSymbol);
	else wprintf(L"Symbol cipher: %i\n", lastCipher);
	wprintf(L"Record: %i \n",id);
    while (dx_get_in_buffer_position() < dx_get_in_buffer_limit()) {
		if ( dx_read_records ( id ) != R_SUCCESSFUL )
			return setParseError(dx_pr_record_reading_failed);
		++records_count;
        };
	{
		dx_int_t i = 0;
		struct dxf_quote* quotes = (struct dxf_quote*)records_buffer;
		for ( ; i < records_count ; ++i )
			wprintf(L"Bid.Exchange=%C Bid.Price=%f Bid.Size=%i Ask.Exchange=%C Ask.Price=%f Ask.Size=%i Bid.Time=%i Ask.Time=%i \n" , 
			quotes[i].bid_exchange,quotes[i].bid_price,quotes[i].bid_size,quotes[i].ask_exchange,quotes[i].ask_price,quotes[i].ask_size,quotes[i].bid_time,quotes[i].ask_time);
	
	records_buffer_position = 0; // mean we've called callback
	}

}

///* -------------------------------------------------------------------------- */
//
enum dx_result_t dx_parse_describe_records() {
    dx_int_t start_position = dx_get_in_buffer_position();
    dx_int_t last_rec_position = start_position;
        while (dx_get_in_buffer_position() < dx_get_in_buffer_limit()) {
            dx_int_t id;
            dx_string_t name;
            dx_int_t n;
            dx_string_t* fname;
            dx_int_t* ftype;
            dx_int_t i;

            CHECKED_CALL(dx_read_compact_int, &id);
            CHECKED_CALL(dx_read_utf_string, &name);
            CHECKED_CALL(dx_read_compact_int, &n);

            if (id < 0 || name == NULL || wcslen(name) == 0 || n < 0) {
                return setParseError(dx_pr_record_info_corrupt);
            }

            fname = (dx_string_t*)dx_malloc(n * sizeof(dx_string_t));
            ftype = (dx_int_t*)dx_malloc(n);
            for (i = 0; i < n; ++i) {
                CHECKED_CALL(dx_read_utf_string, &(fname[i]));
                CHECKED_CALL(dx_read_compact_int, &(ftype[i]))
                if (fname[i] == NULL || wcslen(fname[i]) == 0 || ftype[i] < MIN_FIELD_TYPE_ID || ftype[i] > MAX_FIELD_TYPE_ID) {
                    return setParseError(dx_pr_field_info_corrupt);
                }
            }

            //const struct dx_record_info_t* record = dx_get_record_by_name(name);
            ////DataRecord record = scheme.findRecordByName(name);
            //if (record != NULL) {
            //    //RecordReader rr = record;
            //    if (!dx_matching_fields(record, fname, ftype))
            //        rr = createRecordAdapter(id, record, fname, ftype);
            //    if (rr != null)
            //        remapRecord(id, rr); // silently remap
            //    else
            //        record = null; // log the error message (see below)
            //} else {
            //    // otherwise, attempt to create new record to process and skip incoming data
            //    record = createRecord(id, name, fname, ftype);
            //}
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
enum dx_result_t dx_parse_message(dx_int_t type) {
    enum dx_message_type_t messageType = (enum dx_message_type_t)type;
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

enum dx_result_t dx_combine_buffers( const dx_byte_t* new_buffer, dx_int_t new_buffer_size  ) {
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
enum dx_result_t dx_parse( const dx_byte_t* new_buffer, dx_int_t new_buffer_size  ) {
	if (records_buffer_size == 0){
		records_buffer_size = INITIAL_RECORDS_BUFFER_SIZE;
		records_buffer = dx_malloc(records_buffer_size);
		if (records_buffer == NULL )
			return R_FAILED;
		records_buffer_position = 0;
	}
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

		dx_parse_message(messageType);
    }
	buffer_pos = dx_get_in_buffer_position();
}
