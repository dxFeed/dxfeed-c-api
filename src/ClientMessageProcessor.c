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

#include "DXFeed.h"

#include "ClientMessageProcessor.h"
#include "BufferedOutput.h"
#include "DataStructures.h"
#include "DXMemory.h"
#include "SymbolCodec.h"
#include "Logger.h"
#include "DXNetwork.h"
#include "DXPMessageData.h"
#include "DXAlgorithms.h"
#include "DXErrorHandling.h"
#include "ConnectionContextData.h"
#include "EventData.h"
#include "ServerMessageProcessor.h"
#include "TaskQueue.h"
#include "Version.h"

/* -------------------------------------------------------------------------- */
/*
 *	Event subscription auxiliary stuff
 */
/* -------------------------------------------------------------------------- */

bool dx_to_subscription_message_type(bool subscribe, dx_subscription_type_t subscr_type, OUT dx_message_type_t* res) {
	if (res == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}
	switch (subscr_type) {
	case dx_st_ticker:
		*res = subscribe ? MESSAGE_TICKER_ADD_SUBSCRIPTION : MESSAGE_TICKER_REMOVE_SUBSCRIPTION;
		break;
	case dx_st_stream:
		*res = subscribe ? MESSAGE_STREAM_ADD_SUBSCRIPTION : MESSAGE_STREAM_REMOVE_SUBSCRIPTION;
		break;
	case dx_st_history:
		*res = subscribe ? MESSAGE_HISTORY_ADD_SUBSCRIPTION : MESSAGE_HISTORY_REMOVE_SUBSCRIPTION;
		break;
	default:
		dx_logging_info(L"Unknown dx_subscription_type_t: %d", subscr_type);
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	};
	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Event subscription task stuff
 */
/* -------------------------------------------------------------------------- */

typedef struct {
	dxf_connection_t connection;
	dx_order_source_array_t order_source;
	dxf_const_string_t* symbols;
	size_t symbol_count;
	int event_types;
	bool unsubscribe;
	dxf_uint_t subscr_flags;
	dxf_long_t time;
} dx_event_subscription_task_data_t;

/* -------------------------------------------------------------------------- */

void* dx_destroy_event_subscription_task_data (dx_event_subscription_task_data_t* data) {
	if (data == NULL) {
		return NULL;
	}

	FREE_ARRAY(data->symbols, data->symbol_count);

	dx_free(data->order_source.elements);
	data->order_source.elements = NULL;
	data->order_source.size = 0;
	data->order_source.capacity = 0;

	dx_free(data);

	return NULL;
}

size_t dx_count_symbols(dxf_const_string_t *symbols, size_t symbol_count) {
	size_t count = 0;

	for (size_t i = 0; i < symbol_count; i++) {
		if (dx_string_null_or_empty(symbols[i])) {
			continue;
		}

		count++;
	}

	return count;
}

/* -------------------------------------------------------------------------- */

void* dx_create_event_subscription_task_data (dxf_connection_t connection, dx_order_source_array_ptr_t order_source,
											dxf_const_string_t* symbols, size_t symbol_count, int event_types,
											bool unsubscribe, dxf_uint_t subscr_flags, dxf_long_t time) {
	dx_event_subscription_task_data_t* data = dx_calloc(1, sizeof(dx_event_subscription_task_data_t));

	if (data == NULL) {
		return NULL;
	}

	size_t count = dx_count_symbols(symbols, symbol_count);
	data->symbols = dx_calloc(count, sizeof(dxf_const_string_t));

	if (data->symbols == NULL) {
		return dx_destroy_event_subscription_task_data(data);
	}

	data->symbol_count = count;

	for (size_t i = 0, data_symbol_index = 0; i < symbol_count; ++i) {
		if (dx_string_null_or_empty(symbols[i])) {
			continue;
		}

		dxf_string_t symbol = dx_create_string_src(symbols[i]);

		if (symbol == NULL) {
			return dx_destroy_event_subscription_task_data(data);
		}

		data->symbols[data_symbol_index] = symbol;
		data_symbol_index++;
	}

	if (order_source->size > 0) {
		data->order_source.elements = dx_calloc(order_source->size, sizeof(dx_suffix_t));
		if (data->order_source.elements == NULL)
			return dx_destroy_event_subscription_task_data(data);
		dx_memcpy(data->order_source.elements, order_source->elements, order_source->size * sizeof(dx_suffix_t));
		data->order_source.size = order_source->size;
		data->order_source.capacity = order_source->capacity;
	}

	data->connection = connection;
	data->event_types = event_types;
	data->unsubscribe = unsubscribe;
	data->subscr_flags = subscr_flags;
	data->time = time;

	return data;
}

/* -------------------------------------------------------------------------- */
/*
 *	Common helpers
 */
/* -------------------------------------------------------------------------- */

static bool dx_compose_message_header (void* bocc, dx_message_type_t message_type) {
	CHECKED_CALL_2(dx_write_byte, bocc, (dxf_byte_t)0); /* reserve one byte for message length */
	CHECKED_CALL_2(dx_write_compact_int, bocc, message_type);

	return true;
}
static bool dx_compose_heartbeat (void* bocc) {
	CHECKED_CALL_2(dx_write_byte, bocc, (dxf_byte_t)0); /* reserve one byte for message length */

	return true;
}
/* -------------------------------------------------------------------------- */

static bool dx_move_message_data (void* bocc, int old_offset, int new_offset, int data_length) {
	CHECKED_CALL_2(dx_ensure_capacity, bocc, new_offset + data_length);

	dx_memmove(dx_get_out_buffer(bocc) + new_offset, dx_get_out_buffer(bocc) + old_offset, data_length);

	return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_finish_composing_message (void* bocc) {
	int message_length = dx_get_out_buffer_position(bocc) - 1; /* 1 is for the one byte reserved for size */
	int length_size = dx_get_compact_size(message_length);

	if (length_size > 1) {
		/* Only one byte was initially reserved. Moving the message buffer */

		CHECKED_CALL_4(dx_move_message_data, bocc, 1, length_size, message_length);
	}

	dx_set_out_buffer_position(bocc, 0);
	CHECKED_CALL_2(dx_write_compact_int, bocc, message_length);
	dx_set_out_buffer_position(bocc, length_size + message_length);

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Symbol subscription helpers
 */
/* -------------------------------------------------------------------------- */

static bool dx_compose_body (void* bocc, dxf_int_t record_id, dxf_int_t cipher, dxf_const_string_t symbol) {
	if (cipher == 0 && symbol == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	CHECKED_CALL_3(dx_codec_write_symbol, bocc, cipher, symbol);
	CHECKED_CALL_2(dx_write_compact_int, bocc, record_id);

	return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_subscribe_symbol_to_record(dxf_connection_t connection, dx_message_type_t type,
										dxf_const_string_t symbol, dxf_int_t cipher,
										dxf_int_t record_id, dxf_long_t time, OUT dxf_byte_t** buffer, OUT int* buffer_size) {
	static const dxf_int_t initial_buffer_size = 100;

	void* bocc = NULL;
	void* dscc = NULL;
	dxf_byte_t* subscr_buffer = NULL;
	int message_size = 0;
	dxf_long_t subscription_time;
	*buffer_size = 0;
	*buffer = NULL;

	bocc = dx_get_buffered_output_connection_context(connection);
	dscc = dx_get_data_structures_connection_context(connection);

	if (bocc == NULL || dscc == NULL) {
		return dx_set_error_code(dx_cec_connection_context_not_initialized);
	}

	if (!dx_is_subscription_message(type)) {
		return dx_set_error_code(dx_pec_unexpected_message_type_internal);
	}

	CHECKED_CALL(dx_lock_buffered_output, bocc);

	subscr_buffer = (dxf_byte_t*)dx_malloc(initial_buffer_size);

	if (subscr_buffer == NULL) {
		dx_unlock_buffered_output(bocc);

		return false;
	}

	dx_set_out_buffer(bocc, subscr_buffer, initial_buffer_size);

	if (!dx_compose_message_header(bocc, type) ||
		!dx_compose_body(bocc, record_id, cipher, symbol) ||
		((type == MESSAGE_HISTORY_ADD_SUBSCRIPTION) && !dx_create_subscription_time(dscc, record_id, time, OUT &subscription_time)) ||
		((type == MESSAGE_HISTORY_ADD_SUBSCRIPTION) && !dx_write_compact_long(bocc, subscription_time)) ||
		!dx_finish_composing_message(bocc)) {

		dx_free(dx_get_out_buffer(bocc));
		dx_unlock_buffered_output(bocc);

		return false;
	}

	subscr_buffer = dx_get_out_buffer(bocc);
	message_size = dx_get_out_buffer_position(bocc);

	if (!dx_unlock_buffered_output(bocc)) {
		dx_free(subscr_buffer);
		return false;
	}
	if (subscr_buffer != NULL) {
		if (message_size > 0) {
			*buffer = subscr_buffer;
			*buffer_size = message_size;
		}
		else {
			dx_free(subscr_buffer);
		}
	}
	return true;
}

/* -------------------------------------------------------------------------- */

int dx_subscribe_symbols_to_events_task (void* data, int command) {
	dx_event_subscription_task_data_t* task_data = data;
	int res = dx_tes_pop_me;

	if (task_data == NULL) {
		return res;
	}

	if (IS_FLAG_SET(command, dx_tc_free_resources)) {
		dx_destroy_event_subscription_task_data(task_data);

		return res | dx_tes_success;
	}

	if (dx_subscribe_symbols_to_events(task_data->connection, &(task_data->order_source),
									task_data->symbols, task_data->symbol_count,
									task_data->event_types, task_data->unsubscribe,
									true, task_data->subscr_flags, task_data->time)) {
		res |= dx_tes_success;
	}

	dx_destroy_event_subscription_task_data(task_data);

	return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	Describe record helpers
 */
/* -------------------------------------------------------------------------- */

static bool dx_write_record_field (void* bocc, const dx_field_info_t* field) {
	CHECKED_CALL_2(dx_write_utf_string, bocc, field->name);
	CHECKED_CALL_2(dx_write_compact_int, bocc, field->type);

	return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_write_event_record(void* bocc, const dx_record_item_t* record, dx_record_id_t record_id) {
	int field_index = 0;

	CHECKED_CALL_2(dx_write_compact_int, bocc, (dxf_int_t)record_id);
	CHECKED_CALL_2(dx_write_utf_string, bocc, record->name);
	CHECKED_CALL_2(dx_write_compact_int, bocc, (dxf_int_t)record->field_count);

	for (; field_index != record->field_count; ++field_index) {
		CHECKED_CALL_2(dx_write_record_field, bocc, record->fields + field_index);
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_write_event_records (void* bocc, void* dscc) {
	dx_record_id_t record_id = dx_get_next_unsubscribed_record_id(dscc, false);
	dx_record_id_t count = dx_get_records_list_count(dscc);

	while (record_id < count) {
		CHECKED_CALL_3(dx_write_event_record, bocc, dx_get_record_by_id(dscc, record_id), record_id);
		record_id = dx_get_next_unsubscribed_record_id(dscc, true);
	}

	return true;
}

/* -------------------------------------------------------------------------- */

int dx_describe_records_sender_task (void* data, int command) {
	int res = dx_tes_pop_me;

	if (IS_FLAG_SET(command, dx_tc_free_resources)) {
		return res | dx_tes_success;
	}

	if (dx_send_record_description((dxf_connection_t)data, true)) {
		res |= dx_tes_success;
	}

	return res;
}

/* -------------------------------------------------------------------------- */
bool dx_load_events_for_subscription (dxf_connection_t connection, dx_order_source_array_ptr_t order_source,
										int event_types, dxf_uint_t subscr_flags) {
	dx_event_id_t eid = dx_eid_begin;
	for (; eid < dx_eid_count; ++eid) {
		if (event_types & DX_EVENT_BIT_MASK(eid)) {
			dx_event_subscription_param_list_t subscr_params;
			dx_get_event_subscription_params(connection, order_source, eid, subscr_flags, &subscr_params);
			dx_free(subscr_params.elements);
		}
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_get_event_server_support(dxf_connection_t connection, dx_order_source_array_ptr_t order_source,
	int event_types, bool unsubscribe, dxf_uint_t subscr_flags,
	OUT dx_message_support_status_t* res)
{
	*res = dx_mss_supported;
	CHECKED_CALL_2(dx_lock_describe_protocol_processing, connection, true);
	bool go_to_exit = false;
	bool success = true;
	for (dx_event_id_t eid = dx_eid_begin; eid < dx_eid_count && !go_to_exit; ++eid) {
		if (event_types & DX_EVENT_BIT_MASK(eid)) {
			size_t j = 0;
			dx_event_subscription_param_list_t subscr_params;
			size_t param_count = dx_get_event_subscription_params(connection, order_source, eid, subscr_flags, &subscr_params);
			for (; j < param_count && !go_to_exit; ++j) {
				const dx_event_subscription_param_t* cur_param = subscr_params.elements + j;
				dx_message_type_t message_type;
				if (!dx_to_subscription_message_type(!unsubscribe, cur_param->subscription_type, &message_type)) {
					go_to_exit = true;
					success = false;
					continue;
				}
				dx_message_support_status_t message_support;
				if (!dx_is_message_supported_by_server(connection, message_type, false, &message_support)) {
					go_to_exit = true;
					success = false;
					continue;
				}
				switch (message_support) {
				case dx_mss_supported:
					break;
				case dx_mss_not_supported:
				case dx_mss_pending:
				case dx_mss_reconnection:
					*res = message_support;
					go_to_exit = true;
					break;
				default:
					dx_logging_info(L"Unknown dx_message_support_status_t: %d", message_support);
					dx_set_error_code(dx_ec_internal_assert_violation);
					go_to_exit = true;
					success = false;
				}
			}
			dx_free(subscr_params.elements);
		}
	}
	if (!dx_lock_describe_protocol_processing(connection, false)) {
		return false;
	}
	return success;
}

/* -------------------------------------------------------------------------- */
/*
 *	Describe protocol helpers
 */
/* -------------------------------------------------------------------------- */

static bool dx_write_describe_protocol_magic (void* bocc) {
	/* hex value is 0x44585033 */
	CHECKED_CALL_2(dx_write_byte, bocc, (dxf_byte_t)'D');
	CHECKED_CALL_2(dx_write_byte, bocc, (dxf_byte_t)'X');
	CHECKED_CALL_2(dx_write_byte, bocc, (dxf_byte_t)'P');
	CHECKED_CALL_2(dx_write_byte, bocc, (dxf_byte_t)'3');

	return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_write_describe_protocol_properties (void* bocc, const dx_property_map_t* properties) {
	size_t i;

	if (properties == NULL)
		return false;

	CHECKED_CALL_2(dx_write_compact_int, bocc, (dxf_int_t)properties->size); /* count of properties */
	for (i = 0; i < properties->size; i++) {
		dx_property_item_t* item_ptr = properties->elements + i;
		CHECKED_CALL_2(dx_write_utf_string, bocc, item_ptr->key);
		CHECKED_CALL_2(dx_write_utf_string, bocc, item_ptr->value);
	}

	return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_write_describe_protocol_message_data (void* bocc, const int* msg_roster, int msg_count) {
	int i = 0;

	CHECKED_CALL_2(dx_write_compact_int, bocc, msg_count);

	for (; i < msg_count; ++i) {
		CHECKED_CALL_2(dx_write_compact_int, bocc, msg_roster[i]);
		CHECKED_CALL_2(dx_write_utf_string, bocc, dx_get_message_type_name(msg_roster[i]));
		CHECKED_CALL_2(dx_write_compact_int, bocc, 0); /* number of message properties */
	}

	return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_write_describe_protocol_sends (void* bocc) {
	return dx_write_describe_protocol_message_data(bocc, dx_get_send_message_roster(), dx_get_send_message_roster_size());
}

/* -------------------------------------------------------------------------- */

static bool dx_write_describe_protocol_recvs (void* bocc) {
	return dx_write_describe_protocol_message_data(bocc, dx_get_recv_message_roster(), dx_get_recv_message_roster_size());
}

/* -------------------------------------------------------------------------- */

int dx_heartbeat_sender_task (void* data, int command) {
	int res = dx_tes_pop_me;

	if (IS_FLAG_SET(command, dx_tc_free_resources)) {
		return res | dx_tes_success;
	}

	if (dx_send_heartbeat((dxf_connection_t)data, true)) {
		res |= dx_tes_success;
	}

	return res;
}
int dx_describe_protocol_sender_task (void* data, int command) {
	int res = dx_tes_pop_me;

	if (IS_FLAG_SET(command, dx_tc_free_resources)) {
		return res | dx_tes_success;
	}

	if (dx_send_protocol_description((dxf_connection_t)data, true)) {
		res |= dx_tes_success;
	}

	return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	External interface
 */
/* -------------------------------------------------------------------------- */

bool dx_subscribe_symbols_to_events (dxf_connection_t connection, dx_order_source_array_ptr_t order_source,
									dxf_const_string_t* symbols, size_t symbol_count, int event_types, bool unsubscribe,
									bool task_mode, dxf_uint_t subscr_flags, dxf_long_t time) {
	CHECKED_CALL_2(dx_validate_connection_handle, connection, true);

	{
		dx_message_support_status_t msg_support_status;

		if (!dx_get_event_server_support(connection, order_source, event_types, unsubscribe, subscr_flags, &msg_support_status)) {
			return false;
		}

		switch (msg_support_status) {
		case dx_mss_not_supported:
			return dx_set_error_code(dx_pec_local_message_not_supported_by_server);
		case dx_mss_reconnection:
			/* actual subscription will be sent after reconnection*/
			return true;
		case dx_mss_pending:
			if (task_mode) {
				dx_logging_info(L"Protocol timeout countdown task is complete but status is %d. We shouldn't be here.", msg_support_status);
				return dx_set_error_code(dx_ec_internal_assert_violation);
			}
		default:
			if (!task_mode) {
				/* scheduling the task for asynchronous execution */

				void* data = dx_create_event_subscription_task_data(connection, order_source, symbols,
					symbol_count, event_types, unsubscribe, subscr_flags, time);

				if (data == NULL) {
					return false;
				}

				return dx_add_worker_thread_task(connection, dx_subscribe_symbols_to_events_task, data);
			}
		}
	}

	/* executing in task mode (in background queue thread)*/
	bool success = true;
	for (size_t i = 0; i < symbol_count && success; ++i) {
		if (dx_string_null_or_empty(symbols[i])) {
			continue;
		}

		dxf_byte_t* buffer = NULL;
		int buffer_size = 0;
		int buffer_capacity = 0;
		for (dx_event_id_t eid = dx_eid_begin; eid < dx_eid_count && success; ++eid) {
			if (event_types & DX_EVENT_BIT_MASK(eid)) {
				dx_event_subscription_param_list_t subscr_params;
				size_t param_count = dx_get_event_subscription_params(connection, order_source, eid, subscr_flags, &subscr_params);
				for (size_t j = 0; j < param_count; ++j) {
					const dx_event_subscription_param_t* cur_param = subscr_params.elements + j;
					dx_message_type_t msg_type;
					if (!dx_to_subscription_message_type(!unsubscribe, cur_param->subscription_type, &msg_type)) {
						success = false;
						break;
					}
					dxf_byte_t* param_buffer = NULL;
					int param_buffer_size = 0;
					if (!dx_subscribe_symbol_to_record(connection, msg_type, symbols[i],
													dx_encode_symbol_name(symbols[i]),
													cur_param->record_id, time, &param_buffer, &param_buffer_size)) {
						success = false;
						break;
					}
					if (param_buffer != NULL) {
						if (buffer != NULL) {
							if (buffer_capacity < buffer_size + param_buffer_size) {
								dxf_byte_t* _temp = buffer;
								buffer_capacity = (buffer_size + param_buffer_size) << 1;
								buffer = (dxf_byte_t*)dx_malloc(buffer_capacity);
								dx_memcpy(buffer, _temp, buffer_size);
								dx_free(_temp);
							}
							dx_memcpy(buffer + buffer_size, param_buffer, param_buffer_size);
							buffer_size += param_buffer_size;
							dx_free(param_buffer);
						}
						else {
							buffer = param_buffer;
							buffer_size = param_buffer_size;
							buffer_capacity = param_buffer_size;
						}
					}
				}
				dx_free(subscr_params.elements);
			}
		}
		if (buffer != NULL) {
			if (success) {
				if (!dx_send_data(connection, buffer, buffer_size)) {
					success = false;
				}
			}
			dx_free(buffer);
		}
	}
	return success;
}

/* -------------------------------------------------------------------------- */

bool dx_send_record_description (dxf_connection_t connection, bool task_mode) {
	static const int initial_size = 1024;

	void* bocc = NULL;
	void* dscc = NULL;
	dxf_byte_t* initial_buffer = NULL;
	int message_size = 0;

	CHECKED_CALL_2(dx_validate_connection_handle, connection, true);

	if (!task_mode) {
		/* scheduling the task for asynchronous execution */
		return dx_add_worker_thread_task(connection, dx_describe_records_sender_task, (void*)connection);
	}

	bocc = dx_get_buffered_output_connection_context(connection);
	dscc = dx_get_data_structures_connection_context(connection);

	if (bocc == NULL || dscc == NULL) {
		return dx_set_error_code(dx_cec_connection_context_not_initialized);
	}

	CHECKED_CALL(dx_lock_buffered_output, bocc);

	initial_buffer = dx_malloc(initial_size);

	if (initial_buffer == NULL) {
		dx_unlock_buffered_output(bocc);

		return false;
	}

	dx_set_out_buffer(bocc, initial_buffer, initial_size);

	if (!dx_compose_message_header(bocc, MESSAGE_DESCRIBE_RECORDS) ||
		!dx_write_event_records(bocc, dscc) ||
		!dx_finish_composing_message(bocc)) {

		dx_free(dx_get_out_buffer(bocc));
		dx_unlock_buffered_output(bocc);

		return false;
	}

	initial_buffer = dx_get_out_buffer(bocc);
	message_size = dx_get_out_buffer_position(bocc);

	if (!dx_unlock_buffered_output(bocc) ||
		!dx_send_data(connection, initial_buffer, message_size)) {

		dx_free(initial_buffer);

		return false;
	}

	dx_free(initial_buffer);

	return true;
}
/* -------------------------------------------------------------------------- */

bool dx_send_protocol_description (dxf_connection_t connection, bool task_mode) {
	static const int initial_size = 1024;

	void* bocc = NULL;
	dxf_byte_t* initial_buffer = NULL;
	int message_size = 0;

	CHECKED_CALL_2(dx_validate_connection_handle, connection, true);

	if (!task_mode) {
		/* scheduling the task for asynchronous execution */
		if (!dx_add_worker_thread_task(connection, dx_describe_protocol_sender_task, (void*)connection) ||
			!dx_describe_protocol_sent(connection)) {

			return false;
		}

		return true;
	}

	// set default protocol properties values
	if (!dx_protocol_property_set(connection, L"version", DX_LIBRARY_VERSION) ||
		!dx_protocol_property_set(connection, L"opt", L"hs")) {

		return false;
	}

	bocc = dx_get_buffered_output_connection_context(connection);

	if (bocc == NULL) {
		return dx_set_error_code(dx_cec_connection_context_not_initialized);
	}

	CHECKED_CALL(dx_lock_buffered_output, bocc);

	initial_buffer = dx_malloc(initial_size);

	if (initial_buffer == NULL) {
		dx_unlock_buffered_output(bocc);

		return false;
	}

	dx_set_out_buffer(bocc, initial_buffer, initial_size);

	if (!dx_compose_message_header(bocc, MESSAGE_DESCRIBE_PROTOCOL) ||
		!dx_write_describe_protocol_magic(bocc) ||
		!dx_write_describe_protocol_properties(bocc, dx_protocol_property_get_all(connection)) ||
		!dx_write_describe_protocol_sends(bocc) ||
		!dx_write_describe_protocol_recvs(bocc) ||
		!dx_finish_composing_message(bocc)) {

		dx_free(dx_get_out_buffer(bocc));
		dx_unlock_buffered_output(bocc);

		return false;
	}

	initial_buffer = dx_get_out_buffer(bocc);
	message_size = dx_get_out_buffer_position(bocc);

	if (!dx_unlock_buffered_output(bocc) ||
		!dx_send_data(connection, initial_buffer, message_size)) {

		dx_free(initial_buffer);

		return false;
	}

	dx_free(initial_buffer);

	return true;
}

bool dx_send_heartbeat (dxf_connection_t connection, bool task_mode) {
	static const int initial_size = 1024;

	void* bocc = NULL;
	dxf_byte_t* initial_buffer = NULL;
	int message_size = 0;

	CHECKED_CALL_2(dx_validate_connection_handle, connection, true);

	if (!task_mode) {
		/* scheduling the task for asynchronous execution */
		if (!dx_add_worker_thread_task(connection, dx_heartbeat_sender_task, (void*)connection) ) {

			return false;
		}

		return true;
	}

	bocc = dx_get_buffered_output_connection_context(connection);

	if (bocc == NULL) {
		return dx_set_error_code(dx_cec_connection_context_not_initialized);
	}

	CHECKED_CALL(dx_lock_buffered_output, bocc);

	initial_buffer = dx_malloc(initial_size);

	if (initial_buffer == NULL) {
		dx_unlock_buffered_output(bocc);

		return false;
	}

	dx_set_out_buffer(bocc, initial_buffer, initial_size);

	if (!dx_compose_heartbeat(bocc)) {

		dx_free(dx_get_out_buffer(bocc));
		dx_unlock_buffered_output(bocc);

		return false;
	}

	initial_buffer = dx_get_out_buffer(bocc);
	message_size = dx_get_out_buffer_position(bocc);

	if (!dx_unlock_buffered_output(bocc) ||
		!dx_send_data(connection, initial_buffer, message_size)) {

		dx_free(initial_buffer);

		return false;
	}

	dx_free(initial_buffer);

	return true;
}