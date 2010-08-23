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

////////////////////////////////////////////////////////////////////////////////
// ========== Implementation Details ==========
////////////////////////////////////////////////////////////////////////////////

static dx_byte_t* buffer      = 0;
static dx_int_t   buffer_size  = 0;
static dx_int_t   buffer_pos   = 0;
static dx_int_t   buffer_limit = 0;

#define SYMBOL_BUFFER_LEN 64
static dx_char_t   symbol_buffer[SYMBOL_BUFFER_LEN];
static dx_string_t symbol_result;

static dx_message_type_t pendingMessageType;
static dx_int_t    lastCipher;
static dx_string_t lastSymbol;

////////////////////////////////////////////////////////////////////////////////
// Message types
////////////////////////////////////////////////////////////////////////////////
//enum dx_message_type_t {
//    MESSAGE_HEARTBEAT = 0,
//
//    MESSAGE_DESCRIBE_PROTOCOL = 1,
//    MESSAGE_DESCRIBE_RECORDS = 2,
//    MESSAGE_DESCRIBE_RESERVED = 3,
//
//    MESSAGE_RAW_DATA = 5,
//
//    MESSAGE_TICKER_DATA = 10,
//    MESSAGE_TICKER_ADD_SUBSCRIPTION = 11,
//    MESSAGE_TICKER_REMOVE_SUBSCRIPTION = 12,
//
//    MESSAGE_STREAM_DATA = 15,
//    MESSAGE_STREAM_ADD_SUBSCRIPTION = 16,
//    MESSAGE_STREAM_REMOVE_SUBSCRIPTION = 17,
//
//    MESSAGE_HISTORY_DATA = 20,
//    MESSAGE_HISTORY_ADD_SUBSCRIPTION = 21,
//    MESSAGE_HISTORY_REMOVE_SUBSCRIPTION = 22,
//
//    //MESSAGE_RMI_DESCRIBE_SUBJECT = 50, // int id, byte[] subject
//    //MESSAGE_RMI_DESCRIBE_OPERATION = 51, // int id, byte[] operation (e.g. "service_name.ping()void")
//    //MESSAGE_RMI_REQUEST = 52, // int request_id, int flags, int subject_id, int operation_id, Object[] parameters
//    //MESSAGE_RMI_CANCEL = 53, // int request_id, int flags
//    //MESSAGE_RMI_RESULT = 54, // int request_id, Object result
//    //MESSAGE_RMI_ERROR = 55 // int request_id, int RMIExceptionType#getId(), byte[] causeMessage, byte[] serializedCause
//
//    /*
//    * These two values are reserved and can not be used as any
//    * message type. Point is that when serialized to a file
//    * values 61 and 35 corresponds to ASCII symbols '=' and '#',
//    * which are used in text data format. So, when we
//    * read this values in binary format it means that we are actually
//    * reading data not in binary, but in text format.
//    */
//    MESSAGE_TEXT_FORMAT_SPECIAL = 61, // '='
//    MESSAGE_TEXT_FORMAT_COMMENT = 35, // '#'
//
//    MT_NULL = UINT_MAX
//};

enum dx_message_type_t {
    MESSAGE_HEARTBEAT = 0,

    MESSAGE_DESCRIBE_PROTOCOL = 1,
    MESSAGE_DESCRIBE_RECORDS = 2,
    MESSAGE_DESCRIBE_RESERVED = 3,

    MESSAGE_RAW_DATA = 4,

    MESSAGE_TICKER_DATA = 5,
    MESSAGE_TICKER_ADD_SUBSCRIPTION = 6,
    MESSAGE_TICKER_REMOVE_SUBSCRIPTION = 7,

    MESSAGE_STREAM_DATA = 8,
    MESSAGE_STREAM_ADD_SUBSCRIPTION = 9,
    MESSAGE_STREAM_REMOVE_SUBSCRIPTION = 10,

    MESSAGE_HISTORY_DATA = 11,
    MESSAGE_HISTORY_ADD_SUBSCRIPTION = 12,
    MESSAGE_HISTORY_REMOVE_SUBSCRIPTION = 13,

