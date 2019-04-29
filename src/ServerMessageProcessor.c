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

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "DXFeed.h"

#include "ServerMessageProcessor.h"
#include "BufferedInput.h"
#include "DXMemory.h"
#include "SymbolCodec.h"
#include "DataStructures.h"
#include "Decimal.h"
#include "EventData.h"
#include "RecordTranscoder.h"
#include "RecordBuffers.h"
#include "Logger.h"
#include "DXAlgorithms.h"
#include "ConnectionContextData.h"
#include "DXErrorHandling.h"
#include "DXThreads.h"
#include "TaskQueue.h"
#include "DXNetwork.h"
#include "Snapshot.h"
#include "DXProperties.h"

/* -------------------------------------------------------------------------- */
/*
 *	Common data
 */
/* -------------------------------------------------------------------------- */

#define SYMBOL_BUFFER_LEN 64

#define MIN_FIELD_TYPE_ID 0x00
#define MAX_FIELD_TYPE_ID 0xFF

#define DEFAULT_BUFFER_CAPACITY     16000

#define DESCRIBE_PROTOCOL_TIMEOUT   3000

#define MRU_EVENT_FLAGS 1

#define DX_RECV_PROPERTY_AUTH L"authentication"
#define DX_RECV_PROPERTY_LOGIN_REQUIRED L"LOGIN "

/* -------------------------------------------------------------------------- */
/*
 *	Client-server data overlapping structures and stuff
 */
/* -------------------------------------------------------------------------- */

typedef struct {
	int type;
	dx_record_field_setter_t setter;
	dx_record_field_def_val_getter_t def_val_getter;
} dx_field_digest_t, *dx_field_digest_ptr_t;

typedef struct {
	dx_field_digest_ptr_t* elements;
	int size;
	bool in_sync_with_server;
} dx_record_digest_t;

typedef struct {
	dx_record_digest_t* elements;
	size_t size;
	size_t capacity;
} dx_record_digest_list_t;

/* -------------------------------------------------------------------------- */
/*
 *	Message support data
 */
/* -------------------------------------------------------------------------- */

typedef enum {
	dx_dps_not_sent,
	dx_dps_received,
	dx_dps_not_received,
	dx_dps_pending
} dx_describe_protocol_status_t;

/* -------------------------------------------------------------------------- */
/*
 *	Server message processor connection context
 */
/* -------------------------------------------------------------------------- */

typedef struct {
	dxf_connection_t connection;

	dxf_byte_t* buffer;
	dxf_int_t buffer_size;
	dxf_int_t buffer_pos;
	dxf_int_t buffer_capacity;

	dxf_char_t symbol_buffer[SYMBOL_BUFFER_LEN + 1];
	dxf_string_t symbol_result;

	dxf_string_t last_symbol;
	dxf_int_t last_cipher;
	dxf_event_flags_t last_flags;
	dxf_event_flags_t mru_event_flags;

	dx_record_server_support_state_list_t* record_server_support_states;
	dx_record_digest_list_t record_digests;
	dx_mutex_t record_digests_guard;

	void* bicc; /* buffered input connection context */
	void* rbcc; /* record buffers connection context */
	void* dscc; /* data structures connection context */

	int send_msgs_bitmask; /* the bitmask of server supported message types that _our_application_ sends */
	int recv_msgs_bitmask; /* the bitmask of server supported message types that _our_application_ receives */

	dx_describe_protocol_status_t describe_protocol_status;
	int describe_protocol_timestamp;
	dx_mutex_t describe_protocol_guard;
	char* raw_dump_file_name;
	/* Contains received properties from server */
	dx_property_map_t recv_props;
} dx_server_msg_proc_connection_context_t;

/* -------------------------------------------------------------------------- */

void dx_init_record_digests_list(dx_record_digest_list_t* record_digests) {
	if (record_digests == NULL)
		return;
	record_digests->elements = NULL;
	record_digests->size = 0;
	record_digests->capacity = 0;
}

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_server_msg_processor) {
	dx_server_msg_proc_connection_context_t* context = NULL;

	CHECKED_CALL_2(dx_validate_connection_handle, connection, true);

	context = dx_calloc(1, sizeof(dx_server_msg_proc_connection_context_t));

	if (context == NULL) {
		return false;
	}

	context->connection = connection;
	dx_init_record_digests_list(&(context->record_digests));

	if (!dx_mutex_create(&context->record_digests_guard)) {
		dx_free(context);

		return false;
	}

	if ((context->bicc = dx_get_buffered_input_connection_context(connection)) == NULL ||
		(context->rbcc = dx_get_record_buffers_connection_context(connection)) == NULL ||
		(context->dscc = dx_get_data_structures_connection_context(connection)) == NULL ||
		(context->record_server_support_states = dx_get_record_server_support_states(context->dscc)) == NULL) {

		dx_free(context);

		return dx_set_error_code(dx_cec_connection_context_not_initialized);
	}

	context->buffer = dx_malloc(DEFAULT_BUFFER_CAPACITY);

	if (context->buffer == NULL) {
		dx_free(context);

		return false;
	}

	context->buffer_capacity = DEFAULT_BUFFER_CAPACITY;

	context->describe_protocol_status = dx_dps_not_sent;

	if (!dx_mutex_create(&context->describe_protocol_guard)) {
		dx_free(context);

		return false;
	}

	if (!dx_set_subsystem_data(connection, dx_ccs_server_msg_processor, context)) {
		dx_free(context->buffer);
		dx_free(context);

		return false;
	}

	context->mru_event_flags = MRU_EVENT_FLAGS;
	context->raw_dump_file_name = NULL;

	return true;
}

/* -------------------------------------------------------------------------- */

