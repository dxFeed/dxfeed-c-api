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
#include "SymbolCodec.h"
#include "DXMemory.h"

static dx_byte_t* dx_buf = NULL;
static dx_int_t   dx_buf_len = 0;
static dx_int_t   dx_processed = 0;
static dx_int_t dx_message_body_start = 0;

static const dx_int_t dx_default_threshold = 8000;//TODO: why 8000? what does it mean?

/* -------------------------------------------------------------------------- 
*
*  Implementation Details
*
/* -------------------------------------------------------------------------- */

bool hasCapacity() {
    return dx_get_out_buffer_position() < dx_default_threshold;
}

///* -------------------------------------------------------------------------- */
//
enum dx_result_t dx_compose_message_header(dx_int_t messageTypeId) {

    CHECKED_CALL(dx_write_byte, (dx_byte_t)0); // reserve one byte for message length
    CHECKED_CALL(dx_write_compact_int, messageTypeId);

    return parseSuccessful();
}


/* -------------------------------------------------------------------------- */
/**
* Invoked every time before processing a message.
* @param messageTypeId id of message type.
*/
enum dx_result_t dx_begin_message(dx_int_t messageTypeId) {
    if (dx_processed != dx_get_out_buffer_position()) {
        dx_set_out_buffer_position(dx_processed);
   }

    if(dx_compose_message_header(messageTypeId) != R_SUCCESSFUL) {
        return setParseError(dx_pr_unexpected_io_error);
    }

    dx_message_body_start = dx_get_out_buffer_position();

    return parseSuccessful();
}

///* -------------------------------------------------------------------------- */
//
enum dx_result_t dx_compose_body(dx_int_t record_id, dx_int_t chiper, dx_string_t symbol) {

	dx_int_t buf_len;
    dx_byte_t* buf = dx_get_out_buffer(&buf_len);

    if (chiper == 0 && symbol == NULL) {
        return setParseError(dx_pr_illegal_argument);
    }

	if (dx_codec_write_symbol(buf, buf_len, dx_get_out_buffer_position(), chiper, symbol) != R_SUCCESSFUL) {
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

enum dx_result_t dx_finish_composing_message(dx_int_t messageStart) {
    dx_int_t messageLength = dx_get_out_buffer_position() - (messageStart + 1);
    dx_int_t sizeLength = dx_get_compact_length(messageLength);
    if (sizeLength > 1) {
        // only 1 byte was initially reserved. Shift as needed
        dx_move_data_forward(messageStart + 1, messageStart + sizeLength, messageLength);
    }

   dx_set_out_buffer_position(messageStart);
   CHECKED_CALL(dx_write_compact_int, messageLength);
   dx_set_out_buffer_position(messageStart + sizeLength + messageLength);

   return R_SUCCESSFUL;
}


/* -------------------------------------------------------------------------- */

/**
* Invoked every time after processing all message data.
*/
enum dx_result_t dx_end_message() {
    dx_int_t message_start = dx_processed;
    if (dx_message_body_start == dx_get_out_buffer_position()) { // Collapse empty messages
        dx_set_out_buffer_position(message_start);
        return parseSuccessful();
    }
    if (dx_finish_composing_message(message_start) != R_SUCCESSFUL) {
            return setParseError(dx_pr_unexpected_io_error);
        }
    dx_processed = dx_get_out_buffer_position();

	return parseSuccessful();
}

///* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- 
*
*  External interface
*
/* -------------------------------------------------------------------------- */

enum dx_result_t dx_create_subscription(dx_byte_t* out, dx_int_t out_len, enum dx_message_type_t type, dx_int_t chiper, dx_string_t symbol, dx_int_t record_id) {
    dx_buf = out;
    dx_buf_len = out_len;

    dx_set_out_buffer(dx_buf, dx_buf_len);

    if (!hasCapacity()) {
        return setParseError(dx_pr_illegal_length); 
    }    

	if (!dx_is_subscription_message(type)) {
        return setParseError(dx_pr_illegal_argument);
    }

    CHECKED_CALL(dx_begin_message, type);
    
	CHECKED_CALL_3(dx_compose_body, record_id, chiper, symbol);
	
	CHECKED_CALL_0(dx_end_message);

	return parseSuccessful();
}
