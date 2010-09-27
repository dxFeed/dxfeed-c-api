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

#include "Subscription.h"
#include "BufferedOutput.h"
#include "DataStructures.h"
#include "DXMemory.h"
#include "SymbolCodec.h"

static dx_byte_t* dx_buf = NULL;

static const dx_int_t dx_initial_buffer_size = 100; // It has to be enough to any subscription

/* -------------------------------------------------------------------------- 
*
*  Implementation Details
*
/* -------------------------------------------------------------------------- */

dx_result_t dx_compose_message_header(dx_int_t messageTypeId) {

    CHECKED_CALL(dx_write_byte, (dx_byte_t)0); // reserve one byte for message length
    CHECKED_CALL(dx_write_compact_int, messageTypeId);

    return parseSuccessful();
}


/* -------------------------------------------------------------------------- */
/**
* Invoked every time before processing a message.
* @param messageTypeId id of message type.
*/
dx_result_t dx_begin_message(dx_int_t messageTypeId) {

    if(dx_compose_message_header(messageTypeId) != R_SUCCESSFUL) {
        return setParseError(dx_pr_unexpected_io_error);
    }

    return parseSuccessful();
}

///* -------------------------------------------------------------------------- */
//
dx_result_t dx_compose_body(dx_int_t record_id, dx_int_t cipher, dx_const_string_t symbol) {

	dx_int_t buf_len;
    dx_byte_t* buf = dx_get_out_buffer(&buf_len);

    if (cipher == 0 && symbol == NULL) {
        return setParseError(dx_pr_illegal_argument);
    }

	if (dx_codec_write_symbol(buf, buf_len, dx_get_out_buffer_position(), cipher, symbol) != R_SUCCESSFUL) {
         return R_FAILED;
    }

    CHECKED_CALL(dx_write_compact_int, record_id);

    return parseSuccessful();
}

///* -------------------------------------------------------------------------- */

/**
* Accurately moves block of data of length in
* out buffer form position oldPos
* into position newPos.
* @param oldPos position to move from.
* @param newPos position to move into.
* @param length length of data block.
*/
void dx_move_data_forward(dx_int_t oldPos, dx_int_t newPos, dx_int_t length) {
    dx_ensure_capacity(newPos + length);
    dx_memcpy(dx_get_out_buffer(NULL) + newPos, dx_get_out_buffer(NULL) + oldPos, length);
}

/* -------------------------------------------------------------------------- */

dx_result_t dx_finish_composing_message() {
    dx_int_t messageLength = dx_get_out_buffer_position() - 1;
    dx_int_t sizeLength = dx_get_compact_length(messageLength);
    if (sizeLength > 1) {
        // only 1 byte was initially reserved. Shift as needed
        dx_move_data_forward(1, sizeLength, messageLength);
    }

   dx_set_out_buffer_position(0);
   CHECKED_CALL(dx_write_compact_int, messageLength);
   dx_set_out_buffer_position(sizeLength + messageLength);

   return R_SUCCESSFUL;
}


/* -------------------------------------------------------------------------- */

/**
* Invoked every time after processing all message data.
*/
dx_result_t dx_end_message() {
    if (dx_finish_composing_message() != R_SUCCESSFUL) {
            return setParseError(dx_pr_unexpected_io_error);
        }
	return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

dx_result_t dx_write_record_field (const dx_field_info_t* field) {
    CHECKED_CALL(dx_write_utf_string, field->name);
    CHECKED_CALL(dx_write_compact_int, field->type);
    
    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

dx_result_t dx_write_event_record (const dx_record_info_t* record, dx_int_t record_id) {
    int field_index = 0;
    
    CHECKED_CALL(dx_write_compact_int, record_id);
    CHECKED_CALL(dx_write_utf_string, record->name);
    CHECKED_CALL(dx_write_compact_int, ((dx_int_t)record->field_count));
    
    for (; field_index != record->field_count; ++field_index) {
        CHECKED_CALL(dx_write_record_field, record->fields + field_index);
    }
    
    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

dx_result_t dx_write_event_records (void) {
    dx_record_id_t eid = dx_rid_begin;
    
    for (; eid < dx_rid_count; ++eid) {
        CHECKED_CALL_2(dx_write_event_record, dx_get_record_by_id(eid), eid);
    }
    
    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */
/*
 *	External interface
 */
/* -------------------------------------------------------------------------- */

dx_result_t dx_create_subscription( OUT dx_byte_t** out, OUT dx_int_t* out_len, dx_message_type_t type, dx_int_t cipher, dx_const_string_t symbol, dx_int_t record_id ) {

	dx_buf = (dx_byte_t*)dx_malloc(dx_initial_buffer_size);

	*out = dx_buf;

    dx_set_out_buffer(dx_buf, dx_initial_buffer_size);

	if (!dx_is_subscription_message(type)) {
        return setParseError(dx_pr_illegal_argument);
    }

    if (dx_begin_message(type) != R_SUCCESSFUL) {
        return setParseError(dx_pr_illegal_argument);//TODO: error type
    }

    if (dx_compose_body(record_id, cipher, symbol) != R_SUCCESSFUL) {
        return setParseError(dx_pr_illegal_argument);//TODO: error type
    }
	
	if (type == MESSAGE_HISTORY_ADD_SUBSCRIPTION )
		CHECKED_CALL (dx_write_compact_int , 0); // hardcoded, used only once while subscribing to MarketMaker

    if (dx_end_message()) {
        return setParseError(dx_pr_illegal_argument);//TODO: error type
    }

	*out_len = dx_get_out_buffer_position();

    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

dx_result_t dx_compose_description_message (OUT dx_byte_t** msg_buffer, OUT dx_int_t* msg_length) {
    static const int initial_size = 1024;
    dx_byte_t* initial_buffer = NULL;
    
    if ((initial_buffer = dx_malloc(initial_size)) == NULL) {
        return R_FAILED;
    }
    
    dx_set_out_buffer(initial_buffer, (dx_int_t)initial_size);
    
    CHECKED_CALL(dx_compose_message_header, MESSAGE_DESCRIBE_RECORDS);
    CHECKED_CALL_0(dx_write_event_records);
    CHECKED_CALL_0(dx_finish_composing_message);
    
    *msg_buffer = dx_get_out_buffer(NULL);
    *msg_length = dx_get_out_buffer_position();
    
    return parseSuccessful();
}
