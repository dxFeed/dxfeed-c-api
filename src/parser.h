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

#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED

#include "PrimitiveTypes.h"
#include "DXTypes.h"
#include "ParserCommon.h"

/* -------------------------------------------------------------------------- */
/*
 *	Server synchronization functions
 */
/* -------------------------------------------------------------------------- */

dx_result_t dx_clear_records_server_support_states (dxf_connection_t connection);

/* -------------------------------------------------------------------------- 
/*
 *  Message types
 */
/* -------------------------------------------------------------------------- */

typedef enum {
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
    MESSAGE_TEXT_FORMAT_COMMENT = 35, // '#'
    MESSAGE_TEXT_FORMAT_SPECIAL = 61, // '='

} dx_message_type_t;

/* -------------------------------------------------------------------------- */

bool dx_is_subscription_message (dx_message_type_t type);

/* -------------------------------------------------------------------------- */
/*
 *	Low level network data receiver
 */
/* -------------------------------------------------------------------------- */

bool dx_socket_data_receiver (dxf_connection_t connection, const void* buffer, int buffer_size);

#endif /* PARSER_H_INCLUDED */