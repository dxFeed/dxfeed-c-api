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

#include "DXPMessageData.h"
#include "DXErrorHandling.h"
#include "DXErrorCodes.h"
#include "DXAlgorithms.h"

/* -------------------------------------------------------------------------- */
/*
 *	Message roster structures
 */
/* -------------------------------------------------------------------------- */

static const int g_send_msg_roster[] = {
	/* the messages our application is able and is going to send */

	MESSAGE_TICKER_ADD_SUBSCRIPTION,
	MESSAGE_TICKER_REMOVE_SUBSCRIPTION,

	MESSAGE_STREAM_ADD_SUBSCRIPTION,
	MESSAGE_STREAM_REMOVE_SUBSCRIPTION,

	MESSAGE_HISTORY_ADD_SUBSCRIPTION,
	MESSAGE_HISTORY_REMOVE_SUBSCRIPTION
};

static const int g_recv_msg_roster[] = {
	/* the messages our application is able and is going to send */

	MESSAGE_TICKER_DATA,

	MESSAGE_STREAM_DATA,

	MESSAGE_HISTORY_DATA
};

static const int g_send_msg_count = sizeof(g_send_msg_roster) / sizeof(g_send_msg_roster[0]);
static const int g_recv_msg_count = sizeof(g_recv_msg_roster) / sizeof(g_recv_msg_roster[0]);

/* -------------------------------------------------------------------------- */
/*
 *	Message type validation functions implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_is_message_type_valid (int type) {
	return type == MESSAGE_HEARTBEAT ||
		type == MESSAGE_DESCRIBE_PROTOCOL ||
		type == MESSAGE_DESCRIBE_RECORDS ||
		type == MESSAGE_TICKER_DATA ||
		type == MESSAGE_TICKER_ADD_SUBSCRIPTION ||
		type == MESSAGE_TICKER_REMOVE_SUBSCRIPTION ||
		type == MESSAGE_STREAM_DATA ||
		type == MESSAGE_STREAM_ADD_SUBSCRIPTION ||
		type == MESSAGE_STREAM_REMOVE_SUBSCRIPTION ||
		type == MESSAGE_HISTORY_DATA ||
		type == MESSAGE_HISTORY_ADD_SUBSCRIPTION ||
		type == MESSAGE_HISTORY_REMOVE_SUBSCRIPTION ||
		type == MESSAGE_TEXT_FORMAT_COMMENT ||
		type == MESSAGE_TEXT_FORMAT_SPECIAL;
}

/* -------------------------------------------------------------------------- */

bool dx_is_data_message (int type) {
	return type == MESSAGE_TICKER_DATA ||
		type == MESSAGE_STREAM_DATA ||
		type == MESSAGE_HISTORY_DATA;
}

/* -------------------------------------------------------------------------- */

bool dx_is_subscription_message (int type) {
	return type == MESSAGE_TICKER_ADD_SUBSCRIPTION ||
		type == MESSAGE_TICKER_REMOVE_SUBSCRIPTION ||
		type == MESSAGE_STREAM_ADD_SUBSCRIPTION ||
		type == MESSAGE_STREAM_REMOVE_SUBSCRIPTION ||
		type == MESSAGE_HISTORY_ADD_SUBSCRIPTION ||
		type == MESSAGE_HISTORY_REMOVE_SUBSCRIPTION;
}

/* -------------------------------------------------------------------------- */
/*
 *	Message roster functions implementation
 */
/* -------------------------------------------------------------------------- */

const int* dx_get_send_message_roster (void) {
	return g_send_msg_roster;
}

/* -------------------------------------------------------------------------- */

int dx_get_send_message_roster_size (void) {
	return g_send_msg_count;
}

/* -------------------------------------------------------------------------- */

const int* dx_get_recv_message_roster (void) {
	return g_recv_msg_roster;
}

/* -------------------------------------------------------------------------- */

int dx_get_recv_message_roster_size (void) {
	return g_recv_msg_count;
}

/* -------------------------------------------------------------------------- */
/*
 *	Miscellaneous message functions implementation
 */
/* -------------------------------------------------------------------------- */

dxf_const_string_t dx_get_message_type_name (int type) {
	switch (type){
	case MESSAGE_HEARTBEAT: return L"HEARTBEAT";
	case MESSAGE_DESCRIBE_PROTOCOL: return L"DESCRIBE_PROTOCOL";
	case MESSAGE_DESCRIBE_RECORDS: return L"DESCRIBE_RECORDS";
	case MESSAGE_TICKER_DATA: return L"TICKER_DATA";
	case MESSAGE_TICKER_ADD_SUBSCRIPTION: return L"TICKER_ADD_SUBSCRIPTION";
	case MESSAGE_TICKER_REMOVE_SUBSCRIPTION: return L"TICKER_REMOVE_SUBSCRIPTION";
	case MESSAGE_STREAM_DATA: return L"STREAM_DATA";
	case MESSAGE_STREAM_ADD_SUBSCRIPTION: return L"STREAM_ADD_SUBSCRIPTION";
	case MESSAGE_STREAM_REMOVE_SUBSCRIPTION: return L"STREAM_REMOVE_SUBSCRIPTION";
	case MESSAGE_HISTORY_DATA: return L"HISTORY_DATA";
	case MESSAGE_HISTORY_ADD_SUBSCRIPTION: return L"HISTORY_ADD_SUBSCRIPTION";
	case MESSAGE_HISTORY_REMOVE_SUBSCRIPTION: return L"HISTORY_REMOVE_SUBSCRIPTION";
	default: return L"UNKNOWN MESSAGE TYPE";
	}
}