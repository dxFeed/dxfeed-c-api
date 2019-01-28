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

#ifndef DXP_MESSAGE_DATA_H_INCLUDED
#define DXP_MESSAGE_DATA_H_INCLUDED

#include "PrimitiveTypes.h"
#include "DXTypes.h"

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

	MESSAGE_TICKER_DATA = 10,
	MESSAGE_TICKER_ADD_SUBSCRIPTION = 11,
	MESSAGE_TICKER_REMOVE_SUBSCRIPTION = 12,

	MESSAGE_STREAM_DATA = 15,
	MESSAGE_STREAM_ADD_SUBSCRIPTION = 16,
	MESSAGE_STREAM_REMOVE_SUBSCRIPTION = 17,

	MESSAGE_HISTORY_DATA = 20,
	MESSAGE_HISTORY_ADD_SUBSCRIPTION = 21,
	MESSAGE_HISTORY_REMOVE_SUBSCRIPTION = 22,

	/*
	*  These two values are reserved and can not be used as any
	*  message type. Point is that when serialized to a file
	*  values 61 and 35 corresponds to ASCII symbols '=' and '#',
	*  which are used in text data format. So, when we
	*  read these values in binary format it means that we are actually
	*  reading data not in binary, but in text format.
	*/
	MESSAGE_TEXT_FORMAT_COMMENT = 35, // '#'
	MESSAGE_TEXT_FORMAT_SPECIAL = 61, // '='
} dx_message_type_t;

/* -------------------------------------------------------------------------- */
/*
 *	Message type validation functions
 */
/* -------------------------------------------------------------------------- */

bool dx_is_message_type_valid (int type);
bool dx_is_data_message (int type);
bool dx_is_subscription_message (int type);

/* -------------------------------------------------------------------------- */
/*
 *	Message roster functions
 */
/* -------------------------------------------------------------------------- */

const int* dx_get_send_message_roster (void);
int dx_get_send_message_roster_size (void);
const int* dx_get_recv_message_roster (void);
int dx_get_recv_message_roster_size (void);

/* -------------------------------------------------------------------------- */
/*
 *	Miscellaneous message functions
 */
/* -------------------------------------------------------------------------- */

dxf_const_string_t dx_get_message_type_name (int type);

#endif /* DXP_MESSAGE_DATA_H_INCLUDED */