void dx_clear_record_digests (dx_server_msg_proc_connection_context_t* context);

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_server_msg_processor) {
	bool res = true;
	dx_server_msg_proc_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_server_msg_processor, &res);

	if (context == NULL) {
		return res;
	}

	CHECKED_FREE(context->buffer);
	CHECKED_FREE(context->raw_dump_file_name);

	dx_clear_record_digests(context);
	dx_property_map_free_collection(&context->recv_props);
	dx_free(context);

	return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_server_msg_processor) {
	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Field digest functions
 */
/* -------------------------------------------------------------------------- */

dx_record_digest_t* dx_get_record_digest(dx_server_msg_proc_connection_context_t* context, dx_record_id_t record_id) {
	dx_record_digest_list_t* record_digests = NULL;
	if (context == NULL)
		return NULL;
	record_digests = &(context->record_digests);
	if (record_digests->elements == NULL || record_id < 0 || record_id >= (dx_record_id_t)record_digests->size)
		return NULL;
	return &(record_digests->elements[record_id]);
}

bool dx_create_field_digest (dx_server_msg_proc_connection_context_t* context,
							dx_record_id_t record_id, const dx_record_item_t* record_info,
							OUT dx_field_digest_ptr_t* field_digest) {
	dxf_string_t field_name = NULL;
	dxf_int_t field_type;
	int field_index = INVALID_INDEX;
	dx_record_server_support_state_t* state = NULL;

	CHECKED_CALL_2(dx_read_utf_string, context->bicc, &field_name);
	CHECKED_CALL_2(dx_read_compact_int, context->bicc, &field_type);

	if (field_name == NULL || dx_string_length(field_name) == 0 ||
		field_type < MIN_FIELD_TYPE_ID || field_type > MAX_FIELD_TYPE_ID) {

		dx_free(field_name);

		return dx_set_error_code(dx_pec_descr_record_field_info_corrupted);
	}

	if (record_info == NULL) {
		/* a special case of a dummy digest */

		*field_digest = NULL;

		dx_free(field_name);

		return true;
	}

	field_index = dx_find_record_field(record_info, field_name, field_type);

	dx_free(field_name);

	*field_digest = dx_calloc(1, sizeof(dx_field_digest_t));

	if (field_digest == NULL) {
		return false;
	}

	(*field_digest)->type = field_type;

	if (field_index != INVALID_INDEX) {
		(*field_digest)->setter = record_info->fields[field_index].setter;
		if (!dx_get_record_server_support_state_value(context->record_server_support_states, record_id, &state)) {
			return false;
		}
		*state |= INDEX_BITMASK(field_index);
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_digest_unsupported_fields (dx_server_msg_proc_connection_context_t* context,
								dx_record_id_t record_id, const dx_record_item_t* record_info) {
	dx_record_digest_t* record_digest = NULL;
	int field_index = 0;

	if (record_id < 0) {
		/* a special case of a dummy digest */

		return true;
	}

	record_digest = dx_get_record_digest(context, record_id);
	if (record_digest == NULL)
		return dx_set_error_code(dx_ec_invalid_func_param_internal);

	for (; field_index < record_info->field_count; ++field_index) {
		dx_field_digest_ptr_t field_digest = NULL;
		dx_record_server_support_state_t* state;
		if (!dx_get_record_server_support_state_value(context->record_server_support_states, record_id, &state)) {
			return false;
		}

		if (*state & INDEX_BITMASK(field_index)) {
			/* the field is supported by server, skipping */

			continue;
		}

		field_digest = (dx_field_digest_ptr_t)dx_calloc(1, sizeof(dx_field_digest_t));

		if (field_digest == NULL) {
			return false;
		}

		field_digest->setter = record_info->fields[field_index].setter;
		field_digest->def_val_getter = record_info->fields[field_index].def_val_getter;

		record_digest->elements[(record_digest->size)++] = field_digest;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

void dx_init_record_digest(dx_record_digest_t* digest) {
	if (digest == NULL)
		return;
	digest->elements = NULL;
	digest->size = 0;
	digest->in_sync_with_server = false;
}

/* -------------------------------------------------------------------------- */

void dx_clear_record_digest (dx_record_digest_t* digest) {
	int i = 0;

	if (digest == NULL) {
		return;
	}

	for (; i < digest->size; ++i) {
		dx_free(digest->elements[i]);
	}

	CHECKED_FREE(digest->elements);

	dx_init_record_digest(digest);
}

/* -------------------------------------------------------------------------- */

void dx_clear_record_digests (dx_server_msg_proc_connection_context_t* context) {
	dx_record_id_t i = 0;
	dx_record_id_t count = 0;

	if (!dx_mutex_lock(&(context->record_digests_guard))) {
		return;
	}

	if (context->record_digests.elements == NULL) {
		dx_mutex_unlock(&(context->record_digests_guard));

		return;
	}

	count = (dx_record_id_t)context->record_digests.size;
	for (; i < count; ++i) {
		dx_record_digest_t* record_digest = dx_get_record_digest(context, i);
		if (record_digest == NULL)
			continue;
		dx_clear_record_digest(record_digest);
	}

	dx_free(context->record_digests.elements);
	dx_init_record_digests_list(&(context->record_digests));
	dx_mutex_unlock(&(context->record_digests_guard));
}

/* -------------------------------------------------------------------------- */

void dx_init_record_digests(dx_server_msg_proc_connection_context_t* context) {
	dx_record_id_t i;
	dx_record_id_t count = dx_get_records_list_count(context->dscc);
	for (i = 0; i < count; i++) {
		dx_add_record_digest_to_list(context->connection, i);
	}
}

/* -------------------------------------------------------------------------- */
/*
 *	Server synchronization functions implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_clear_server_info (dxf_connection_t connection) {
	bool res = true;
	dx_server_msg_proc_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_server_msg_processor, &res);
	dx_record_id_t rid;
	dx_record_id_t records_count;

	if (context == NULL) {
		if (res) {
			return dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return false;
	}

	/* stage 1 - resetting all the field flags */
	records_count = dx_get_records_list_count(context->dscc);
	for (rid = 0; rid < records_count; ++rid) {
		dx_record_server_support_state_t* state;
		if (!dx_get_record_server_support_state_value(context->record_server_support_states, rid, &state)) {
			return false;
		}
		*state = 0;
	}

	/* stage 2 - freeing all the memory allocated by previous synchronization */
	dx_clear_record_digests(context);
	dx_init_record_digests(context);

	/* stage 3 - dropping all the info about supported message types */
	CHECKED_CALL(dx_mutex_lock, &(context->describe_protocol_guard));
	context->send_msgs_bitmask = context->recv_msgs_bitmask = 0;
	context->describe_protocol_status = dx_dps_not_sent;
	dx_drop_unsubscribe_counter(context->dscc);
	CHECKED_CALL(dx_mutex_unlock, &(context->describe_protocol_guard));

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Message support functions
 */
/* -------------------------------------------------------------------------- */

bool dx_lock_describe_protocol_processing (dxf_connection_t connection, bool lock) {
	bool res = true;
	dx_server_msg_proc_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_server_msg_processor, &res);

	if (context == NULL) {
		if (res) {
			return dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return false;
	}

	if (lock) {
		return dx_mutex_lock(&(context->describe_protocol_guard));
	} else {
		return dx_mutex_unlock(&(context->describe_protocol_guard));
	}
}

/* -------------------------------------------------------------------------- */

static bool dx_get_msg_support_state (int msg, const int* roster, size_t count, int bitmask) {
	size_t idx;
	bool found = false;

	DX_ARRAY_SEARCH(roster, 0, count, msg, DX_NUMERIC_COMPARATOR, false, found, idx);

	if (!found) {
		return false;
	}

	if (IS_FLAG_SET(bitmask, INDEX_BITMASK(idx))) {
		return true;
	}

	return false;
}

/* ---------------------------------- */

bool dx_is_message_supported_by_server (dxf_connection_t connection, dx_message_type_t msg, bool lock_required,
										OUT dx_message_support_status_t* status) {
	bool res = true;
	dx_server_msg_proc_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_server_msg_processor, &res);

	if (context == NULL) {
		if (res) {
			return dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return false;
	}

	if (status == NULL || !dx_is_message_type_valid(msg)) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	if (lock_required) {
		CHECKED_CALL(dx_mutex_lock, &(context->describe_protocol_guard));
	}
	switch (context->describe_protocol_status) {
	case dx_dps_not_sent:
		dx_logging_info(L"reconnection in progress");
		*status = dx_mss_reconnection;
		break;
	case dx_dps_pending:
		*status = dx_mss_pending;
		break;
	case dx_dps_received:
	case dx_dps_not_received:
		/* those cases are identical in the way that is important here, because they both properly set the message bitmasks */

		if (dx_get_msg_support_state(msg, dx_get_send_message_roster(), dx_get_send_message_roster_size(), context->send_msgs_bitmask) ||
			dx_get_msg_support_state(msg, dx_get_recv_message_roster(), dx_get_recv_message_roster_size(), context->recv_msgs_bitmask)) {

			*status = dx_mss_supported;
		} else {
			*status = dx_mss_not_supported;
		}
	}

	if (lock_required) {
		return dx_mutex_unlock(&context->describe_protocol_guard);
	}
	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_legacy_send_msg_bitmask (OUT int* bitmask) {
	static int s_legacy_msg_list[] = {
		MESSAGE_TICKER_ADD_SUBSCRIPTION,
		MESSAGE_TICKER_REMOVE_SUBSCRIPTION,

		MESSAGE_STREAM_ADD_SUBSCRIPTION,
		MESSAGE_STREAM_REMOVE_SUBSCRIPTION,

		MESSAGE_HISTORY_ADD_SUBSCRIPTION,
		MESSAGE_HISTORY_REMOVE_SUBSCRIPTION
	};
	static int s_msg_count = sizeof(s_legacy_msg_list) / sizeof(s_legacy_msg_list[0]);
	static int s_msg_bitmask = 0;

	if (bitmask == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	if (s_msg_bitmask == 0) {
		const int* msg_roster = dx_get_send_message_roster();
		size_t roster_size = dx_get_send_message_roster_size();
		int i = 0;

		for (; i < s_msg_count; ++i) {
			size_t msg_index = 0;
			bool found = false;

			DX_ARRAY_SEARCH(msg_roster, 0, roster_size, s_legacy_msg_list[i], DX_NUMERIC_COMPARATOR, false, found, msg_index);

			if (!found) {
				/* Nonsense! The legacy messages mustn't be absent in our message rosters */

				return dx_set_error_code(dx_ec_internal_assert_violation);
			}

			s_msg_bitmask |= INDEX_BITMASK(msg_index);
		}
	}

	*bitmask = s_msg_bitmask;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_legacy_recv_msg_bitmask (int* bitmask) {
	static int s_legacy_msg_list[] = {
		MESSAGE_TICKER_DATA,

		MESSAGE_STREAM_DATA,

		MESSAGE_HISTORY_DATA
	};
	static int s_msg_count = sizeof(s_legacy_msg_list) / sizeof(s_legacy_msg_list[0]);
	static int s_msg_bitmask = 0;

	if (bitmask == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	if (s_msg_bitmask == 0) {
		const int* msg_roster = dx_get_recv_message_roster();
		size_t roster_size = dx_get_recv_message_roster_size();
		int i = 0;

		for (; i < s_msg_count; ++i) {
			size_t msg_index = 0;
			bool found = false;

			DX_ARRAY_SEARCH(msg_roster, 0, roster_size, s_legacy_msg_list[i], DX_NUMERIC_COMPARATOR, false, found, msg_index);

			if (!found) {
				/* Nonsense! The legacy messages mustn't be absent in our message rosters */

				return dx_set_error_code(dx_ec_internal_assert_violation);
			}

			s_msg_bitmask |= INDEX_BITMASK(msg_index);
		}
	}

	*bitmask = s_msg_bitmask;

	return true;
}

/* -------------------------------------------------------------------------- */

int dx_describe_protocol_timeout_countdown_task (void* data, int command) {
	/*
	*	This is a special worker thread task that processes the describe protocol timeout logic
	*/

	dx_server_msg_proc_connection_context_t* context = data;
	int res = 0;

	if (IS_FLAG_SET(command, dx_tc_free_resources)) {
		return dx_tes_success;
	}

	if (!dx_mutex_lock(&(context->describe_protocol_guard))) {
		return dx_tes_dont_advance;
	}

	switch (context->describe_protocol_status) {
	case dx_dps_not_sent:
	case dx_dps_not_received:
		/*
		*	These cases must never happen.
		*  The first is obvious. The second must not be met here because this function IS the ONLY place
		*  where such state may be set, and it would pop itself from the task queue in that case.
		*/
		dx_logging_info(L"Oops, we are here: %d", context->describe_protocol_status);
		dx_set_error_code(dx_ec_internal_assert_violation);

		res = dx_tes_dont_advance;

		break;
	case dx_dps_received:
		/*
		*	The message was received in time. This task is of no more use.
		*/

		res = dx_tes_pop_me | dx_tes_success;

		break;
	case dx_dps_pending:
		/*
		*	That's the main case for us. The message hasn't been received yet, and the timeout is ticking.
		*/

		if (dx_millisecond_timestamp_diff(dx_millisecond_timestamp(), context->describe_protocol_timestamp) < DESCRIBE_PROTOCOL_TIMEOUT) {
			/*
			*	Timeout hasn't run out yet, keeping on waiting
			*/

			res = dx_tes_dont_advance | dx_tes_success;

			break;
		}

		/*
		*	The timeout has passed, yet no message had been received. Setting the describe protocol state and message support
		*  bitmasks accordingly and finishing the job.
		*/

		res = dx_tes_pop_me | dx_tes_success;

		context->describe_protocol_status = dx_dps_not_received;

		if (!dx_legacy_send_msg_bitmask(&(context->send_msgs_bitmask)) ||
			!dx_legacy_recv_msg_bitmask(&(context->recv_msgs_bitmask))) {

			res &= ~dx_tes_success;
		}
	}

	if (!dx_mutex_unlock(&(context->describe_protocol_guard))) {
		res &= ~dx_tes_success;
	}

	return res;
}

/* ---------------------------------- */

bool dx_describe_protocol_sent (dxf_connection_t connection) {
	bool res = true;
	dx_server_msg_proc_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_server_msg_processor, &res);

	if (context == NULL) {
		if (res) {
			return dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return false;
	}

	CHECKED_CALL(dx_mutex_lock, &(context->describe_protocol_guard));

	res = true;

	switch (context->describe_protocol_status) {
	case dx_dps_not_received:
	case dx_dps_pending:
		/*
		*	These cases must never happen.
		*  Because these states may be set only after this function was called.
		*/

		dx_set_error_code(dx_ec_internal_assert_violation);

		res = false;

		break;
	case dx_dps_not_sent:
		/*
		*	that's the most normal case - the message has just been sent for the first time.
		*  our actions are:
				1) to set the describe protocol timestamp;
				2) to set the describe protocol status as pending
				3) to add a special task to the worker thread queue, that would count down
				the describe message response timeout
		*/

		context->describe_protocol_timestamp = dx_millisecond_timestamp();
		context->describe_protocol_status = dx_dps_pending;

		res = dx_add_worker_thread_task(connection, dx_describe_protocol_timeout_countdown_task, (void*)context) && res;

		break;
	case dx_dps_received:
		/*
		*	That's the most unlikely but still a valid case. The describe protocol response was received before we even got
		*  here to inform that out describe protocol was sent.
		*  Do nothing and return success.
		*/

		break;
	}

	return dx_mutex_unlock(&(context->describe_protocol_guard)) && res;
}

/* -------------------------------------------------------------------------- */
/*
 *  Server message processor helpers
 */
/* -------------------------------------------------------------------------- */

static bool dx_read_message_type (dx_server_msg_proc_connection_context_t* context, OUT dx_message_type_t* value) {
	dxf_long_t type;

	if (!dx_read_compact_long(context->bicc, &type)) {
		if (dx_get_error_code() == dx_bioec_buffer_underflow) {
			return dx_set_error_code(dx_pec_message_incomplete);
		}

		return false;
	}

	if (!dx_is_message_type_valid((dx_message_type_t)type)) {
		return dx_set_error_code(dx_pec_unexpected_message_type);
	}

	*value = (dx_message_type_t)type;

	return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_read_message_length_and_set_buffer_limit (dx_server_msg_proc_connection_context_t* context) {
	dxf_long_t message_length;
	int message_end_offset;

	if (!dx_read_compact_long(context->bicc, &message_length)) {
		return dx_set_error_code_impl(dx_pec_message_incomplete, false);
	}

	if (message_length < 0 || message_length > INT_MAX - dx_get_in_buffer_position(context->bicc)) {
		return dx_set_error_code(dx_pec_invalid_message_length);
	}

	message_end_offset = dx_get_in_buffer_position(context->bicc) + (int)message_length;

	if (message_end_offset > context->buffer_size) {
		return dx_set_error_code_impl(dx_pec_message_incomplete, false);
	}

	dx_set_in_buffer_limit(context->bicc, message_end_offset); /* setting limit for this message */

	return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_read_symbol (dx_server_msg_proc_connection_context_t* context) {
	dxf_int_t r;

	if (context == NULL ) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	if (dx_codec_read_symbol(context->bicc, context->symbol_buffer, SYMBOL_BUFFER_LEN,
							&(context->symbol_result), &r, &(context->last_flags),
							&(context->mru_event_flags)) == false) {
		return false;
	}

	if ((r & dx_get_codec_valid_cipher()) != 0) {
		context->last_cipher = r;
		CHECKED_FREE(context->last_symbol);
		context->last_symbol = NULL;
	} else if (r > 0) {
		context->last_cipher = 0;
		CHECKED_FREE(context->last_symbol);
		context->last_symbol = dx_create_string_src_len(context->symbol_buffer, r);

		if (context->last_symbol == NULL) {
			return false;
		}
	} else {
		if (context->symbol_result != NULL) {
			context->last_symbol = context->symbol_result;
			context->symbol_result = NULL;

			context->last_cipher = dx_encode_symbol_name(context->last_symbol);
		}
		if (context->last_cipher == 0 && context->last_symbol == NULL)
			return dx_set_error_code(dx_pec_invalid_symbol);
	}

	return true;
}

/* -------------------------------------------------------------------------- */

#define CHECKED_SET_VALUE(setter, buffer, value) \
	if (setter != NULL) { \
		setter(buffer, value); \
	}

bool dx_read_qdtime_on_remove_event(dx_server_msg_proc_connection_context_t* context,
									dx_record_id_t record_id, void* row) {
	int i;
	dxf_int_t high;
	dxf_int_t low;
	dxf_ulong_t qdtime;
	const dx_record_item_t* record_info = dx_get_record_by_id(context->dscc, record_id);

	if (record_info == NULL)
		return dx_set_error_code(dx_ec_invalid_func_param_internal);

	CHECKED_CALL_2(dx_read_compact_long, context->bicc, &qdtime);

	high = (dxf_int_t)(qdtime >> 32);
	low = (dxf_int_t)(qdtime & 0xFFFFFFFF);
	for (i = 0; i < record_info->field_count; ++i) {
		dx_field_info_t field = record_info->fields[i];
		if (field.time == dx_ft_first_time_int_field) {
			CHECKED_SET_VALUE(field.setter, row, &high);
		} else if (field.time == dx_ft_second_time_int_field) {
			CHECKED_SET_VALUE(field.setter, row, &low);
		}
	}

	return true;
}

bool dx_read_records (dx_server_msg_proc_connection_context_t* context,
					dx_record_id_t record_id, void* record_buffer) {
	int i = 0;
	dxf_byte_t read_byte;
	dxf_short_t read_short;
	dxf_int_t read_int;
	dxf_char_t read_utf_char;
	dxf_double_t read_double;
	dxf_string_t read_string;
	dxf_byte_array_t read_byte_array;

	if (IS_FLAG_SET(context->last_flags, dxf_ef_remove_event)) {
		return dx_read_qdtime_on_remove_event(context, record_id, record_buffer);
	}

	CHECKED_CALL(dx_mutex_lock, &(context->record_digests_guard));

	const dx_record_digest_t* record_digest = dx_get_record_digest(context, record_id);

	if (record_digest == NULL) {
		CHECKED_CALL(dx_mutex_unlock, &(context->record_digests_guard));

		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	for (; i < record_digest->size; ++i) {
		int serialization = record_digest->elements[i]->type & dx_fid_mask_serialization;
		int presentation = record_digest->elements[i]->type & dx_fid_mask_presentation;

		switch (serialization) {
		case dx_fid_void:
			/* 0 here means that we're dealing with the field the server does not support;
				using the default value for it */
			record_digest->elements[i]->setter(record_buffer, record_digest->elements[i]->def_val_getter());

			break;
		case dx_fid_byte:
			CHECKED_CALL_2(dx_read_byte, context->bicc, &read_byte);

			if (presentation == dx_fid_flag_decimal) {
				CHECKED_CALL_2(dx_int_to_double, read_byte, &read_double);
				CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_double);
			} else {
				CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_byte);
			}

			break;
		case dx_fid_utf_char:
			/* No special presentation bits are supported for UTF char */
			CHECKED_CALL_2(dx_read_utf_char, context->bicc, &read_int);

			read_utf_char = read_int;

			CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_int);

			break;
		case dx_fid_short:
			/* No special presentation bits are supported for UTF char */
			CHECKED_CALL_2(dx_read_short, context->bicc, &read_short);

			if (presentation == dx_fid_flag_decimal) {
				CHECKED_CALL_2(dx_int_to_double, read_short, &read_double);
				CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_double);
			} else {
				CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_short);
			}

			break;
		case dx_fid_int:
			CHECKED_CALL_2(dx_read_int, context->bicc, &read_int);

			if (presentation == dx_fid_flag_decimal) {
				CHECKED_CALL_2(dx_int_to_double, read_int, &read_double);
				CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_double);
			} else {
				CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_int);
			}

			break;
		case dx_fid_compact_int:
			CHECKED_CALL_2(dx_read_compact_int, context->bicc, &read_int);

			if (presentation == dx_fid_flag_decimal) {
				CHECKED_CALL_2(dx_int_to_double, read_int, &read_double);
				CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_double);
			} else {
				CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_int);
			}

			break;
		case dx_fid_byte_array:
			if (presentation == dx_fid_flag_string) {
				/* Treat as UTF char array with length in bytes */
				CHECKED_CALL_2(dx_read_utf_string, context->bicc, &read_string);

				dx_store_string_buffer(context->rbcc, read_string);

				CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_string);
			} else {
				CHECKED_CALL_2(dx_read_byte_array, context->bicc, &read_byte_array);

				dx_store_byte_array_buffer(context->rbcc, read_byte_array);
				/* Objects goes as byte array to.
				According to specification (DESCRIBE_RECORDS.txt):

				Unsupported values in those bits are reserved and MUST be ignored.
				such field SHOULD be treated as PLAIN.
				*/
				CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_byte_array);
			}

			break;
		case dx_fid_utf_char_array:
			CHECKED_CALL_2(dx_read_utf_char_array, context->bicc, &read_string);

			dx_store_string_buffer(context->rbcc, read_string);

			CHECKED_SET_VALUE(record_digest->elements[i]->setter, record_buffer, &read_string);

			break;
		default:
			CHECKED_CALL(dx_mutex_unlock, &(context->record_digests_guard));

			return dx_set_error_code(dx_pec_record_field_type_not_supported);
		}
	}

	CHECKED_CALL(dx_mutex_unlock, &(context->record_digests_guard));

	return true;
}

/* -------------------------------------------------------------------------- */

#define CHECKED_GET_VALUE(getter, buffer, value) \
	if (getter != NULL) { \
		getter(buffer, value); \
	}

dxf_time_int_field_t dx_get_time_int_field(void* dscc, dx_record_id_t record_id, void* record_buffer) {
	int i;
	dxf_ulong_t high = 0;
	dxf_ulong_t low = 0;
	dxf_time_int_field_t time = 0;
	const dx_record_item_t* record_info = dx_get_record_by_id(dscc, record_id);

	if (record_info == NULL)
		return ULLONG_MAX;

	for (i = 0; i < record_info->field_count; ++i) {
		dx_field_info_t field = record_info->fields[i];
		if (field.time == dx_ft_first_time_int_field) {
			CHECKED_GET_VALUE(field.getter, record_buffer, &high);
		}
		else if (field.time == dx_ft_second_time_int_field) {
			CHECKED_GET_VALUE(field.getter, record_buffer, &low);
		}
	}

	time = (high << 32) | (low & 0xFFFFFFFF);

	return time;
}

/* -------------------------------------------------------------------------- */

bool dx_process_data_message (dx_server_msg_proc_connection_context_t* context) {
	context->last_cipher = 0;
	CHECKED_FREE(context->last_symbol);
	context->last_symbol = NULL;
	context->last_flags = 0;
	context->mru_event_flags = dxf_ef_tx_pending;

	while (dx_get_in_buffer_position(context->bicc) < dx_get_in_buffer_limit(context->bicc)) {
		void* record_buffer = NULL;
		dx_record_id_t record_id;
		dxf_const_string_t suffix;
		int record_count = 0;
		const dx_record_item_t* record_info = NULL;
		dx_record_digest_t* record_digest = NULL;
		dx_record_params_t record_params;
		dxf_event_params_t event_params;

		CHECKED_CALL_1(dx_read_symbol, context);

		if (context->last_symbol == NULL && !dx_decode_symbol_name(context->last_cipher, (dxf_const_string_t*)&context->last_symbol)) {
			return false;
		}

		dxf_int_t server_record_id;
		{
			if (!dx_read_compact_int(context->bicc, &server_record_id)) {
				dx_free_buffers(context->rbcc);

				return false;
			}

			record_id = dx_get_record_id(context->dscc, server_record_id);
		}

		if (record_id < 0) {
			dx_free_buffers(context->rbcc);
			dx_logging_info(L"Not supported record from server (id=%d)", server_record_id);
			return dx_set_error_code(dx_pec_record_not_supported);
		}

		record_digest = dx_get_record_digest(context, record_id);
		if (record_digest == NULL) {
			dx_free_buffers(context->rbcc);
			return dx_set_error_code(dx_ec_invalid_func_param_internal);
		}
		if (!record_digest->in_sync_with_server) {
			dx_free_buffers(context->rbcc);

			return dx_set_error_code(dx_pec_record_description_not_received);
		}

		record_info = dx_get_record_by_id(context->dscc, record_id);
		if (record_info == NULL)
			return false;

		record_buffer = g_buffer_managers[record_info->info_id].record_getter(context->rbcc, record_count++);
		suffix = dx_string_length(record_info->suffix) > 0 ? record_info->suffix : NULL;

		if (record_buffer == NULL) {
			dx_free_buffers(context->rbcc);

			return false;
		}


		if (!dx_read_records(context, record_id, record_buffer)) {
			dx_free_buffers(context->rbcc);

			return false;
		}
		// TODO: add assert to overlimit in context->bicc limit

		record_params.record_id = record_id;
		record_params.record_info_id = record_info->info_id;
		record_params.suffix = suffix;
		record_params.symbol_name = context->last_symbol;
		record_params.symbol_cipher = context->last_cipher;
		record_params.flags = context->last_flags;
		record_params.time_int_field = dx_get_time_int_field(context->dscc, record_id, record_buffer);

		event_params.flags = record_params.flags;
		event_params.time_int_field = record_params.time_int_field;
		event_params.snapshot_key = dx_new_snapshot_key(record_info->info_id, record_params.symbol_name, suffix);

		if (!dx_transcode_record_data(context->connection, &record_params, &event_params,
			g_buffer_managers[record_info->info_id].record_buffer_getter(context->rbcc), record_count)) {

			dx_free_buffers(context->rbcc);

			return false;
		}

		dx_free_buffers(context->rbcc);
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_fill_record_digest(dx_server_msg_proc_connection_context_t* context, dx_record_id_t rid, const dx_record_item_t* record_info,
						dxf_int_t field_count, OUT dx_record_digest_t* record_digest) {
	int i = 0;

	for (; i != field_count; ++i) {
		dx_field_digest_ptr_t field_digest = NULL;

		CHECKED_CALL_4(dx_create_field_digest, context, rid, record_info, &field_digest);

		if (field_digest != NULL && record_digest->elements != NULL) {
			record_digest->elements[(record_digest->size)++] = field_digest;
		}
	}

	CHECKED_CALL_3(dx_digest_unsupported_fields, context, rid, record_info);

	if (rid >= 0) {
		record_digest = dx_get_record_digest(context, rid);
		if (record_digest == NULL)
			return dx_set_error_code(dx_ec_invalid_func_param_internal);
		record_digest->in_sync_with_server = true;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_process_describe_records (dx_server_msg_proc_connection_context_t* context) {
	while (dx_get_in_buffer_position(context->bicc) < dx_get_in_buffer_limit(context->bicc)) {
		dxf_int_t server_record_id;
		dxf_string_t record_name = NULL;
		dxf_int_t server_field_count;
		const dx_record_item_t* record_info = NULL;
		dx_record_digest_t* record_digest = NULL;
		dx_record_id_t local_record_id = DX_RECORD_ID_INVALID;
		dx_record_digest_t dummy = { NULL, 0, 0 };

		CHECKED_CALL_2(dx_read_compact_int, context->bicc, &server_record_id);
		CHECKED_CALL_2(dx_read_utf_string, context->bicc, &record_name);
		CHECKED_CALL_2(dx_read_compact_int, context->bicc, &server_field_count);

		if (record_name == NULL || dx_string_length(record_name) == 0 || server_field_count < 0) {
			dx_free(record_name);

			return dx_set_error_code(dx_pec_record_info_corrupted);
		}

		local_record_id = dx_get_record_id_by_name(context->dscc, record_name);

		dx_free(record_name);

		if (local_record_id == DX_RECORD_ID_INVALID) {
			record_digest = &dummy;
		} else {
			CHECKED_CALL_3(dx_assign_server_record_id, context->dscc, local_record_id, server_record_id);

			record_info = dx_get_record_by_id(context->dscc, local_record_id);
			if (record_info == NULL)
				return false;

			record_digest = dx_get_record_digest(context, local_record_id);
			if (record_digest == NULL || record_digest->size < 0)
				return dx_set_error_code(dx_ec_invalid_func_param_internal);

			if (record_digest->elements != NULL) {
				dx_clear_record_digest(record_digest);
			}

			/* a generous memory allocation, to allow the maximum possible amount of pointers to be stored,
			but the overhead is insignificant */
			if ((record_digest->elements = dx_calloc(server_field_count + record_info->field_count, sizeof(dx_field_digest_ptr_t))) == NULL) {
				return false;
			}
		}

		CHECKED_CALL_5(dx_fill_record_digest, context, local_record_id, record_info, server_field_count, record_digest);
	}

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Describe protocol functions
 */
/* -------------------------------------------------------------------------- */

bool dx_read_describe_protocol_properties (dx_server_msg_proc_connection_context_t* context) {
	dxf_string_t key;
	dxf_string_t value;
	dxf_int_t count;
	int i = 0;

	CHECKED_CALL_2(dx_read_compact_int, context->bicc, &count);

	if (count < 0) {
		return dx_set_error_code(dx_pec_describe_protocol_message_corrupted);
	}

	for (; i < count; ++i) {
		CHECKED_CALL_2(dx_read_utf_string, context->bicc, &key);
		CHECKED_CALL_2(dx_read_utf_string, context->bicc, &value);

		dx_property_map_set(&(context->recv_props), key, value);

		/* so far the properties are not supported */

		dx_free(key);
		dx_free(value);
	}

	return true;
}

/* -------------------------------------------------------------------------- */

typedef void (*dx_message_type_processor_t)(dx_server_msg_proc_connection_context_t* context, int message_id, dxf_string_t message_name);

/* ---------------------------------- */

static void dx_process_server_send_message_type (dx_server_msg_proc_connection_context_t* context, int message_id, dxf_string_t message_name) {
	size_t msg_index;
	bool found = false;
	const int* msg_roster = dx_get_recv_message_roster();
	size_t roster_size = dx_get_recv_message_roster_size();

	/* Note: this function processes the _server_ send messages, i.e. they should be compared to the messages
	our application _receives_. */

	DX_ARRAY_SEARCH(msg_roster, 0, roster_size, message_id, DX_NUMERIC_COMPARATOR, false, found, msg_index);

	if (!found) {
		return;
	}

	if (dx_compare_strings(message_name, dx_get_message_type_name(msg_roster[msg_index])) != 0) {
		/* message id exists in our roster, but the names don't match */

		return;
	}

	context->recv_msgs_bitmask |= INDEX_BITMASK(msg_index);
}

/* ---------------------------------- */

static void dx_process_server_recv_message_type (dx_server_msg_proc_connection_context_t* context, int message_id, dxf_string_t message_name) {
	size_t msg_index;
	bool found = false;
	const int* msg_roster = dx_get_send_message_roster();
	size_t roster_size = dx_get_send_message_roster_size();

	/*
	Note: this function processes the _server_ recv messages, i.e. they
	should be compared to the messages our application _sends_.
	*/

	DX_ARRAY_SEARCH(msg_roster, 0, roster_size, message_id, DX_NUMERIC_COMPARATOR, false, found, msg_index);

	if (!found) {
		return;
	}

	if (dx_compare_strings(message_name, dx_get_message_type_name(msg_roster[msg_index])) != 0) {
		/* message id exists in our roster, but the names don't match */

		return;
	}

	context->send_msgs_bitmask |= INDEX_BITMASK(msg_index);
}

/* -------------------------------------------------------------------------- */

bool dx_read_describe_protocol_message_list (dx_server_msg_proc_connection_context_t* context, dx_message_type_processor_t processor) {
	dxf_int_t count;
	int i = 0;

	CHECKED_CALL_2(dx_read_compact_int, context->bicc, &count);

	if (count < 0) {
		return dx_set_error_code(dx_pec_describe_protocol_message_corrupted);
	}

	for (; i < count; ++i) {
		dxf_int_t message_id;
		dxf_string_t message_name;

		CHECKED_CALL_2(dx_read_compact_int, context->bicc, &message_id);
		CHECKED_CALL_2(dx_read_utf_string, context->bicc, &message_name);

		processor(context, (int)message_id, message_name);

		dx_free(message_name);

		/* read properties for this message type */
		CHECKED_CALL(dx_read_describe_protocol_properties, context);
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_process_describe_protocol (dx_server_msg_proc_connection_context_t* context) {
	dxf_int_t magic;
	int buf_pos;
	int buf_limit;
	dxf_const_string_t prop_value;

	CHECKED_CALL(dx_mutex_lock, &(context->describe_protocol_guard));

	if (context->describe_protocol_status == dx_dps_not_received) {
		/* so far we simply 'forgive' the server and return it from the legacy state to the modern one */

		/* clearing the bitmasks, so that the message handler fills them out */
		context->send_msgs_bitmask = context->recv_msgs_bitmask = 0;
	}

	if (!dx_read_int(context->bicc, &magic)) {
		dx_mutex_unlock(&(context->describe_protocol_guard));

		return false;
	}

	if (magic != 0x44585033) { /* 'D', 'X', 'P', '3' */
		dx_mutex_unlock(&(context->describe_protocol_guard));

		return dx_set_error_code(dx_pec_describe_protocol_message_corrupted);
	}

	dx_property_map_free_collection(&context->recv_props);

	if (!dx_read_describe_protocol_properties(context)) {
		dx_mutex_unlock(&(context->describe_protocol_guard));

		return false;
	}

	if (!dx_read_describe_protocol_message_list(context, dx_process_server_send_message_type)) {
		dx_mutex_unlock(&(context->describe_protocol_guard));

		return false;
	}

	if (!dx_read_describe_protocol_message_list(context, dx_process_server_recv_message_type)) {
		dx_mutex_unlock(&(context->describe_protocol_guard));

		return false;
	}

	/* Check received properties from server */
	if (dx_property_map_try_get_value(&(context->recv_props), DX_RECV_PROPERTY_AUTH, OUT &prop_value) && dx_string_length(prop_value) > 0) {
		if (dx_compare_strings_num(DX_RECV_PROPERTY_LOGIN_REQUIRED, prop_value, dx_string_length(DX_RECV_PROPERTY_LOGIN_REQUIRED)) == 0) {
			dx_logging_info(L"Login required: \"%ls\"", &prop_value[dx_string_length(DX_RECV_PROPERTY_LOGIN_REQUIRED)]);
			dx_connection_status_set(context->connection, dxf_cs_login_required);
			context->describe_protocol_status = dx_dps_pending;
			dx_mutex_unlock(&(context->describe_protocol_guard));
			return false;
		} else {
			dx_connection_status_set(context->connection, dxf_cs_not_connected);
			dx_logging_info(L"Login failed: \"%ls\"", prop_value);
			dx_set_error_code(dx_pec_authentication_error);
			dx_logging_error(prop_value);
			dx_mutex_unlock(&(context->describe_protocol_guard));
			return false;
		}
	}

	context->describe_protocol_status = dx_dps_received;

	dx_connection_status_set(context->connection, dxf_cs_authorized);

	/* All additional data must be skipped according to message length */
	buf_pos = dx_get_in_buffer_position(context->bicc);
	buf_limit = dx_get_in_buffer_limit(context->bicc);
	if (buf_pos < buf_limit)
		dx_set_in_buffer_position(context->bicc, buf_limit);

	return dx_mutex_unlock(&(context->describe_protocol_guard));
}

/* -------------------------------------------------------------------------- */
/*
 *	High level message processor functions
 */
/* -------------------------------------------------------------------------- */

bool dx_process_message (dx_server_msg_proc_connection_context_t* context, dx_message_type_t message_type) {
	if (dx_is_data_message(message_type)) {
		if (!dx_process_data_message(context)) {
			if (dx_get_error_code() == dx_bioec_buffer_underflow) {
				/* message processing would never start if the message length we previously
					read indicated we don't have enough data for the message. therefore,
					the message length is invalid */

				return dx_set_error_code(dx_pec_invalid_message_length);
			}

			return false;
		}

		return true;
	} else if (dx_is_subscription_message(message_type)) {
		return dx_set_error_code(dx_pec_unexpected_message_type);
	}

	switch (message_type) {
	case MESSAGE_DESCRIBE_RECORDS:
		if (!dx_process_describe_records(context)) {
			if (dx_get_error_code() == dx_bioec_buffer_underflow) {
				return dx_set_error_code(dx_pec_invalid_message_length);
			}

			return false;
		}

		break;
	case MESSAGE_DESCRIBE_PROTOCOL:
		if (!dx_process_describe_protocol(context)) {
			if (dx_get_error_code() == dx_bioec_buffer_underflow) {
				return dx_set_error_code(dx_pec_invalid_message_length);
			}

			return false;
		}

		break;
	case MESSAGE_HEARTBEAT:
		return dx_set_error_code_impl(dx_pec_server_message_not_supported, false);

	case MESSAGE_TEXT_FORMAT_COMMENT:
	case MESSAGE_TEXT_FORMAT_SPECIAL:
	default:
		/* ignoring and skipping these messages */
		return dx_set_error_code(dx_pec_server_message_not_supported);
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_append_new_data (dx_server_msg_proc_connection_context_t* context,
						const dxf_byte_t* new_buffer, dxf_int_t new_buffer_size) {
	if (context->buffer_size != context->buffer_pos) {
		/* copy all unprocessed data to beginning of buffer */
		dx_memmove(context->buffer, context->buffer + context->buffer_pos, context->buffer_size - context->buffer_pos);

		context->buffer_size = context->buffer_size - context->buffer_pos;
		context->buffer_pos = 0;
	} else {
		context->buffer_pos = context->buffer_size = 0;
	}

	/*
	*	Appending the new data to the old one (if present).
	*  That is possible if the server message got split into several network portions
	*/

	if (context->buffer_capacity < context->buffer_size + new_buffer_size) {
		dxf_byte_t* larger_buffer;

		context->buffer_capacity = context->buffer_size + new_buffer_size;
		larger_buffer = dx_malloc(context->buffer_capacity);

		if (larger_buffer == NULL) {
			return false;
		}

		dx_memcpy(larger_buffer, context->buffer, context->buffer_size);
		dx_free(context->buffer);
		context->buffer = larger_buffer;
	}

	dx_memcpy(context->buffer + context->buffer_size, new_buffer, new_buffer_size);

	context->buffer_size += new_buffer_size;

	dx_set_in_buffer(context->bicc, context->buffer, context->buffer_size);
	dx_set_in_buffer_position(context->bicc, context->buffer_pos);

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Main functions
 */
/* -------------------------------------------------------------------------- */

bool dx_process_server_data (dxf_connection_t connection, const dxf_byte_t* data_buffer, int data_buffer_size) {
	bool process_result = true;
	bool conn_ctx_res = true;
	dx_server_msg_proc_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_server_msg_processor, &conn_ctx_res);

	if (context == NULL) {
		if (conn_ctx_res) {
			return dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return false;
	}

	if (data_buffer_size <= 0) {
		return dx_set_error_code(dx_ec_internal_assert_violation);
	}

	if (!dx_append_new_data(context, data_buffer, data_buffer_size)) {
		return false;
	}

	while (dx_get_in_buffer_position(context->bicc) < context->buffer_size) {
		dx_message_type_t message_type = MESSAGE_HEARTBEAT; /* zero-length messages are treated as just heartbeats */
		const dxf_int_t message_start_pos = dx_get_in_buffer_position(context->bicc);

		if (!dx_read_message_length_and_set_buffer_limit(context)) {
			if (dx_get_error_code() == dx_pec_message_incomplete) {
				/*
				*	This is a valid case, just reset buffer position back to the
				*  message beginning and wait for the rest of the message data
				*/

				dx_set_in_buffer_position(context->bicc, message_start_pos);
				dx_set_error_code(dx_ec_success);

				break;
			} else {
				/*
				*	Something's wrong with the message format, skipping the whole buffer
				*/

				dx_set_in_buffer_position(context->bicc, context->buffer_size);
				dx_logging_last_error();

				process_result = false;

				break;
			}
		}

		if (dx_get_in_buffer_position(context->bicc) < dx_get_in_buffer_limit(context->bicc)) {
			/* we have a message with a non-zero length */

			if (!dx_read_message_type(context, &message_type)) {
				if (dx_get_error_code() == dx_pec_message_incomplete) {
					/*
					 *	In fact, this incompleteness is a sign that something went wrong,
					 *  since the message length was already read above and the buffer
					 *  has enough data according to the message length.
					 *  Skipping and aborting.
					 */

					dx_set_error_code(dx_pec_invalid_message_length);
					dx_set_in_buffer_position(context->bicc, context->buffer_size);
					dx_logging_last_error();

					process_result = false;

					break;
				} else if (dx_get_error_code() == dx_pec_unexpected_message_type) {
					/* Unknown message type, skipping */

					dx_set_in_buffer_position(context->bicc, dx_get_in_buffer_limit(context->bicc));
					dx_logging_last_error();
					dx_set_error_code(dx_ec_success);

					continue;
				} else {
					/*
					 *	Some other sort of corruption, unrecoverable
					 */

					dx_set_in_buffer_position(context->bicc, context->buffer_size);
					dx_logging_last_error();

					process_result = false;

					break;
				}
			}
		}

		if (!dx_process_message(context, message_type)) {
			switch (dx_get_error_code()) {
			case dx_pec_server_message_not_supported:
				/* skipping the message */

				dx_set_in_buffer_position(context->bicc, dx_get_in_buffer_limit(context->bicc));
				dx_set_error_code(dx_ec_success);

				continue;
			default:
				/*
				 *	Unrecoverable error
				 */

				dx_set_in_buffer_position(context->bicc, context->buffer_size);
				dx_logging_last_error();

				process_result = false;

				break;
			}
		}
	}

	context->buffer_pos = dx_get_in_buffer_position(context->bicc);

	return process_result;
}

/* -------------------------------------------------------------------------- */
/*
 *	Low level socket data receiver implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_socket_data_receiver (dxf_connection_t connection, const void* buffer, int buffer_size) {
	bool conn_ctx_res = true;
	dx_server_msg_proc_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_server_msg_processor, &conn_ctx_res);

	if (context != NULL && context->raw_dump_file_name != NULL) {
		FILE* raw_out = fopen(context->raw_dump_file_name, "ab+");
		fwrite(buffer, 1, buffer_size, raw_out);
		fclose(raw_out);
	}

	return dx_process_server_data(connection, buffer, buffer_size);
}

/* -------------------------------------------------------------------------- */
/*
 *	Records digest management
 */
/* -------------------------------------------------------------------------- */

bool dx_add_record_digest_to_list(dxf_connection_t connection, dx_record_id_t index) {
	bool failed = false;
	dx_record_digest_t new_digest;
	dx_server_msg_proc_connection_context_t* mpcc = dx_get_subsystem_data(connection, dx_ccs_server_msg_processor, &failed);
	if (mpcc == NULL) {
		if (failed) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}
		return false;
	}

	if (index < (dx_record_id_t)(mpcc->record_digests.size)) {
		/* Digest with such index already exist, don't insert new record*/
		return true;
	}

	dx_init_record_digest(&new_digest);
	DX_ARRAY_INSERT(mpcc->record_digests, dx_record_digest_t, new_digest, index, dx_capacity_manager_halfer, failed);

	if (failed) {
		return dx_set_error_code(dx_sec_not_enough_memory);
	}

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Start dumping incoming raw data into specific file
 */
/* -------------------------------------------------------------------------- */

bool dx_add_raw_dump_file(dxf_connection_t connection, const char * raw_file_name)
{
	FILE* raw_out = fopen(raw_file_name, "wb");
	if (raw_out == NULL) {
		dx_set_error_code(dx_lec_failed_to_open_file);
		return false;
	}
	fclose(raw_out);
	{
		bool conn_ctx_res = true;
		dx_server_msg_proc_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_server_msg_processor, &conn_ctx_res);

		CHECKED_FREE(context->raw_dump_file_name);
		context->raw_dump_file_name = dx_calloc(strlen(raw_file_name) + 1, sizeof(char));
		if (context->raw_dump_file_name == NULL) {
			dx_set_error_code(dx_mec_insufficient_memory);
			return false;
		}
		strcpy(context->raw_dump_file_name, raw_file_name);
	}
	return true;
}
