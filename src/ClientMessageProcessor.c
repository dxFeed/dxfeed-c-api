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

typedef enum {
	dx_at_add_subscription,
	dx_at_remove_subscription,

	/* add new action types above this line */

	dx_at_count
} dx_action_type_t;

dx_message_type_t dx_get_subscription_message_type (dx_action_type_t action_type, dx_subscription_type_t subscr_type) {
	static dx_message_type_t subscr_message_types[dx_at_count][dx_st_count] = {
		{ MESSAGE_TICKER_ADD_SUBSCRIPTION, MESSAGE_STREAM_ADD_SUBSCRIPTION, MESSAGE_HISTORY_ADD_SUBSCRIPTION },
		{ MESSAGE_TICKER_REMOVE_SUBSCRIPTION, MESSAGE_STREAM_REMOVE_SUBSCRIPTION, MESSAGE_HISTORY_REMOVE_SUBSCRIPTION }
	};

	return subscr_message_types[action_type][subscr_type];
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
	int i = 0;

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

/* -------------------------------------------------------------------------- */

void* dx_create_event_subscription_task_data (dxf_connection_t connection, dx_order_source_array_ptr_t order_source,
											dxf_const_string_t* symbols, size_t symbol_count, int event_types,
											bool unsubscribe, dxf_uint_t subscr_flags, dxf_long_t time) {
	size_t i = 0;
	dx_event_subscription_task_data_t* data = dx_calloc(1, sizeof(dx_event_subscription_task_data_t));

	if (data == NULL) {
		return NULL;
	}

	data->symbols = dx_calloc(symbol_count, sizeof(dxf_const_string_t));

	if (data->symbols == NULL) {
		return dx_destroy_event_subscription_task_data(data);
	}

	data->symbol_count = symbol_count;

	for (; i < symbol_count; ++i) {
		if ((data->symbols[i] = dx_create_string_src(symbols[i])) == NULL) {
			return dx_destroy_event_subscription_task_data(data);
		}
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
										dxf_int_t record_id, dxf_long_t time) {
	static const dxf_int_t initial_buffer_size = 100;

	void* bocc = NULL;
	void* dscc = NULL;
	dxf_byte_t* subscr_buffer = NULL;
	int message_size = 0;
	dxf_long_t subscription_time;

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

	if (!dx_unlock_buffered_output(bocc) ||
		!dx_send_data(connection, subscr_buffer, message_size)) {

		dx_free(subscr_buffer);

		return false;
	}

	dx_free(subscr_buffer);

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

#ifdef _DEBUG
	dx_logging_dbg_lock();
	dx_logging_dbg(L"SENDRECORDS Send records [%u, %u)", record_id, count);
	dx_logging_dbg_stack();
	dx_logging_dbg_unlock();
#endif
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

bool dx_get_event_server_support (dxf_connection_t connection, dx_order_source_array_ptr_t order_source,
								int event_types, bool unsubscribe, dxf_uint_t subscr_flags,
								OUT dx_message_support_status_t* res) {
	dx_event_id_t eid = dx_eid_begin;
	bool halt = false;
	bool success = true;

	*res = dx_mss_pending;

	CHECKED_CALL_2(dx_lock_describe_protocol_processing, connection, true);

	for (; eid < dx_eid_count; ++eid) {
		if (event_types & DX_EVENT_BIT_MASK(eid)) {
			size_t j = 0;
			dx_event_subscription_param_list_t subscr_params;
			size_t param_count = dx_get_event_subscription_params(connection, order_source, eid, subscr_flags, &subscr_params);

			for (; j < param_count; ++j) {
				const dx_event_subscription_param_t* cur_param = subscr_params.elements + j;
				dx_message_type_t msg_type = dx_get_subscription_message_type(unsubscribe ? dx_at_remove_subscription : dx_at_add_subscription, cur_param->subscription_type);
				dx_message_support_status_t msg_res;

				if (!dx_is_message_supported_by_server(connection, msg_type, false, &msg_res)) {
					halt = true;
					success = false;

					break;
				}

				if (msg_res != *res) {
					if (*res == dx_mss_pending) {
						*res = msg_res;
					} else {
						if (msg_res == dx_mss_pending) {
							/* combination of messages with known and unknown support is forbidden */

							dx_set_error_code(dx_pec_inconsistent_message_support);

							halt = true;
							success = false;

							break;
						}

						/* combination of supported and unsupported messages means that the server
						does not fully support our requirements, hence the overall 'unsupported' result */

						*res = dx_mss_not_supported;

						halt = true;

						break;
					}
				}
			}

			if (halt) {
				break;
			}

			dx_free(subscr_params.elements);
		}
	}

	return (dx_lock_describe_protocol_processing(connection, false) && success);
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
	size_t i = 0;

	CHECKED_CALL_2(dx_validate_connection_handle, connection, true);

	{
		dx_message_support_status_t msg_support_status;

		if (!dx_get_event_server_support(connection, order_source, event_types, unsubscribe, subscr_flags, &msg_support_status)) {
			return false;
		}

		switch (msg_support_status) {
		case dx_mss_not_supported:
			return dx_set_error_code(dx_pec_local_message_not_supported_by_server);
		case dx_mss_pending:
			if (task_mode) {
				/* this task must never be executed unless it's clear whether the message is supported */
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

	for (; i < symbol_count; ++i){
		dx_event_id_t eid = dx_eid_begin;

		for (; eid < dx_eid_count; ++eid) {
			if (event_types & DX_EVENT_BIT_MASK(eid)) {
				size_t j = 0;
				dx_event_subscription_param_list_t subscr_params;
				size_t param_count = dx_get_event_subscription_params(connection, order_source, eid,
					subscr_flags, &subscr_params);

				for (; j < param_count; ++j) {
					const dx_event_subscription_param_t* cur_param = subscr_params.elements + j;
					dx_message_type_t msg_type = dx_get_subscription_message_type(unsubscribe ? dx_at_remove_subscription : dx_at_add_subscription, cur_param->subscription_type);

					if (!dx_subscribe_symbol_to_record(connection, msg_type, symbols[i],
													dx_encode_symbol_name(symbols[i]),
													cur_param->record_id, time)) {

						return false;
					}
				}

				dx_free(subscr_params.elements);
			}
		}
	}

	return true;
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