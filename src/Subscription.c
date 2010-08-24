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
#include "parser.h"
#include "BufferedOutput.h"

static dx_byte_t* dx_buf = NULL;
static dx_int_t   dx_buf_len = 0;
static dx_int_t   dx_processed = 0;

static dx_int_t    dx_last_cipher = 0;
static dx_string_t dx_last_symbol = NULL;

static dx_int_t dx_message_body_start = 0;
static bool dx_history_add_subscription = false;

static const dx_int_t dx_default_threshold = 8000;

/* -------------------------------------------------------------------------- 
*
*  Implementation Details
*
/* -------------------------------------------------------------------------- */

bool hasCapacity() {
    return dx_get_out_buffer_position() < dx_default_threshold;
}

/* -------------------------------------------------------------------------- */

enum dx_result_t dx_compose_message_header(dx_int_t messageTypeId) {
    dx_last_cipher = 0;
    dx_last_symbol = NULL;

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

/* -------------------------------------------------------------------------- */

enum dx_result_t dx_compose_record(dx_record_info_t* record, dx_int_t cipher, dx_string_t symbol) {
    dx_int_t rid = record->id;

    //if (recordState != null && recordState[rid] == RecordState.NEW) {
    //    recordState[rid] = RecordState.SHALL_DESCRIBE;
    //    shallDescribeRecords = true;
    //}

    if (cipher == 0 && symbol == null) {
        return setParseError(dx_pr_illegal_argument);
    }

    if (cipher == lastCipher && (cipher != 0 || symbol.equals(lastSymbol)))
        dx_int_t buf_len;
        dx_byte_t* buf = dx_get_out_buffer(&buf_len);
        if (dx_codec_write_symbol(buf, buf_len, dx_get_out_buffer_position(), 0, NULL) != R_SUCCESSFUL) {
            return R_FAILED;
        }
    // Symbol is the same as previous one. We write special empty symbol instead.
    else {
        dx_int_t buf_len;
        dx_byte_t* buf = dx_get_out_buffer(&buf_len);
        if (dx_codec_write_symbol(buf, buf_len, dx_get_out_buffer_position(), lastCipher = cipher, lastSymbol = symbol) != R_SUCCESSFUL) {
            return R_FAILED;
        }
    }

    CHECKED_CALL(dx_write_compact_int, rid);

    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

bool dx_retrieve_subscription() {
    // here we need to get somehow: symbol, cipher, time
    dx_string_t symbol = NULL;
    dx_int_t    cipher = 0;
    dx_long_t   time = 0;

    // which record must be used here?
    dx_record_info_t some_record;

    try {
        if (dx_compose_record(some_record, cipher, symbol) != R_SUCCESSFUL) {
            //QDLog.log.error("Unexpected IO exception", e);
            //throw new AssertionError(e.toString());

        }
        if (dx_history_add_subscription) {
            composeHistorySubscriptionTime(record, time);
        }

    } catch (IOException e) {
        QDLog.log.error("Unexpected IO exception", e);
        throw new AssertionError(e.toString());
    }
    //global_lock.lock(collector.management, CollectorOperation.RETRIEVE_SUBSCRIPTION);
    //try {
    //    SubMatrix asub = this.sub;
    //    int aindex = queue_head;
    //    while (aindex > 0 && visitor.hasCapacity()) {
    //        if (asub.getInt(aindex + QUEUE_MARK) != 0) {
    //            int key = asub.getInt(aindex);
    //            int rid = asub.getInt(aindex + 1);
    //            int cipher = key;
    //            String symbol = null;
    //            if ((key & SymbolCodec.VALID_CIPHER) == 0) {
    //                if (key == 0)
    //                    throw new IllegalArgumentException("Undefined key.");
    //                cipher = 0;
    //                symbol = mapper.getSymbol(key);
    //            }
    //            long time = 0;
    //            if (has_time)
    //                time = sub.getLong(aindex + TIME_SUB);
    //            visitor.visitRecord(records[rid], cipher, symbol, time);

    //            asub.setInt(aindex + QUEUE_MARK, 0);
    //            asub.updateRemovedPayload(stats, rid);
    //            if (has_time)
    //                asub.setLong(aindex + TIME_SUB, 0);
    //        }
    //        int next = asub.getInt(aindex + QUEUE_NEXT);
    //        asub.setInt(aindex + QUEUE_NEXT, 0);
    //        aindex = next;

    //        // Adjust first/last here to protect from exception in visitor.
    //        queue_head = aindex;
    //        if (aindex < 0)
    //            queue_tail = -1;
    //    }
    //    if (asub.needRehash())
    //        rehash();
    //    return queue_head > 0;
    //} finally {
    //    global_lock.unlock();
    //}
}

/* -------------------------------------------------------------------------- */

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
    memcpy(dx_get_out_buffer(NULL) + newPos, dx_get_out_buffer(NULL) + oldPos, length);
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

   // ????
    // Describe records if necessary
    //if (shallDescribeRecords) {
    //    shallDescribeRecords = false;
    //    describeRecordsBeforeMessage(messageStart);
    //}
}

/* -------------------------------------------------------------------------- */

/**
* Invoked every time after processing all message data.
*/
enum dx_result_t dx_end_message() {
    dx_int_t message_start = dx_processed;
    if (dx_message_body_start == dx_get_out_buffer_position()) {
        // Collapse empty messages
        dx_set_out_buffer_position(message_start);
        return parseSuccessful();
    }
    try {
        if (dx_finish_composing_message(message_start) != R_SUCCESSFUL) {
            //QDLog.log.error("Unexpected IO exception", e);
            //throw new AssertionError(e.toString());
            return setParseError(dx_pr_unexpected_io_error);
        }
        dx_processed = dx_get_out_buffer_position();
}

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- 
*
*  External interface
*
/* -------------------------------------------------------------------------- */

enum dx_result_t dx_create_subscription(dx_byte_t* out, dx_int_t out_len, dx_message_type_t type) {
    dx_buf = out;
    dx_buf_len = out_len;

    dx_set_out_buffer(dx_buf, dx_buf_len);

    if (!dx_is_subscription_message(type)) {
        return setParseError(dx_pr_illegal_argument);
    }
    if (!hasCapacity()) {
        return parseSuccessful();
    }

    dx_begin_message(type);
    dx_history_add_subscription = (type == MESSAGE_HISTORY_ADD_SUBSCRIPTION);
    bool remained = dx_retrieve_subscription();
    //bool remained = provider.retrieveSubscription(this);
    dx_end_message();
    return remained;
}
