/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 2.0 (the "License"); you may not use this file except in compliance with
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

#ifndef DXP_MESSAGE_DATA_H_INCLUDED
#define DXP_MESSAGE_DATA_H_INCLUDED

#include "DXTypes.h"
#include "PrimitiveTypes.h"

/* -------------------------------------------------------------------------- */
/*
 *	Message types
 */
/* -------------------------------------------------------------------------- */

typedef enum {
	/*
	*	When adding a new message type, please modify the following message related objects:
			1) Bodies of functions 'dx_is_message_type_valid', 'dx_is_data_message' and 'dx_is_subscription_message'.
			Include the new message type in the corresponding list.
			2) Body of the 'dx_get_message_type_name' function. Add your message type name there.
			3) Send/receive message rosters. Include the new message type in the corresponding roster.
			4) Other functions your new message must be explicitly used in.
	*/

	MESSAGE_HEARTBEAT = 0,

	MESSAGE_DESCRIBE_PROTOCOL = 1,
	MESSAGE_DESCRIBE_RECORDS = 2,
	MESSAGE_DESCRIBE_RESERVED = 3,

	// Message parts are used to disassemble large messages into small parts so that they can be multiplexed
	// with other messages or message parts. Message parts are assembled on receiving side into original message.
	// Disassembled messages are considered "arrived" when all their parts are received and are ready for assembling.
	// Disassembled messages are assigned unique message sequence numbers which have no meaning themselves.
	// Particular sequence of disassembled message parts may be cancelled before being completely sent by sending
	// a part message consisting only of corresponding message sequence number (with empty message body part).
	// In such case the receiving party behaves as if it has never received any parts of the corresponding message.
	MESSAGE_PART = 4,  // compact long message sequence number, part of wrapped message body including length and id (in
					   // first part only)

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

	// This message is supported for versions of RMI protocol with channels
	MESSAGE_RMI_ADVERTISE_SERVICES = 49,  // byte[] + int RMIServiceId, int distance, int nInterNodes, byte[] + int
										  // EndpointIdNode, int nProps, byte[] keyProp, byte[] keyValue
	MESSAGE_RMI_DESCRIBE_SUBJECT = 50,	  // int id, byte[] subject
	MESSAGE_RMI_DESCRIBE_OPERATION = 51,  // int id, byte[] operation (e.g. "service_name.ping()void")
	MESSAGE_RMI_REQUEST = 52,  // int request_id, int RMIKind, [optional channel_id], int request_type, RMIRoute route,
							   // RMIServiceId target, int subject_id, int operation_id, Object[] parameters
	MESSAGE_RMI_CANCEL = 53,   // <<LEGACY>> int request_id, int flags
	MESSAGE_RMI_RESULT = 54,   // <<LEGACY>> int request_id, Object result, [optional route]
	MESSAGE_RMI_ERROR = 55,	   // <<LEGACY>> int request_id, int RMIExceptionType#getId(), byte[] causeMessage, byte[]
							   // serializedCause, [optional route]
	MESSAGE_RMI_RESPONSE =
		56,	 // int request_id, int RMIKind, [optional channel_id],  RMIRoute route, byte[] serializedResult

	MAX_SUPPORTED_MESSAGE_TYPE = 63,  // must fit into long mask

	/*
	 * This value is reserved and can not be used as any message type. Point is that when serialized to a file
	 * values 61 corresponds to ASCII symbol '=', which is used in text data format. So, when we read this values
	 * in binary format it means that we are actually reading data not in binary, but in text format.
	 */
	MESSAGE_TEXT_FORMAT = 0x3d,	 // 61 decimal, '=' char

	/**
	 * This value is reserved and can not be used as any message type. It is the second byte of zip file compression.
	 */
	MESSAGE_ZIP_COMPRESSION = 0x4b,	 // 75 decimal, 'K' char

	/**
	 * This value is reserved and can not be used as any message type. It is the second byte of gzip file compression.
	 */
	MESSAGE_GZIP_COMPRESSION = 0x8b,  // 139 decimal, '<' char
} dx_message_type_t;

/* -------------------------------------------------------------------------- */
/*
 *	Message type validation functions
 */
/* -------------------------------------------------------------------------- */

int dx_is_message_type_valid(int type);
int dx_is_data_message(int type);
int dx_is_subscription_message(int type);

/* -------------------------------------------------------------------------- */
/*
 *	Message roster functions
 */
/* -------------------------------------------------------------------------- */

const int* dx_get_send_message_roster(void);
int dx_get_send_message_roster_size(void);
const int* dx_get_recv_message_roster(void);
int dx_get_recv_message_roster_size(void);

/* -------------------------------------------------------------------------- */
/*
 *	Miscellaneous message functions
 */
/* -------------------------------------------------------------------------- */

dxf_const_string_t dx_get_message_type_name(int type);

#endif /* DXP_MESSAGE_DATA_H_INCLUDED */
