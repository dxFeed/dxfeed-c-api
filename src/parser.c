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

jByte* buffer      = 0;
jInt   bufferSize  = 0;
jInt   bufferPos   = 0;
jInt   bufferLimit = 0;

MessageType pendingMessageType;

////////////////////////////////////////////////////////////////////////////////
// Message types
////////////////////////////////////////////////////////////////////////////////
enum MessageType {
    MESSAGE_HEARTBEAT = 0,

    MESSAGE_DESCRIBE_PROTOCOL = 1,
    MESSAGE_DESCRIBE_RECORDS = 2,
    MESSAGE_DESCRIBE_RESERVED = 3,

    MESSAGE_RAW_DATA = 5,

    MESSAGE_TICKER_DATA = 10,
    MESSAGE_TICKER_ADD_SUBSCRIPTION = 11,
    MESSAGE_TICKER_REMOVE_SUBSCRIPTION = 12,

    MESSAGE_STREAM_DATA = 15,
    MESSAGE_STREAM_ADD_SUBSCRIPTION = 16,
    MESSAGE_STREAM_REMOVE_SUBSCRIPTION = 17,

    MESSAGE_HISTORY_DATA = 20,
    MESSAGE_HISTORY_ADD_SUBSCRIPTION = 21,
    MESSAGE_HISTORY_REMOVE_SUBSCRIPTION = 22,

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
    MESSAGE_TEXT_FORMAT_SPECIAL = 61, // '='
    MESSAGE_TEXT_FORMAT_COMMENT = 35, // '#'

    MT_NULL = UINT_MAX
};


////////////////////////////////////////////////////////////////////////////////
/**
* Parses and return message type.
* @throws CorruptedException if stream is corrupted.
*/
dx_result_t parseType(OUT jInt* ltype) {
    jLong type;
    if (readCompactLong(&type) != R_SUCCESSFUL) {
        return setParseError(pr_buffer_corrupt);
    }
    if (*ltype < 0 || *ltype > INT_MAX) {
        return setParseError(pr_buffer_corrupt); // stream is corrupted
    }

    *ltype = (jInt)(type);
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
/**
* Parses message length and sets up in.position and in.limit for parsing of the message contents.
* Returns false when message is not complete yet and its parsing cannot be started.
* @throws CorruptedException if stream is corrupted.
*/
dx_result_t parseLengthAndSetupInput(jInt position, jInt limit, OUT ) {
    setInBuffer(buffer, bufferSize);
    setInBufferPosition(bufferPos);

    jLong length;
    if (readCompactLong(&length) != R_SUCCESSFUL) {
        return R_FAILED; // need more bytes
    }
    if (length < 0 || length > INT_MAX - getInBufferPosition()) {
        return setParseError(pr_buffer_corrupt);
    }

    jInt endPosition = getInBufferPosition() + (jInt)length;
    if (endPosition > limit)
        return setParseError(pr_message_not_complete);

    setInBufferLimit(endPosition); // set limit for this message
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
/**
* Processes pending message from buffers (if any) and resets pendingMessageType to null.
*/
protected void processPending(MessageConsumer consumer) {
    if (pendingMessageType != MT_NULL) {
        processMessage(pendingMessageType, consumer);
        pendingMessageType = MT_NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////
/**
* Processses message from the recordBuffer or subscriptionBuffer into the consumer.
*/
protected void processMessage(MessageType type, MessageConsumer consumer) {
    if (type.isData())
        processData(type, consumer);
    else if (type.isSubscription())
        processSubscription(type, consumer);
}

////////////////////////////////////////////////////////////////////////////////
private void processData(MessageType type, MessageConsumer consumer) {
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
void nextMessage(MessageConsumer consumer, MessageType messageType) {
    if (messageType == MESSAGE_HEARTBEAT || messageType == MESSAGE_DESCRIBE_RECORDS)
        return; // these message does not cause immediate processing
    if (pendingMessageType != MT_NULL && messageType != pendingMessageType)
        processPending(consumer);
    pendingMessageType = messageType;
}

dx_result_t parseData() {
    lastCipher = 0;
    lastSymbol = null;
    jInt start_position = getInBufferPosition();
    jInt last_rec_position = start_position;
    try {
        while (getInBufferPosition() < getInBufferLimit()) {
            readSymbol();
            int id = readCompactInt();
            RecordReader rr = getRecordReader(id);
            if (rr == null)
                throw new IOException("Unknown record #" + id);
            rr.readRecord(in, recordBuffer, lastCipher, lastSymbol);
            last_rec_position = in.getPosition();
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

////////////////////////////////////////////////////////////////////////////////
/**
* Parses message of the specified type. Some messages are processed immedetely, but data and subscription messages
* are just parsed into buffers and pendingMessageType is set.
*/
dx_result_t parseMessage(int type, MessageConsumer consumer) {
    MessageType messageType = MessageType.findById(type);
    if (messageType != null) {
        if (messageType.isData()) {
            parseData();
            return;
        } else if (messageType.isSubscription()) {
            parseSubscription(type);
            return;
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
            consumer.processOtherMessage(type, in.getBuffer(), in.getPosition(), in.getLimit() - in.getPosition());
    }
}


////////////////////////////////////////////////////////////////////////////////
// ========== external interface ==========
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
int dx_parse( const jByte* buf, jInt bufLen  ) {
    // Parsing loop
    while (bufferPos < bufferLimit) {
        jInt messageType = MESSAGE_HEARTBEAT; // zero-length messages are treated as just heartbeats

        if (parseLengthAndSetupInput(bufferPos, bufferLimit) != R_SUCCESSFUL) {
            if (getParserLastError() == pr_message_not_complete) {
                break; // message is incomplete -- just stop parsing
            }
        }
        if (getInBufferLimit() > getInBufferPosition()) { // only if not empty message, empty message is heartbeat
            if (parseType(&messageType) != R_SUCCESSFUL) {
                return R_FAILED; // cannot continue parsing on corrupted stream
            }
        }

        nextMessage(consumer, MessageType.findById(messageType));
        try {
            if (consumer instanceof MessageConsumerAdapter)
                symbolResolver = (MessageConsumerAdapter)consumer;
            parseMessage(messageType, consumer);
        } catch (CorruptedException e) {
            consumer.handleCorruptedMessage(messageType);
        } finally {
            symbolResolver = null;
        }
        // mark as "processed" and continue to the next message
        processed = in.getLimit();
    }
    processPending(consumer);
}