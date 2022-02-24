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

#include "ConnectionContextData.h"
#include "DXAlgorithms.h"
#include "DXErrorCodes.h"
#include "DXErrorHandling.h"
#include "DXThreads.h"
#include "EventManager.h"
#include "EventSubscription.h"
#include "Logger.h"
#include "Snapshot.h"

/* -------------------------------------------------------------------------- */
/*
 *	Internal data structures and defines
 */
/* -------------------------------------------------------------------------- */

typedef enum {
	dx_status_unknown = 0,
	dx_status_begin,
	dx_status_full,
	dx_status_pending
} dx_snapshot_status_t;

typedef struct {
	int incremental;
	int full_snapshot_seen;
	union {
		dxf_snapshot_listener_t full_listener;
		dxf_snapshot_inc_listener_t inc_listener;
	};
	void* user_data;
} dx_snapshot_listener_context_t;

typedef struct {
	dx_snapshot_listener_context_t* elements;
	size_t size;
	size_t capacity;
} dx_snapshot_listener_array_t;

typedef struct {
	dxf_event_data_t elements;
	size_t size;
	size_t capacity;
} dx_snapshot_records_array_t;

typedef struct {
	dxf_time_int_field_t time_int_field;
	dxf_long_t index;
} dx_snapshot_record_key_t;

typedef struct {
	dx_snapshot_record_key_t* elements;
	size_t size;
	size_t capacity;
} dx_snapshot_record_keys_t;

typedef struct {
	dx_string_array_ptr_t* elements;
	size_t size;
	size_t capacity;
} dx_snapshot_record_buffers_t;

typedef struct {
	/* Store received records */
	dx_snapshot_records_array_t records;
	/* Store keys of snapshot records */
	dx_snapshot_record_keys_t record_keys;
	/* Stores a string for snapshot records */
	dx_snapshot_record_buffers_t record_buffers;
	/* position of record == position of record_time_ints == position of record_buffer */
} dx_snapshot_records_t, *dx_snapshot_records_ptr_t;

typedef struct {
	dx_mutex_t guard;

	dxf_ulong_t key;
	dxf_subscription_t subscription;
	dx_record_info_id_t record_info_id;
	dx_event_id_t event_id;
	int event_type;
	dxf_string_t order_source;
	dxf_string_t symbol;
	dxf_long_t time;
	dx_snapshot_status_t status;

	int full_snapshot_published;
	dx_snapshot_records_t snapshot_records;
	dx_snapshot_records_t last_tx_records;

	dx_snapshot_listener_array_t listeners;
	int has_inc_listeners;

	void* sscc;
} dx_snapshot_data_t, *dx_snapshot_data_ptr_t;

/* Snapshots are sorted in ascending order by dx_snapshot_data_ptr_t->key
 * Use DX_ARRAY_BINARY_SEARCH with dx_snapshot_comparator for searching elements and before
 * inserting new once
 */
typedef struct {
	dx_snapshot_data_ptr_t* elements;
	size_t size;
	size_t capacity;
} dx_snapshots_data_array_t;

const dxf_snapshot_t dx_invalid_snapshot = (dxf_snapshot_t)NULL;

#define SYMBOL_COUNT 1

#define SNAPSHOT_KEY_SOURCE_MASK 0xFFFFFFu

/* -------------------------------------------------------------------------- */
/*
 *	Event subscription connection context
 */
/* -------------------------------------------------------------------------- */

typedef struct {
	dxf_connection_t connection;
	dx_mutex_t guard;
	dx_snapshots_data_array_t snapshots_array;
	int fields_flags;
} dx_snapshot_subscription_connection_context_t;

#define GUARD_FIELD_FLAG    (0x1)

#define CTX(context) \
	((dx_snapshot_subscription_connection_context_t*)context)

/* -------------------------------------------------------------------------- */

int dx_clear_snapshot_subscription_connection_context(dx_snapshot_subscription_connection_context_t* context);
int dx_free_snapshot_data(dx_snapshot_data_ptr_t snapshot_data);

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_snapshot_subscription) {
	dx_snapshot_subscription_connection_context_t* context = NULL;

	CHECKED_CALL_2(dx_validate_connection_handle, connection, true);

	context = dx_calloc(1, sizeof(dx_snapshot_subscription_connection_context_t));

	if (context == NULL) {
		return false;
	}

	context->connection = connection;

	if (!dx_mutex_create(&context->guard)) {
		dx_clear_snapshot_subscription_connection_context(context);

		return false;
	}

	context->fields_flags |= GUARD_FIELD_FLAG;

	if (!dx_set_subsystem_data(connection, dx_ccs_snapshot_subscription, context)) {
		dx_clear_snapshot_subscription_connection_context(context);

		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_snapshot_subscription) {
	int res = true;
	dx_snapshot_subscription_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_snapshot_subscription, &res);

	if (context == NULL) {
		return res;
	}

	return dx_clear_snapshot_subscription_connection_context(context);
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_snapshot_subscription) {
	return true;
}