    //MESSAGE_RMI_DESCRIBE_SUBJECT = 50, // int id, byte[] subject
    //MESSAGE_RMI_DESCRIBE_OPERATION = 51, // int id, byte[] operation (e.g. "service_name.ping()void")
    //MESSAGE_RMI_REQUEST = 52, // int request_id, int flags, int subject_id, int operation_id, Object[] parameters
    //MESSAGE_RMI_CANCEL = 53, // int request_id, int flags
    //MESSAGE_RMI_RESULT = 54, // int request_id, Object result
    //MESSAGE_RMI_ERROR = 55 // int request_id, int RMIExceptionType#getId(), byte[] causeMessage, byte[] serializedCause

    /*
    * These two values are reserved and can not be used as any
    * message type. Point is that when serialized to a file
    * values 61 and 35 corresponds to ASCII symbols '=' and '#',
    * which are used in text data format. So, when we
    * read this values in binary format it means that we are actually
    * reading data not in binary, but in text format.
    */
    MESSAGE_TEXT_FORMAT_SPECIAL = 14, // '='
    MESSAGE_TEXT_FORMAT_COMMENT = 15, // '#'

    MT_NULL = UINT_MAX
};

/* -------------------------------------------------------------------------- */

bool dx_is_message_type_valid(dx_message_type_t type) {
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
    return type == MESSAGE_TICKER_ADD_SUBSCRIPTION
        type == MESSAGE_TICKER_REMOVE_SUBSCRIPTION
        type == MESSAGE_STREAM_ADD_SUBSCRIPTION
        type == MESSAGE_STREAM_REMOVE_SUBSCRIPTION
        type == MESSAGE_HISTORY_ADD_SUBSCRIPTION
        type == MESSAGE_HISTORY_REMOVE_SUBSCRIPTION;

}

/* -------------------------------------------------------------------------- */

/**
* Parses and return message type.
* @throws CorruptedException if stream is corrupted.
*/
static dx_result_t dx_parse_type(OUT dx_int_t* ltype) {
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
* Parses message length and sets up in.position and in.limit for parsing of the message contents.
* Returns false when message is not complete yet and its parsing cannot be started.
* @throws CorruptedException if stream is corrupted.
*/
static dx_result_t dx_parse_length_and_setup_input(dx_int_t position, dx_int_t limit, OUT ) {
    //dx_set_in_buffer(buffer, buffer_size);
    //dx_set_in_buffer_position(buffer_pos);

    dx_long_t length;
    if (dx_read_compact_long(&length) != R_SUCCESSFUL) {
        return R_FAILED; // need more bytes
    }
    if (length < 0 || length > INT_MAX - dx_get_in_buffer_position()) {
        return setParseError(dx_pr_buffer_corrupt);
    }

    dx_int_t endPosition = dx_get_in_buffer_position() + (dx_int_t)length;
    if (endPosition > limit)
        return setParseError(dx_pr_message_not_complete);

    dx_set_in_buffer_limit(endPosition); // set limit for this message
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
/**
* Processes pending message from buffers (if any) and resets pendingMessageType to null.
*/
static void processPending(MessageConsumer consumer) {
    if (pendingMessageType != MT_NULL) {
        processMessage(pendingMessageType, consumer);
        pendingMessageType = MT_NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////
/**
* Processses message from the recordBuffer or subscriptionBuffer into the consumer.
*/
static void processMessage(dx_message_type_t type, MessageConsumer consumer) {
    if (type.isData())
        processData(type, consumer);
    else if (type.isSubscription())
        processSubscription(type, consumer);
}

////////////////////////////////////////////////////////////////////////////////
static void processData(dx_message_type_t type, MessageConsumer consumer) {
    switch (type) {
        case MESSAGE_RAW_DATA:
            consumer.processTickerData(recordBuffer);
            recordBuffer.rewind();
            consumer.processStreamData(recordBuffer);
            recordBuffer.rewind();
            consumer.processHistoryData(recordBuffer);
            break;
        case MESSAGE_TICKER_DATA:
            consumer.processTickerData(recordBuffer);
            break;
        case MESSAGE_STREAM_DATA:
            consumer.processStreamData(recordBuffer);
            break;
        case MESSAGE_HISTORY_DATA:
            consumer.processHistoryData(recordBuffer);
            break;
    }
    if (recordBuffer.hasNext())
        QDLog.log.error("WARNING: Data was not proceessed for QTP message " + type);
    recordBuffer.clear();
}

////////////////////////////////////////////////////////////////////////////////
static void nextMessage(MessageConsumer consumer, dx_message_type_t messageType) {
    if (messageType == MESSAGE_HEARTBEAT || messageType == MESSAGE_DESCRIBE_RECORDS)
        return; // these message does not cause immediate processing
    if (pendingMessageType != MT_NULL && messageType != pendingMessageType)
        processPending(consumer);
    pendingMessageType = messageType;
}

static dx_result_t dx_read_symbol() {
    dx_int_t r;
    if (dx_codec_read_symbol(symbol_buffer, SYMBOL_BUFFER_LEN, &symbol_result, &r) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    if ((r & dx_get_codec_valid_chipher()) != 0) {
        lastCipher = r;
        lastSymbol = NULL;
    } else if (r > 0) {
        lastCipher = 0;
        //if (symbolResolver == null || (lastSymbol = symbolResolver.getSymbol(symbol_buffer, 0, r)) == null)
        lastSymbol = dx_calloc(r + 1, sizeof(dx_char_t));
        wcscpy(lastSymbol, symbol_buffer)

    } else {
        if (symbol_result != null) {
            lastSymbol = dx_calloc(wcslen(symbol_result) + 1, sizeof(dx_char_t));
            wcscpy(lastSymbol, symbol_result);
            lastCipher = dx_encode(lastSymbol);
        }
        if (lastCipher == 0 && lastSymbol == null)
            return setParseError(dx_pr_undefined_symbol);
    }

    return parseSuccessful();
}

dx_result_t dx_parse_data() {
    lastCipher = 0;
    lastSymbol = NULL;
    dx_int_t start_position = dx_get_in_buffer_position();
    dx_int_t last_rec_position = start_position;
    try {
        while (dx_get_in_buffer_position() < dx_get_in_buffer_limit()) {
            dx_read_symbol();
            dx_int_t id;
            if (dx_read_compact_int(&id) != R_SUCCESSFUL) {
                return R_FAILED;
            }

            dx_read_record(id);
            //RecordReader rr = getRecordReader(id);
            //if (rr == null)
            //    throw new IOException("Unknown record #" + id);
            //rr.readRecord(in, recordBuffer, lastCipher, lastSymbol);
            //last_rec_position = in.getPosition();
        }
    } catch (IOException e) {
        dumpParseDataErrorReport(e, start_position, last_rec_position);
        throw new CorruptedException(e);
    } catch (IllegalStateException e) {
        // Happens when visiting of previous record was not properly finished.
        // Happens when data schemes are incompatible or message is corrupted.
        dumpParseDataErrorReport(e, start_position, last_rec_position);
        throw new CorruptedException(e);
    }
}

/* -------------------------------------------------------------------------- */

enum dx_result_t parseDescribeRecords() {
    dx_int_t start_position = dx_get_in_buffer_position();
    dx_int_t last_rec_position = start_position;
    try {
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

            if (id < 0 || name == null || wcslen(name) == 0 || n < 0) {
                return setParseError(dx_pr_record_info_corrupt);
            }

            fname = dx_malloc(n * sizeof(dx_string_t));
            ftype = dx_malloc(n);
            for (i = 0; i < n; ++i) {
                CHECKED_CALL(dx_read_utf_string, &(fname[i]));
                CHECKED_CALL(dx_read_compact_int, &(ftype[i]))
                if (fname[i] == NULL || wcslen(fname[i]) == 0 || ftype[i] < MIN_FIELD_TYPE_ID || ftype[i] > MAX_FIELD_TYPE_ID) {
                    return setParseError(dx_pr_field_info_corrupt);
                }
            }

            DataRecord record = scheme.findRecordByName(name);
            if (record != null) {
                RecordReader rr = record;
                if (!matchingFields(record, fname, ftype))
                    rr = createRecordAdapter(id, record, fname, ftype);
                if (rr != null)
                    remapRecord(id, rr); // silently remap
                else
                    record = null; // log the error message (see below)
            } else {
                // otherwise, attempt to create new record to process and skip incoming data
                record = createRecord(id, name, fname, ftype);
                if (record != null) {
                    QDLog.log.info("Incoming record #" + id + " " + name +
                        " is not found in data scheme. Incoming data and subscription will be skipped.");
                    remapRecord(id, new RecordReaderSkipper(record));
                }
            }
            if (record == null)
                QDLog.log.info("Incoming record #" + id + " " + name +
                " cannot be found or created. Might not be able to parse incoming data.");
            last_rec_position = in.getPosition();
        }
    } catch (IOException e) {
        dumpParseDescribeRecordsErrorReport(e, start_position, last_rec_position);
        throw new CorruptedException(e);
    }
}

/* -------------------------------------------------------------------------- */

void dx_process_other_message(dx_int_t type, dx_byte_t* buf, dx_int_t size, dx_int_t buffer_position, dx_int_t len) {
    
}

////////////////////////////////////////////////////////////////////////////////
/**
* Parses message of the specified type. Some messages are processed immedetely, but data and subscription messages
* are just parsed into buffers and pendingMessageType is set.
*/
dx_result_t dx_parse_message(dx_int_t type, MessageConsumer consumer) {
    dx_message_type_t messageType = (dx_message_type_t)type;
    if (dx_is_message_type_valid(messageType)) {
        if (dx_is_data_message(messageType)) {
            if (dx_parse_data() != R_SUCCESSFUL) {
                return R_FAILED;
            }
            return parseSuccessful();
        } else if (dx_is_subscription_message(messageType)) {
            //parseSubscription(type);
            return parseSuccessful();
        }
    }
    switch (type) {
        case MESSAGE_DESCRIBE_RECORDS:
            if (parseDescribeRecords)
                // todo: error handling is not atomic now -- partially parsed message is still being processed.
                parseDescribeRecords();
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
                dx_byte_t* buf = dx_get_in_buffer(&size);
                dx_process_other_message(type, buf, size, dx_get_in_buffer_position(), dx_get_in_buffer_limit() - dx_get_in_buffer_position());
            }
    }
    return parseSuccessful();

}


////////////////////////////////////////////////////////////////////////////////
// ========== external interface ==========
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
int dx_parse( const dx_byte_t* buf, dx_int_t bufLen  ) {
    dx_set_in_buffer(buffer, buffer_size);
    dx_set_in_buffer_position(buffer_pos);

    // Parsing loop
    while (buffer_pos < buffer_limit) {
        dx_int_t messageType = MESSAGE_HEARTBEAT; // zero-length messages are treated as just heartbeats

        if (dx_parse_length_and_setup_input(buffer_pos, buffer_limit) != R_SUCCESSFUL) {
            if (dx_get_parser_last_error() == dx_pr_message_not_complete) {
                break; // message is incomplete -- just stop parsing
            }
        }
        if (dx_get_in_buffer_limit() > dx_get_in_buffer_position()) { // only if not empty message, empty message is heartbeat
            if (dx_parse_type(&messageType) != R_SUCCESSFUL) {
                return R_FAILED; // cannot continue parsing on corrupted stream
            }
        }

        // ???
        nextMessage(consumer, dx_message_type_t.findById(messageType));

        //try {
            //if (consumer instanceof MessageConsumerAdapter)
            //    symbolResolver = (MessageConsumerAdapter)consumer;
            dx_parse_message(messageType, consumer);
        //} catch (CorruptedException e) {
        //    consumer.handleCorruptedMessage(messageType);
        //} finally {
        //    symbolResolver = null;
        //}
        // mark as "processed" and continue to the next message
        processed = in.getLimit();
    }
    processPending(consumer);
}