/* -------------------------------------------------------------------------- */

int dx_clear_snapshot_subscription_connection_context(dx_snapshot_subscription_connection_context_t* context) {
	int res = true;
	size_t i = 0;

	for (; i < context->snapshots_array.size; ++i) {
		res = dx_free_snapshot_data((dx_snapshot_data_t*)context->snapshots_array.elements[i]) && res;
	}

	if (IS_FLAG_SET(context->fields_flags, GUARD_FIELD_FLAG)) {
		res = dx_mutex_destroy(&(context->guard)) && res;
	}

	if (context->snapshots_array.elements != NULL) {
		dx_free(context->snapshots_array.elements);
	}

	dx_free(context);

	return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	Helpers functions implementation
 */
/* -------------------------------------------------------------------------- */

int dx_snapshot_records_comparator(dxf_time_int_field_t t1, dxf_time_int_field_t t2) {
	return t1 > t2 ? -1 : (t1 < t2 ? 1 : 0);
}

int dx_snapshot_records_comparator2(dx_snapshot_record_key_t key1, dx_snapshot_record_key_t key2) {
	if (key1.time_int_field > key2.time_int_field) {
		return -1;
	} else if (key1.time_int_field < key2.time_int_field) {
		return 1;
	}

	return key1.index < key2.index ? -1 : (key1.index > key2.index ? 1 : 0);
}

int dx_set_event_flags(dxf_event_data_t* event_data, size_t index, dx_event_id_t event_id, dxf_event_flags_t event_flags) {
	switch (event_id) {
		case dx_eid_order: case dx_eid_spread_order:
			((dxf_order_t*)event_data)[index].event_flags = event_flags;

			break;
		case dx_eid_candle:
			((dxf_candle_t*)event_data)[index].event_flags = event_flags;

			break;
		case dx_eid_time_and_sale:
			((dxf_time_and_sale_t*)event_data)[index].event_flags = event_flags;

			break;
		case dx_eid_greeks:
			((dxf_greeks_t*)event_data)[index].event_flags = event_flags;

			break;
		case dx_eid_series:
			((dxf_series_t*)event_data)[index].event_flags = event_flags;

			break;
		default:
			return dx_set_error_code(dx_ssec_invalid_event_id);
	}

	return true;
}

int dx_snapshot_insert_record(dx_snapshot_data_ptr_t snapshot_data, dx_snapshot_records_ptr_t recs,
								dxf_event_data_t obj, const size_t position) {
	int failed = false;
	switch (snapshot_data->event_id) {
	case dx_eid_order:
		DX_ARRAY_INSERT(recs->records, dxf_order_t, *((dxf_order_t*)obj), position,
			dx_capacity_manager_halfer, failed);
		break;
	case dx_eid_candle:
		DX_ARRAY_INSERT(recs->records, dxf_candle_t, *((dxf_candle_t*)obj), position,
			dx_capacity_manager_halfer, failed);
		break;
	case dx_eid_spread_order:
		DX_ARRAY_INSERT(recs->records, dxf_order_t, *((dxf_order_t*)obj), position,
			dx_capacity_manager_halfer, failed);
		break;
	case dx_eid_time_and_sale:
		DX_ARRAY_INSERT(recs->records, dxf_time_and_sale_t, *((dxf_time_and_sale_t*)obj), position,
			dx_capacity_manager_halfer, failed);
		break;
	case dx_eid_greeks:
		DX_ARRAY_INSERT(recs->records, dxf_greeks_t, *((dxf_greeks_t*)obj), position,
			dx_capacity_manager_halfer, failed);
		break;
	case dx_eid_series:
		DX_ARRAY_INSERT(recs->records, dxf_series_t, *((dxf_series_t*)obj), position,
			dx_capacity_manager_halfer, failed);
		break;
	default:
		return dx_set_error_code(dx_ssec_invalid_event_id);
	}

	return !failed;
}

int dx_snapshot_delete_record(dx_snapshot_data_ptr_t snapshot_data, dx_snapshot_records_ptr_t recs,
								const size_t position) {
	int failed = false;
	switch (snapshot_data->event_id) {
	case dx_eid_order:
		DX_ARRAY_DELETE(recs->records, dxf_order_t, position,
			dx_capacity_manager_halfer, failed);
		break;
	case dx_eid_candle:
		DX_ARRAY_DELETE(recs->records, dxf_candle_t, position,
			dx_capacity_manager_halfer, failed);
		break;
	case dx_eid_spread_order:
		DX_ARRAY_DELETE(recs->records, dxf_order_t, position,
			dx_capacity_manager_halfer, failed);
		break;
	case dx_eid_time_and_sale:
		DX_ARRAY_DELETE(recs->records, dxf_time_and_sale_t, position,
			dx_capacity_manager_halfer, failed);
		break;
	case dx_eid_greeks:
		DX_ARRAY_DELETE(recs->records, dxf_greeks_t, position,
			dx_capacity_manager_halfer, failed);
		break;
	case dx_eid_series:
		DX_ARRAY_DELETE(recs->records, dxf_series_t, position,
			dx_capacity_manager_halfer, failed);
		break;
	default:
		return dx_set_error_code(dx_ssec_invalid_event_id);
	}

	return !failed;
}

int dx_snapshot_set_record(dx_snapshot_data_ptr_t snapshot_data, dx_snapshot_records_ptr_t recs,
							dxf_event_data_t obj, const size_t position) {
	switch (snapshot_data->event_id) {
	case dx_eid_order:
		dx_memcpy(&(((dxf_order_t*)recs->records.elements)[position]), (dxf_order_t*)obj,
			sizeof(dxf_order_t));
		break;
	case dx_eid_candle:
		dx_memcpy(&(((dxf_candle_t*)recs->records.elements)[position]), (dxf_candle_t*)obj,
			sizeof(dxf_candle_t));
		break;
	case dx_eid_spread_order:
		dx_memcpy(&(((dxf_order_t*)recs->records.elements)[position]), (dxf_order_t*)obj,
			sizeof(dxf_order_t));
		break;
	case dx_eid_time_and_sale:
		dx_memcpy(&(((dxf_time_and_sale_t*)recs->records.elements)[position]), (dxf_time_and_sale_t*)obj,
			sizeof(dxf_time_and_sale_t));
		break;
	case dx_eid_greeks:
		dx_memcpy(&(((dxf_greeks_t*)recs->records.elements)[position]), (dxf_greeks_t*)obj,
			sizeof(dxf_greeks_t));
		break;
	case dx_eid_series:
		dx_memcpy(&(((dxf_series_t*)recs->records.elements)[position]), (dxf_series_t*)obj,
			sizeof(dxf_series_t));
		break;
	default:
		return dx_set_error_code(dx_ssec_invalid_event_id);
	}

	return true;
}

dxf_long_t dx_get_order_index(dx_snapshot_data_ptr_t snapshot_data, const dxf_event_data_t event_data) {
	if (snapshot_data->event_id == dx_eid_order &&
		(snapshot_data->record_info_id == dx_rid_market_maker || snapshot_data->record_info_id == dx_rid_quote)) {
		return ((dxf_order_t*)event_data)->index;
	}

	return 0;
}

int dx_snapshot_add_event_record(dx_snapshot_data_ptr_t snapshot_data,
								dx_snapshot_records_ptr_t recs,
								const dxf_event_params_t* event_params,
								const dxf_event_data_t event_data, const size_t position) {
	int failed = false;
	dxf_event_data_t obj = NULL;
	dx_event_copy_function_t clone_event = dx_get_event_copy_function(snapshot_data->event_id);
	dx_event_free_function_t free_event = dx_get_event_free_function(snapshot_data->event_id);
	dxf_bool_t res = false;
	dx_string_array_ptr_t string_buffer = NULL;
	if (clone_event == NULL || free_event == NULL) {
		return false;
	}
	if (!clone_event(event_data, &string_buffer, &obj)) {
		return false;
	}

	/* store event data */
	res = dx_snapshot_insert_record(snapshot_data, recs, obj, position);
	free_event(obj);
	if (!res) {
		dx_string_array_free(string_buffer);
		dx_free((void*)string_buffer);
		return false;
	}

	/* store key */
	dx_snapshot_record_key_t key = {event_params->time_int_field, dx_get_order_index(snapshot_data, event_data)};

	DX_ARRAY_INSERT(recs->record_keys, dx_snapshot_record_key_t,
					key, position, dx_capacity_manager_halfer, failed);
	if (failed) {
		dx_string_array_free(string_buffer);
		dx_free((void*)string_buffer);
		dx_snapshot_delete_record(snapshot_data, recs, position);
		return dx_set_error_code(dx_mec_insufficient_memory);
	}

	/* store string buffer */
	DX_ARRAY_INSERT(recs->record_buffers, dx_string_array_ptr_t, string_buffer,
		position, dx_capacity_manager_halfer, failed);
	if (failed) {
		dx_string_array_free(string_buffer);
		dx_free((void*)string_buffer);
		dx_snapshot_delete_record(snapshot_data, recs, position);
		DX_ARRAY_DELETE(recs->record_keys, dx_snapshot_record_key_t, position,
						dx_capacity_manager_halfer, failed);
		return dx_set_error_code(dx_mec_insufficient_memory);
	}
	return true;
}

int dx_snapshot_clear_records_array(dx_snapshot_data_ptr_t snapshot_data, dx_snapshot_records_ptr_t recs) {
	size_t i;
	dx_snapshot_records_array_t* records;
	dx_snapshot_record_buffers_t* string_buffers;

	if (snapshot_data == NULL || recs == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	/* free records data */
	records = &(recs->records);

	dx_free(records->elements);
	records->elements = NULL;
	records->size = 0;
	records->capacity = 0;

	/* free keys */
	dx_snapshot_record_keys_t* keys = &(recs->record_keys);
	dx_free(keys->elements);
	keys->elements = NULL;
	keys->size = 0;
	keys->capacity = 0;

	/* clear records string buffers */
	string_buffers = &(recs->record_buffers);
	for (i = 0; i < string_buffers->size; ++i) {
		dx_string_array_free(string_buffers->elements[i]);
		dx_free((void*)string_buffers->elements[i]);
	}
	dx_free(string_buffers->elements);
	string_buffers->elements = NULL;
	string_buffers->size = 0;
	string_buffers->capacity = 0;

	return true;
}

int dx_snapshot_remove_event_record(dx_snapshot_data_ptr_t snapshot_data, dx_snapshot_records_ptr_t recs,
									size_t position) {
	int failed = false;

	if (snapshot_data == NULL || recs == NULL || position >= recs->records.size) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	dx_string_array_free(recs->record_buffers.elements[position]);
	dx_free((void*)recs->record_buffers.elements[position]);

	if (!dx_snapshot_delete_record(snapshot_data, recs, position)) return false;

	DX_ARRAY_DELETE(recs->record_keys, dx_snapshot_record_key_t, position, dx_capacity_manager_halfer, failed);

	if (failed) return dx_set_error_code(dx_mec_insufficient_memory);

	DX_ARRAY_DELETE(recs->record_buffers, dx_string_array_ptr_t, position, dx_capacity_manager_halfer, failed);

	if (failed) return dx_set_error_code(dx_mec_insufficient_memory);
	return true;
}

int dx_snapshot_remove_event_records(dx_snapshot_data_ptr_t snapshot_data, dx_snapshot_records_ptr_t recs,
									const dxf_event_data_t* data,
									const dxf_event_params_t* event_params) {
	if (snapshot_data == NULL || recs == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	dx_snapshot_record_keys_t* keys = &(recs->record_keys);
	dx_snapshot_record_key_t key = {event_params->time_int_field,
									dx_get_order_index(snapshot_data, (const dxf_event_data_t)data)};

	int found = false;
	size_t index = 0;

	DX_ARRAY_BINARY_SEARCH(keys->elements, 0, keys->size, key,
						   dx_snapshot_records_comparator2, found, index);

	if (found) {
		if (!dx_snapshot_remove_event_record(snapshot_data, recs, index)) {
			return false;
		}
	}

	return true;
}

int dx_snapshot_replace_record(dx_snapshot_data_ptr_t snapshot_data, dx_snapshot_records_ptr_t recs,
								size_t position, const dxf_event_data_t new_record) {
	dx_event_free_function_t free_event = NULL;
	dx_event_copy_function_t clone_event = NULL;
	dxf_event_data_t new_obj = NULL;
	int status;

	if (snapshot_data == NULL || recs == NULL || position >= recs->records.size) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	free_event = dx_get_event_free_function(snapshot_data->event_id);
	clone_event = dx_get_event_copy_function(snapshot_data->event_id);
	if (free_event == NULL || clone_event == NULL) {
		return false;
	}

	dx_string_array_free(recs->record_buffers.elements[position]);

	if (!clone_event(new_record, &(recs->record_buffers.elements[position]), &new_obj)) {
		return false;
	}

	status = dx_snapshot_set_record(snapshot_data, recs, new_obj, position);
	if (!status)
		dx_string_array_free(recs->record_buffers.elements[position]);

	free_event(new_obj);

	return status;
}

static int dx_has_size(dx_event_id_t event_id, const dxf_event_data_t* event_data) {
	if (event_id != dx_eid_order && event_id != dx_eid_spread_order)
		return true;

	const dxf_order_t* order = (const dxf_order_t*)event_data;

	return !(order->size == 0 || isnan(order->size));
}

// Updates the snapshot event records.
// Adds a REMOVE event flag for last tx records
// Clears the event flags for snapshot entries.
int dx_snapshot_update_event_records(dx_snapshot_data_ptr_t snapshot_data, dx_snapshot_records_ptr_t recs,
									 const dxf_event_data_t* data, const dxf_event_params_t* event_params,
									 int is_last_tx_records) {
	if (snapshot_data == NULL || recs == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	dx_snapshot_record_keys_t* keys = &(recs->record_keys);
	dx_snapshot_record_key_t key = {event_params->time_int_field,
									dx_get_order_index(snapshot_data, (const dxf_event_data_t)data)};

	int found = false;
	size_t index = 0;

	DX_ARRAY_BINARY_SEARCH(keys->elements, 0, keys->size, key,
						   dx_snapshot_records_comparator2, found, index);

	/* add or update record */
	if (found) {
		if (!dx_snapshot_replace_record(snapshot_data, recs, index, (const dxf_event_data_t)(data))) {
			return false;
		}
	} else {
		if (!dx_snapshot_add_event_record(snapshot_data, recs, event_params, (const dxf_event_data_t)(data), index)) {
			return false;
		}
	}

	if (is_last_tx_records) {
		// Explicitly set the REMOVE flag for events without a specified size.
		if (!dx_has_size(snapshot_data->event_id, data)) {
			dx_set_event_flags(recs->records.elements, index, snapshot_data->event_id, dxf_ef_remove_event);
		}
	} else {
		// Otherwise, clear the flags
		dx_set_event_flags(recs->records.elements, index, snapshot_data->event_id, 0);
	}

	return true;
}

int dx_snapshot_call_listeners(dx_snapshot_data_ptr_t snapshot_data, int new_snapshot) {
	size_t cur_listener_index = 0;

	for (; cur_listener_index < snapshot_data->listeners.size; ++cur_listener_index) {
		dx_snapshot_listener_context_t* listener_context = snapshot_data->listeners.elements + cur_listener_index;
		dxf_snapshot_data_t callback_data;
		callback_data.event_type = snapshot_data->event_type;
		callback_data.symbol = dx_create_string_src(snapshot_data->symbol);

		if (listener_context->incremental) {
			if (new_snapshot || !listener_context->full_snapshot_seen) {
				callback_data.records_count = snapshot_data->snapshot_records.records.size;
				callback_data.records = snapshot_data->snapshot_records.records.elements;
				listener_context->inc_listener(&callback_data, true, listener_context->user_data);
				listener_context->full_snapshot_seen = true;
			} else {
				callback_data.records_count = snapshot_data->last_tx_records.records.size;
				callback_data.records = snapshot_data->last_tx_records.records.elements;
				listener_context->inc_listener(&callback_data, false, listener_context->user_data);
			}
		} else {
			callback_data.records_count = snapshot_data->snapshot_records.records.size;
			callback_data.records = snapshot_data->snapshot_records.records.elements;
			listener_context->full_listener(&callback_data, listener_context->user_data);
		}
		dx_free(callback_data.symbol);
	}
	return true;
}

int dx_is_snapshot_event(const dx_snapshot_data_ptr_t snapshot_data, const dxf_event_params_t* event_params) {
	// comparing by snapshot key
	return snapshot_data->key == event_params->snapshot_key;
}

void event_listener(int event_type, dxf_const_string_t symbol_name,
					const dxf_event_data_t* data, int data_count,
					const dxf_event_params_t* event_params, void* user_data) {
	dx_snapshot_data_ptr_t snapshot_data = (dx_snapshot_data_ptr_t)user_data;

	int sb = IS_FLAG_SET(event_params->flags, dxf_ef_snapshot_begin);
	int se = IS_FLAG_SET(event_params->flags, dxf_ef_snapshot_end) || IS_FLAG_SET(event_params->flags, dxf_ef_snapshot_snip);
	int tx = IS_FLAG_SET(event_params->flags, dxf_ef_tx_pending);

	/* It should be no-op, really */
	if (!dx_is_snapshot_event(snapshot_data, event_params)) {
		return;
	}

	int rm = IS_FLAG_SET(event_params->flags, dxf_ef_remove_event) || !dx_has_size(snapshot_data->event_id, data);

	/* Ok, process this event */
	if (sb) {
		/* Clear snapshot */
		dx_snapshot_clear_records_array(snapshot_data, &snapshot_data->snapshot_records);
		dx_snapshot_clear_records_array(snapshot_data, &snapshot_data->last_tx_records);
		snapshot_data->status = dx_status_begin;
		snapshot_data->full_snapshot_published = false;
	}

	if (snapshot_data->status == dx_status_unknown) {
		/* If we in unknown state, skip */
		return;
	}

	/* Process this event */
	if (rm) {
		dx_snapshot_remove_event_records(snapshot_data, &snapshot_data->snapshot_records, data, event_params);
	} else {
		dx_snapshot_update_event_records(snapshot_data, &snapshot_data->snapshot_records, data, event_params, false);
	}
	/* Add to the current tx, if it is pending transaction, may be only with one event, add even "rm" event */
	if (snapshot_data->has_inc_listeners && snapshot_data->full_snapshot_published) {
		dx_snapshot_update_event_records(snapshot_data, &snapshot_data->last_tx_records, data, event_params, true);
	}

	/* Set end-of-snapshot */
	if (se) {
		snapshot_data->status = dx_status_full;
	}

	if (tx && snapshot_data->status == dx_status_full) {
		snapshot_data->status = dx_status_pending;
	} else if (!tx && snapshot_data->status == dx_status_pending) {
		snapshot_data->status = dx_status_full;
	}

	/* And call all consumers  if it is end of transaction */
	if (snapshot_data->status == dx_status_full) {
		/* Lock guard to be sure that source list is intact */
		if (!dx_mutex_lock(&snapshot_data->guard))
			return;
		dx_snapshot_call_listeners(snapshot_data, !snapshot_data->full_snapshot_published);
		dx_mutex_unlock(&snapshot_data->guard);
		/* for sure */
		snapshot_data->full_snapshot_published = true;
		/* start new transaction */
		dx_snapshot_clear_records_array(snapshot_data, &snapshot_data->last_tx_records);
	}
}

/*
 * Function generate key for snapshot object using record_info_id, symbol and source
 *
 * snapshot key format:
 * 64 - 56     55 - 24  23 - 0
 * record_id | symbol | source
 *
 * record_info_id - record type of snapshot subscription
 * symbol - string symbol of snapshot subscription
 * order_source - source for Order records or keyword for MarketMaker; can be NULL also
 * returns 64 bit unsigned decimal
 */
dxf_ulong_t dx_new_snapshot_key(dx_record_info_id_t record_info_id, dxf_const_string_t symbol,
								dxf_const_string_t order_source) {
	dxf_ulong_t symbol_hash = dx_symbol_name_hasher(symbol);
	dxf_ulong_t order_source_hash = (order_source == NULL ? 0u : dx_symbol_name_hasher(order_source));
	return ((dxf_ulong_t)record_info_id << 56u) |
		((dxf_ulong_t)symbol_hash << 24u) |
		(order_source_hash & SNAPSHOT_KEY_SOURCE_MASK);
}

int dx_snapshots_comparator(dx_snapshot_data_ptr_t s1, dx_snapshot_data_ptr_t s2) {
	if (s1 == s2) return 0;
	if (s1 == NULL) return -1;
	if (s2 == NULL) return 1;
	if (s1->key < s2->key) return -1;
	if (s1->key > s2->key) return 1;

	if (s1->event_id < s2->event_id) return -1;
	if (s1->event_id > s2->event_id) return 1;

	if (s1->order_source == NULL && s2->order_source != NULL) return -1;
	if (s2->order_source == NULL && s1->order_source != NULL) return 1;

	//!NULL
	if (s1->order_source != s2->order_source) {
		int order_sources_cmp_result = wcscmp(s1->order_source, s2->order_source);

		if (order_sources_cmp_result < 0) return -1;
		if (order_sources_cmp_result > 0) return 1;
	}

	int symbols_cmp_result = wcscmp(s1->symbol, s2->symbol);

	if (symbols_cmp_result < 0) return -1;
	if (symbols_cmp_result > 0) return 1;

	return 0;
}

void dx_clear_snapshot_listener_array(dx_snapshot_listener_array_t* listeners) {
	dx_free(listeners->elements);

	listeners->elements = NULL;
	listeners->size = 0;
	listeners->capacity = 0;
}

int dx_snapshot_listener_comparator(dx_snapshot_listener_context_t e1,
									dx_snapshot_listener_context_t e2) {
	return e1.incremental == e2.incremental ? DX_FORCED_NUMERIC_COMPARATOR(e1.full_listener, e2.full_listener) : -1;
}

size_t dx_find_snapshot_listener_in_array(dx_snapshot_listener_array_t* listeners,
									int incremental, void *listener, OUT int* found) {
	size_t listener_index;
	dx_snapshot_listener_context_t listener_context = { incremental, false, {.full_listener = *(dxf_snapshot_listener_t*)(&listener)}, NULL };

	DX_ARRAY_SEARCH(listeners->elements, 0, listeners->size, listener_context,
		dx_snapshot_listener_comparator, false, *found, listener_index);

	return listener_index;
}

int dx_free_snapshot_data(dx_snapshot_data_ptr_t snapshot_data) {
	if (snapshot_data == NULL) {
		return false;
	}

	/* remove listeners */
	dx_clear_snapshot_listener_array(&(snapshot_data->listeners));

	/* remove records */
	dx_snapshot_clear_records_array(snapshot_data, &snapshot_data->snapshot_records);
	dx_snapshot_clear_records_array(snapshot_data, &snapshot_data->last_tx_records);

	if (snapshot_data->symbol != NULL)
		dx_free(snapshot_data->symbol);
	if (snapshot_data->order_source != NULL)
		dx_free(snapshot_data->order_source);

	dx_mutex_destroy(&snapshot_data->guard);
	dx_free(snapshot_data);
	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Subscription functions implementation
 */
/* -------------------------------------------------------------------------- */

dxf_snapshot_t dx_create_snapshot(dxf_connection_t connection,
								dxf_subscription_t subscription,
								dx_event_id_t event_id,
								dx_record_info_id_t record_info_id,
								dxf_const_string_t symbol,
								dxf_const_string_t order_source,
								dxf_long_t time) {
	dx_snapshot_subscription_connection_context_t* context = NULL;
	dx_snapshot_data_t *snapshot_data = NULL;
	int failed = false;
	int found = false;
	int res = false;
	size_t position = 0;
	unsigned event_types;

	if (!dx_validate_connection_handle(connection, false)) {
		return dx_invalid_snapshot;
	}

	context = dx_get_subsystem_data(connection, dx_ccs_snapshot_subscription, &res);
	if (context == NULL) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return dx_invalid_snapshot;
	}

	if (!dx_get_event_subscription_event_types(subscription, &event_types)) {
		return dx_invalid_snapshot;
	}

	snapshot_data = dx_calloc(1, sizeof(dx_snapshot_data_t));
	if (snapshot_data == NULL) {
		return dx_invalid_snapshot;
	}
	if (!dx_mutex_create(&snapshot_data->guard)) {
		dx_free(snapshot_data);
		return dx_invalid_snapshot;
	}
	snapshot_data->key = dx_new_snapshot_key(record_info_id, symbol, order_source);
	snapshot_data->record_info_id = record_info_id;
	snapshot_data->event_id = event_id;
	snapshot_data->event_type = (int)event_types;
	snapshot_data->symbol = dx_create_string_src(symbol);
	snapshot_data->time = time;
	if (order_source != NULL)
		snapshot_data->order_source = dx_create_string_src(order_source);
	snapshot_data->status = dx_status_full;
	snapshot_data->full_snapshot_published = true;
	snapshot_data->sscc = context;
	snapshot_data->subscription = subscription;

	if (!dx_add_listener_v2(snapshot_data->subscription, event_listener, (void*)snapshot_data)) {
		dx_free_snapshot_data(snapshot_data);
		return dx_invalid_snapshot;
	}

	if (!dx_mutex_lock(&(context->guard))) {
		dx_free_snapshot_data(snapshot_data);
		return dx_invalid_snapshot;
	}

	if (context->snapshots_array.size > 0) {
		DX_ARRAY_BINARY_SEARCH(context->snapshots_array.elements, 0, context->snapshots_array.size,
			snapshot_data, dx_snapshots_comparator, found, position);
		if (found) {
			dx_free_snapshot_data(snapshot_data);
			dx_set_error_code(dx_ssec_snapshot_exist);
			dx_mutex_unlock(&(context->guard));
			return dx_invalid_snapshot;
		}
	}

	/* add snapshot to array */
	DX_ARRAY_INSERT(context->snapshots_array, dx_snapshot_data_ptr_t, snapshot_data, position,
		dx_capacity_manager_halfer, failed);
	if (failed) {
		dx_free_snapshot_data(snapshot_data);
		snapshot_data = dx_invalid_snapshot;
	}

	return (dx_mutex_unlock(&(context->guard)) ? (dxf_snapshot_t)snapshot_data : dx_invalid_snapshot);
}

int dx_close_snapshot(dxf_snapshot_t snapshot) {
	dx_snapshot_subscription_connection_context_t* context = NULL;
	dx_snapshot_data_ptr_t snapshot_data = NULL;
	int found = false;
	int failed = false;
	size_t position = 0;

	if (snapshot == dx_invalid_snapshot) {
		return dx_set_error_code(dx_ssec_invalid_snapshot_id);
	}

	snapshot_data = (dx_snapshot_data_ptr_t)snapshot;
	context = CTX(snapshot_data->sscc);

	/* locking a guard mutex */
	CHECKED_CALL(dx_mutex_lock, &(context->guard));

	/* remove item from snapshots_array */
	DX_ARRAY_BINARY_SEARCH(context->snapshots_array.elements, 0, context->snapshots_array.size,
		snapshot_data, dx_snapshots_comparator, found, position);
	if (found) {
		DX_ARRAY_DELETE(context->snapshots_array, dx_snapshot_data_ptr_t, position,
			dx_capacity_manager_halfer, failed);
		if (failed) {
			dx_set_error_code(dx_mec_insufficient_memory);
		}
		dx_free_snapshot_data(snapshot_data);
		dx_mutex_unlock(&(context->guard));
		return !failed;
	}
	else {
		dx_mutex_unlock(&(context->guard));
		return dx_set_error_code(dx_ssec_invalid_snapshot_id);
	}
}

int dx_add_snapshot_listener(dxf_snapshot_t snapshot, dxf_snapshot_listener_t listener, void* user_data) {
	dx_snapshot_data_ptr_t snapshot_data = (dx_snapshot_data_ptr_t)snapshot;
	size_t listener_index;
	int failed;
	int found = false;

	if (snapshot == dx_invalid_snapshot) {
		return dx_set_error_code(dx_ssec_invalid_snapshot_id);
	}

	if (listener == NULL) {
		return dx_set_error_code(dx_ssec_invalid_listener);
	}

	CHECKED_CALL(dx_mutex_lock, &(snapshot_data->guard));

	listener_index = dx_find_snapshot_listener_in_array(&(snapshot_data->listeners), false, *(void**)(&listener), &found);

	if (found) {
		return dx_mutex_unlock(&snapshot_data->guard);
	}

	{
		dx_snapshot_listener_context_t listener_context = {
			.incremental = false,
			.full_snapshot_seen = false,
			.full_listener = listener,
			.user_data = user_data
		};
		DX_ARRAY_INSERT(snapshot_data->listeners, dx_snapshot_listener_context_t, listener_context,
			listener_index, dx_capacity_manager_halfer, failed);
	}

	return dx_mutex_unlock(&(snapshot_data->guard)) && !failed;
}

int dx_remove_snapshot_listener(dxf_snapshot_t snapshot, dxf_snapshot_listener_t listener) {
	dx_snapshot_data_ptr_t snapshot_data = (dx_snapshot_data_ptr_t)snapshot;
	size_t listener_index;
	int failed;
	int found = false;

	if (snapshot == dx_invalid_snapshot) {
		return dx_set_error_code(dx_ssec_invalid_snapshot_id);
	}

	if (listener == NULL) {
		return dx_set_error_code(dx_ssec_invalid_listener);
	}

	CHECKED_CALL(dx_mutex_lock, &(snapshot_data->guard));
	listener_index = dx_find_snapshot_listener_in_array(&(snapshot_data->listeners), false, *(void**)(&listener), &found);

	if (!found) {
		return dx_mutex_unlock(&snapshot_data->guard);
	}

	/*
	a guard mutex is required to protect the internal containers
	from the secondary data retriever threads
	*/

	DX_ARRAY_DELETE(snapshot_data->listeners, dx_snapshot_listener_context_t, listener_index,
		dx_capacity_manager_halfer, failed);

	return dx_mutex_unlock(&snapshot_data->guard) && !failed;
}

int dx_add_snapshot_inc_listener(dxf_snapshot_t snapshot, dxf_snapshot_inc_listener_t listener, void* user_data) {
	dx_snapshot_data_ptr_t snapshot_data = (dx_snapshot_data_ptr_t)snapshot;
	size_t listener_index;
	int failed;
	int found = false;

	if (snapshot == dx_invalid_snapshot) {
		return dx_set_error_code(dx_ssec_invalid_snapshot_id);
	}

	if (listener == NULL) {
		return dx_set_error_code(dx_ssec_invalid_listener);
	}

	CHECKED_CALL(dx_mutex_lock, &(snapshot_data->guard));
	listener_index = dx_find_snapshot_listener_in_array(&(snapshot_data->listeners), true, *(void**)(&listener), &found);

	if (found) {
		return dx_mutex_unlock(&snapshot_data->guard);
	}

	{
		dx_snapshot_listener_context_t listener_context = {
			.incremental = true,
			.full_snapshot_seen = false,
			.inc_listener = listener,
			.user_data = user_data
		};
		DX_ARRAY_INSERT(snapshot_data->listeners, dx_snapshot_listener_context_t, listener_context,
			listener_index, dx_capacity_manager_halfer, failed);
		snapshot_data->has_inc_listeners = true;
	}

	return dx_mutex_unlock(&snapshot_data->guard) && !failed;
}

int dx_remove_snapshot_inc_listener(dxf_snapshot_t snapshot, dxf_snapshot_inc_listener_t listener) {
	dx_snapshot_data_ptr_t snapshot_data = (dx_snapshot_data_ptr_t)snapshot;
	size_t listener_index;
	int failed;
	int found = false;

	if (snapshot == dx_invalid_snapshot) {
		return dx_set_error_code(dx_ssec_invalid_snapshot_id);
	}

	if (listener == NULL) {
		return dx_set_error_code(dx_ssec_invalid_listener);
	}

	CHECKED_CALL(dx_mutex_lock, &(snapshot_data->guard));
	listener_index = dx_find_snapshot_listener_in_array(&(snapshot_data->listeners), true, *(void**)(&listener), &found);

	if (!found) {
		return dx_mutex_unlock(&snapshot_data->guard);
	}

	DX_ARRAY_DELETE(snapshot_data->listeners, dx_snapshot_listener_context_t, listener_index,
		dx_capacity_manager_halfer, failed);

	/* Check, do we have anymore incremental listeners */
	for (listener_index = 0; !snapshot_data->has_inc_listeners && listener_index < snapshot_data->listeners.size; listener_index++) {
		snapshot_data->has_inc_listeners |= snapshot_data->listeners.elements[listener_index].incremental;
	}

	return dx_mutex_unlock(&snapshot_data->guard) && !failed;
}

int dx_get_snapshot_subscription(dxf_snapshot_t snapshot, OUT dxf_subscription_t *subscription) {
	if (snapshot == dx_invalid_snapshot) {
		return dx_set_error_code(dx_ssec_invalid_snapshot_id);
	}
	*subscription = ((dx_snapshot_data_t*)snapshot)->subscription;
	return true;
}

dxf_string_t dx_get_snapshot_symbol(dxf_snapshot_t snapshot) {
	dx_snapshot_data_ptr_t snapshot_data = (dx_snapshot_data_ptr_t)snapshot;

	if (snapshot == dx_invalid_snapshot) {
		dx_set_error_code(dx_ssec_invalid_snapshot_id);
		return NULL;
	}

	return snapshot_data->symbol;
}
