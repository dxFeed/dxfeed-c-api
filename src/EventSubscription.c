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

#include "EventSubscription.h"
#include "DXErrorHandling.h"
#include "DXErrorCodes.h"
#include "DXMemory.h"
#include "DXThreads.h"
#include "SymbolCodec.h"
#include "DXAlgorithms.h"
#include "Logger.h"
#include "ConnectionContextData.h"

/* -------------------------------------------------------------------------- */
/*
 *	Internal data structures and objects
 */
/* -------------------------------------------------------------------------- */

const dxf_const_string_t dx_all_order_sources[] = {
	L"NTV",
	L"NFX",
	L"ESPD",
	L"XNFI",
	L"ICE",
	L"ISE",
	L"DEA",
	L"DEX",
	L"BYX",
	L"BZX",
	L"BATE",
	L"CHIX",
	L"CEUX",
	L"BXTR",
	L"IST",
	L"BI20",
	L"ABE",
	L"FAIR",
	L"GLBX",
	L"ERIS",
	L"XEUR",
	L"CFE",
	L"SMFE",
	NULL
};
const size_t dx_all_order_sources_count = sizeof(dx_all_order_sources) / sizeof(dx_all_order_sources[0]) - 1;

const size_t dx_all_regional_count = 26;

struct dx_subscription_data_struct_t;
typedef struct dx_subscription_data_struct_t dx_subscription_data_t;
typedef dx_subscription_data_t* dx_subscription_data_ptr_t;

typedef struct {
	dx_subscription_data_ptr_t* elements;
	size_t size;
	size_t capacity;
} dx_subscription_data_array_t;

typedef struct {
	dxf_const_string_t name;
	dxf_int_t cipher;
	int ref_count;
	dx_subscription_data_array_t subscriptions;
	dxf_event_data_t* last_events;
	dxf_event_data_t* last_events_accessed;
} dx_symbol_data_t, *dx_symbol_data_ptr_t;

typedef void* dx_event_listener_ptr_t;

typedef enum {
	dx_elv_default = 1,
	dx_elv_v2 = 2
} dx_event_listener_version_t;

typedef struct {
	dx_event_listener_ptr_t listener;
	dx_event_listener_version_t version;
	void* user_data;
} dx_listener_context_t;

typedef struct {
	dx_listener_context_t* elements;
	size_t size;
	size_t capacity;
} dx_listener_array_t;

typedef struct {
	dx_symbol_data_ptr_t* elements;
	size_t size;
	size_t capacity;
} dx_symbol_data_array_t;

struct dx_subscription_data_struct_t {
	int event_types;
	dx_symbol_data_array_t symbols;
	dx_listener_array_t listeners;
	dx_order_source_array_t order_source;
	dx_event_subscr_flag subscr_flags;
	dxf_long_t time;

	dxf_const_string_t* symbol_name_array;
	size_t symbol_name_array_capacity;

	void* escc; /* event subscription connection context */
};

/* -------------------------------------------------------------------------- */
/*
 *	Symbol map structures
 */
/* -------------------------------------------------------------------------- */

#define SYMBOL_BUCKET_COUNT    1000

/* -------------------------------------------------------------------------- */
/*
 *	Event subscription connection context
 */
/* -------------------------------------------------------------------------- */

typedef struct {
	dxf_connection_t connection;

	dx_mutex_t subscr_guard;

	dx_symbol_data_array_t ciphered_symbols[SYMBOL_BUCKET_COUNT];
	dx_symbol_data_array_t hashed_symbols[SYMBOL_BUCKET_COUNT];

	dx_subscription_data_array_t subscriptions;

	int set_fields_flags;
} dx_event_subscription_connection_context_t;

#define MUTEX_FIELD_FLAG    (0x1)

#define CTX(context) \
	((dx_event_subscription_connection_context_t*)context)

/* -------------------------------------------------------------------------- */

bool dx_clear_event_subscription_connection_context (dx_event_subscription_connection_context_t* context);

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_event_subscription) {
	dx_event_subscription_connection_context_t* context = NULL;

	CHECKED_CALL_2(dx_validate_connection_handle, connection, true);

	context = dx_calloc(1, sizeof(dx_event_subscription_connection_context_t));

	if (context == NULL) {
		return false;
	}

	context->connection = connection;

	if (!dx_mutex_create(&context->subscr_guard)) {
		dx_clear_event_subscription_connection_context(context);

		return false;
	}

	context->set_fields_flags |= MUTEX_FIELD_FLAG;

	if (!dx_set_subsystem_data(connection, dx_ccs_event_subscription, context)) {
		dx_clear_event_subscription_connection_context(context);

		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_event_subscription) {
	bool res = true;
	dx_event_subscription_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_event_subscription, &res);

	if (context == NULL) {
		return res;
	}

	return dx_clear_event_subscription_connection_context(context);
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_event_subscription) {
	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_clear_event_subscription_connection_context (dx_event_subscription_connection_context_t* context) {
	bool res = true;
	size_t i = 0;

	for (; i < context->subscriptions.size; ++i) {
		res = dx_close_event_subscription((dxf_subscription_t)context->subscriptions.elements[i]) && res;
	}

	if (IS_FLAG_SET(context->set_fields_flags, MUTEX_FIELD_FLAG)) {
		res = dx_mutex_destroy(&(context->subscr_guard)) && res;
	}

	if (context->subscriptions.elements != NULL) {
		dx_free(context->subscriptions.elements);
	}

	dx_free(context);

	return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	Helper functions
 */
/* -------------------------------------------------------------------------- */

dxf_int_t dx_symbol_name_hasher (dxf_const_string_t symbol_name) {
	dxf_int_t h = 0;
	size_t len = 0;
	size_t i = 0;

	len = dx_string_length(symbol_name);

	for (; i < len; ++i) {
		h = 37 * h + dx_toupper(symbol_name[i]);
	}

	return h;
}

/* -------------------------------------------------------------------------- */

typedef int (*dx_symbol_comparator_t)(dx_symbol_data_ptr_t e1, dx_symbol_data_ptr_t e2);

int dx_ciphered_symbol_comparator (dx_symbol_data_ptr_t e1, dx_symbol_data_ptr_t e2) {
	if (e1->cipher < e2->cipher)
		return -1;
	if (e1->cipher > e2->cipher)
		return 1;
	return 0;
	// do not use next line, you'll get overflow
	//return (e1->cipher - e2->cipher);
}

/* -------------------------------------------------------------------------- */

int dx_hashed_symbol_comparator (dx_symbol_data_ptr_t e1, dx_symbol_data_ptr_t e2) {
	if (e1->cipher != e2->cipher) {
		if (e1->cipher < e2->cipher)
			return -1;
		if (e1->cipher > e2->cipher)
			return 1;
	}

	return dx_compare_strings(e1->name, e2->name);
}

/* -------------------------------------------------------------------------- */

int dx_name_symbol_comparator (dx_symbol_data_ptr_t e1, dx_symbol_data_ptr_t e2) {
	return dx_compare_strings(e1->name, e2->name);
}

/* -------------------------------------------------------------------------- */

int dx_get_bucket_index (dxf_int_t cipher) {
	dxf_int_t mod = cipher % SYMBOL_BUCKET_COUNT;

	if (mod < 0) {
		mod += SYMBOL_BUCKET_COUNT;
	}

	return (int)mod;
}

/* -------------------------------------------------------------------------- */

void dx_cleanup_event_data_array (dxf_event_data_t* data_array) {
	int i = dx_eid_begin;

	if (data_array == NULL) {
		return;
	}

	for (; i < dx_eid_count; ++i) {
		CHECKED_FREE(data_array[i]);
	}

	dx_free(data_array);
}

/* ---------------------------------- */

dx_symbol_data_ptr_t dx_cleanup_symbol_data (dx_symbol_data_ptr_t symbol_data) {
	if (symbol_data == NULL) {
		return NULL;
	}

	CHECKED_FREE(symbol_data->name);

	dx_cleanup_event_data_array(symbol_data->last_events);
	dx_cleanup_event_data_array(symbol_data->last_events_accessed);

	dx_free(symbol_data);

	return NULL;
}

/* -------------------------------------------------------------------------- */

dx_symbol_data_ptr_t dx_create_symbol_data (dxf_const_string_t name, dxf_int_t cipher) {
	int i = dx_eid_begin;
	dx_symbol_data_ptr_t res = dx_calloc(1, sizeof(dx_symbol_data_t));

	if (res == NULL) {
		return res;
	}

	res->name = dx_create_string_src(name);
	res->cipher = cipher;

	if (res->name == NULL) {
		return dx_cleanup_symbol_data(res);
	}

	res->last_events = dx_calloc(dx_eid_count, sizeof(dxf_event_data_t));
	res->last_events_accessed = dx_calloc(dx_eid_count, sizeof(dxf_event_data_t));

	if (res->last_events == NULL ||
		res->last_events_accessed == NULL) {

		return dx_cleanup_symbol_data(res);
	}

	for (; i < dx_eid_count; ++i) {
		res->last_events[i] = dx_calloc(1, dx_get_event_data_struct_size(i));
		res->last_events_accessed[i] = dx_calloc(1, dx_get_event_data_struct_size(i));

		if (res->last_events[i] == NULL ||
			res->last_events_accessed[i] == NULL) {

			return dx_cleanup_symbol_data(res);
		}
	}

	return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary functions
 */
/* -------------------------------------------------------------------------- */

dx_symbol_data_ptr_t dx_subscribe_symbol (dx_event_subscription_connection_context_t* context,
										dxf_const_string_t symbol_name, dx_subscription_data_ptr_t owner) {
	dx_symbol_data_ptr_t res = NULL;
	bool is_just_created = false;

	{
		dx_symbol_comparator_t comparator = dx_ciphered_symbol_comparator;
		dx_symbol_data_array_t* symbol_container = context->ciphered_symbols;
		dx_symbol_data_array_t* symbol_array_obj_ptr = NULL;
		dx_symbol_data_t dummy;

		bool symbol_exists = false;
		size_t symbol_index;
		bool failed = false;

		dummy.name = symbol_name;

		if ((dummy.cipher = dx_encode_symbol_name(symbol_name)) == 0) {
			symbol_container = context->hashed_symbols;
			comparator = dx_hashed_symbol_comparator;
			dummy.cipher = dx_symbol_name_hasher(symbol_name);
		}

		symbol_array_obj_ptr = &(symbol_container[dx_get_bucket_index(dummy.cipher)]);

		DX_ARRAY_SEARCH(symbol_array_obj_ptr->elements, 0, symbol_array_obj_ptr->size,
						&dummy, comparator, true, symbol_exists, symbol_index);

		if (!symbol_exists) {
			res = dx_create_symbol_data(symbol_name, dummy.cipher);

			if (res == NULL) {
				return NULL;
			}

			is_just_created = true;

			DX_ARRAY_INSERT(*symbol_array_obj_ptr, dx_symbol_data_ptr_t, res, symbol_index, dx_capacity_manager_halfer, failed);

			if (failed) {
				return dx_cleanup_symbol_data(res);
			}
		} else {
			res = symbol_array_obj_ptr->elements[symbol_index];
		}
	}

	{
		bool subscr_exists = false;
		size_t subscr_index;
		bool failed = false;

		DX_ARRAY_SEARCH(res->subscriptions.elements, 0, res->subscriptions.size, owner, DX_NUMERIC_COMPARATOR, false,
						subscr_exists, subscr_index);

		if (subscr_exists) {
			return res;
		}

		DX_ARRAY_INSERT(res->subscriptions, dx_subscription_data_ptr_t, owner, subscr_index, dx_capacity_manager_halfer, failed);

		if (failed) {
			return (is_just_created ? dx_cleanup_symbol_data(res) : NULL);
		}

		++(res->ref_count);
	}

	return res;
}

/* -------------------------------------------------------------------------- */

bool dx_unsubscribe_symbol (dx_event_subscription_connection_context_t* context, dx_symbol_data_ptr_t symbol_data, dx_subscription_data_ptr_t owner) {
	bool res = true;

	do {
		bool subscr_exists = false;
		size_t subscr_index;
		bool failed = false;

		DX_ARRAY_SEARCH(symbol_data->subscriptions.elements, 0, symbol_data->subscriptions.size, owner, DX_NUMERIC_COMPARATOR, false,
						subscr_exists, subscr_index);

		if (!subscr_exists) {
			/* should never be here */

			dx_set_error_code(dx_ec_internal_assert_violation);

			res = false;

			break;
		}

		DX_ARRAY_DELETE(symbol_data->subscriptions, dx_subscription_data_ptr_t, subscr_index, dx_capacity_manager_halfer, failed);

		if (failed) {
			/* most probably the memory allocation error */

			res = false;
		}
	} while (false);

	if (--(symbol_data->ref_count) == 0) {
		dx_symbol_comparator_t comparator = dx_ciphered_symbol_comparator;
		dx_symbol_data_array_t* symbol_container = context->ciphered_symbols;
		dx_symbol_data_array_t* symbol_array_obj_ptr = NULL;

		bool symbol_exists = false;
		size_t symbol_index;
		bool failed = false;

		if (dx_encode_symbol_name(symbol_data->name) == 0) {
			symbol_container = context->hashed_symbols;
			comparator = dx_hashed_symbol_comparator;
		}

		symbol_array_obj_ptr = &(symbol_container[dx_get_bucket_index(symbol_data->cipher)]);

		DX_ARRAY_SEARCH(symbol_array_obj_ptr->elements, 0, symbol_array_obj_ptr->size,
						symbol_data, comparator, true, symbol_exists, symbol_index);

		if (!symbol_exists) {
			/* the symbol must've had been found in the container */

			dx_set_error_code(dx_ec_internal_assert_violation);

			res = false;
		} else {
			DX_ARRAY_DELETE(*symbol_array_obj_ptr, dx_symbol_data_ptr_t, symbol_index, dx_capacity_manager_halfer, failed);

			if (failed) {
				res = false;
			}
		}

		dx_cleanup_symbol_data(symbol_data);
	}

	return res;
}

/* -------------------------------------------------------------------------- */

void dx_clear_listener_array (dx_listener_array_t* listeners) {
	dx_free(listeners->elements);

	listeners->elements = NULL;
	listeners->size = 0;
	listeners->capacity = 0;
}

/* -------------------------------------------------------------------------- */

bool dx_clear_symbol_array (dx_event_subscription_connection_context_t* context,
							dx_symbol_data_array_t* symbols, dx_subscription_data_ptr_t owner) {
	size_t symbol_index = 0;
	bool res = true;

	for (; symbol_index < symbols->size; ++symbol_index) {
		dx_symbol_data_ptr_t symbol_data = symbols->elements[symbol_index];

		res = dx_unsubscribe_symbol(context, symbol_data, owner) && res;
	}

	dx_free(symbols->elements);

	symbols->elements = NULL;
	symbols->size = 0;
	symbols->capacity = 0;

	return res;
}

/* -------------------------------------------------------------------------- */

size_t dx_find_symbol_in_array (dx_symbol_data_array_t* symbols, dxf_const_string_t symbol_name, OUT bool* found) {
	dx_symbol_data_t data;
	dx_symbol_comparator_t comparator;
	size_t symbol_index;

	data.name = symbol_name;

	if ((data.cipher = dx_encode_symbol_name(symbol_name)) != 0) {
		comparator = dx_ciphered_symbol_comparator;
	} else {
		comparator = dx_name_symbol_comparator;
	}

	DX_ARRAY_SEARCH(symbols->elements, 0, symbols->size, &data, comparator, true, *found, symbol_index);

	return symbol_index;
}

/* -------------------------------------------------------------------------- */

int dx_listener_comparator (dx_listener_context_t e1, dx_listener_context_t e2) {
	return DX_NUMERIC_COMPARATOR(e1.listener, e2.listener);
}

/* -------------------------------------------------------------------------- */

size_t dx_find_listener_in_array (dx_listener_array_t* listeners, dx_event_listener_ptr_t listener, OUT bool* found) {
	size_t listener_index;
	/* There is comparing listeners by address, so fill version and user_data with default values */
	dx_listener_context_t listener_context = { listener, dx_elv_default, NULL };

	DX_ARRAY_SEARCH(listeners->elements, 0, listeners->size, listener_context, dx_listener_comparator, false, *found, listener_index);

	return listener_index;
}

/* -------------------------------------------------------------------------- */

dx_symbol_data_ptr_t dx_find_symbol (dx_event_subscription_connection_context_t* context,
									dxf_const_string_t symbol_name, dxf_int_t symbol_cipher) {
	dx_symbol_data_ptr_t res = NULL;

	dx_symbol_comparator_t comparator = dx_ciphered_symbol_comparator;
	dx_symbol_data_array_t* symbol_container = context->ciphered_symbols;
	dx_symbol_data_array_t* symbol_array_obj_ptr = NULL;
	dx_symbol_data_t dummy;

	bool symbol_exists = false;
	size_t symbol_index;

	dummy.name = symbol_name;

	if ((dummy.cipher = symbol_cipher) == 0) {
		symbol_container = context->hashed_symbols;
		comparator = dx_hashed_symbol_comparator;
		dummy.cipher = dx_symbol_name_hasher(symbol_name);
	}

	symbol_array_obj_ptr = &(symbol_container[dx_get_bucket_index(dummy.cipher)]);

	DX_ARRAY_SEARCH(symbol_array_obj_ptr->elements, 0, symbol_array_obj_ptr->size,
					&dummy, comparator, true, symbol_exists, symbol_index);

	if (!symbol_exists) {
		return NULL;
	}

	return symbol_array_obj_ptr->elements[symbol_index];
}

/* -------------------------------------------------------------------------- */

void dx_store_last_symbol_event (dx_symbol_data_ptr_t symbol_data, dx_event_id_t event_id,
								const dxf_event_data_t data, int data_count) {
	dx_memcpy(symbol_data->last_events[event_id],
			dx_get_event_data_item(DX_EVENT_BIT_MASK(event_id), data, data_count - 1),
			dx_get_event_data_struct_size(event_id));
}

/* -------------------------------------------------------------------------- */

bool dx_add_subscription_to_context (dx_subscription_data_ptr_t subscr_data) {
	bool subscr_exists = false;
	size_t subscr_index;
	bool failed = false;
	dx_subscription_data_array_t* subscriptions = &(CTX(subscr_data->escc)->subscriptions);

	DX_ARRAY_SEARCH(subscriptions->elements, 0, subscriptions->size, subscr_data, DX_NUMERIC_COMPARATOR, false,
					subscr_exists, subscr_index);

	if (subscr_exists) {
		/* should never be here */

		return dx_set_error_code(dx_ec_internal_assert_violation);
	}

	DX_ARRAY_INSERT(*subscriptions, dx_subscription_data_ptr_t, subscr_data, subscr_index, dx_capacity_manager_halfer, failed);

	return !failed;
}

/* -------------------------------------------------------------------------- */

bool dx_remove_subscription_from_context (dx_subscription_data_ptr_t subscr_data) {
	bool subscr_exists = false;
	size_t subscr_index;
	bool failed = false;
	dx_subscription_data_array_t* subscriptions = &(CTX(subscr_data->escc)->subscriptions);

	DX_ARRAY_SEARCH(subscriptions->elements, 0, subscriptions->size, subscr_data, DX_NUMERIC_COMPARATOR, false,
					subscr_exists, subscr_index);

	if (!subscr_exists) {
		/* should never be here */

		return dx_set_error_code(dx_ec_internal_assert_violation);
	}

	DX_ARRAY_DELETE(*subscriptions, dx_subscription_data_ptr_t, subscr_index, dx_capacity_manager_halfer, failed);

	if (failed) {
		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

void dx_free_event_subscription_data (dx_subscription_data_ptr_t subscr_data) {
	if (subscr_data == NULL) {
		return;
	}

	/* no element freeing is performed because this array only stores the pointers to the strings allocated somewhere else */
	CHECKED_FREE(subscr_data->symbol_name_array);
	dx_clear_order_source(subscr_data);

	dx_free(subscr_data);
}

/* -------------------------------------------------------------------------- */
/*
 *	Subscription functions implementation
 */
/* -------------------------------------------------------------------------- */

const dxf_subscription_t dx_invalid_subscription = (dxf_subscription_t)NULL;

dxf_subscription_t dx_create_event_subscription(dxf_connection_t connection, int event_types,
												dx_event_subscr_flag subscr_flags, dxf_long_t time) {
	dx_subscription_data_ptr_t subscr_data = NULL;
	bool res = true;
	dx_event_subscription_connection_context_t* context = NULL;
	int i = 0;

	if (!dx_validate_connection_handle(connection, false)) {
		return dx_invalid_subscription;
	}

	context = dx_get_subsystem_data(connection, dx_ccs_event_subscription, &res);

	if (context == NULL) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return dx_invalid_subscription;
	}

	if (event_types & DXF_ET_UNUSED) {
		dx_set_error_code(dx_esec_invalid_event_type);

		return dx_invalid_subscription;
	}

	subscr_data = dx_calloc(1, sizeof(dx_subscription_data_t));

	if (subscr_data == NULL) {
		return dx_invalid_subscription;
	}

	subscr_data->event_types = event_types;
	subscr_data->escc = context;
	subscr_data->subscr_flags = subscr_flags;
	subscr_data->time = time;

	res = true;
	if (event_types & DXF_ET_ORDER) {
		for (; dx_all_order_sources[i]; i++) {
			res &= dx_add_order_source(subscr_data, dx_all_order_sources[i]);
		}
		res &= dx_add_order_source(subscr_data, L"");
	}
	if (!res) {
		dx_free_event_subscription_data(subscr_data);
		return dx_invalid_subscription;
	}

	if (!dx_mutex_lock(&(context->subscr_guard))) {
		dx_free_event_subscription_data(subscr_data);

		return dx_invalid_subscription;
	}

	if (!dx_add_subscription_to_context(subscr_data)) {
		dx_free_event_subscription_data(subscr_data);

		subscr_data = (dx_subscription_data_ptr_t)dx_invalid_subscription;
	}

	return (dx_mutex_unlock(&(context->subscr_guard)) ? (dxf_subscription_t)subscr_data : dx_invalid_subscription);
}

/* -------------------------------------------------------------------------- */

bool dx_close_event_subscription (dxf_subscription_t subscr_id) {
	dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
	bool res = true;
	dx_event_subscription_connection_context_t* context = NULL;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	context = subscr_data->escc;

	/* locking a guard mutex */
	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	res = dx_clear_symbol_array(context, &(subscr_data->symbols), subscr_data) && res;
	dx_clear_listener_array(&(subscr_data->listeners));

	if (!dx_remove_subscription_from_context(subscr_data)) {
		res = false;
	}

	res = dx_mutex_unlock(&(context->subscr_guard)) && res;

	dx_free_event_subscription_data(subscr_data);

	return res;
}

/* -------------------------------------------------------------------------- */

bool dx_add_symbols (dxf_subscription_t subscr_id, dxf_const_string_t* symbols, int symbol_count) {
	dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
	int cur_symbol_index = 0;
	dx_event_subscription_connection_context_t* context = NULL;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	context = subscr_data->escc;

	/* a guard mutex is required to protect the internal containers
	from the secondary data retriever threads */
	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	for (; cur_symbol_index < symbol_count; ++cur_symbol_index) {
		dx_symbol_data_ptr_t symbol_data;
		size_t symbol_index;
		bool found = false;
		bool failed = false;

		if (dx_string_null_or_empty(symbols[cur_symbol_index])) {
			continue;
		}

		symbol_index = dx_find_symbol_in_array(&(subscr_data->symbols), symbols[cur_symbol_index], &found);

		if (found) {
			/* symbol is already subscribed */

			continue;
		}

		symbol_data = dx_subscribe_symbol(context, symbols[cur_symbol_index], subscr_data);

		if (symbol_data == NULL) {
			dx_mutex_unlock(&(context->subscr_guard));

			return false;
		}

		DX_ARRAY_INSERT(subscr_data->symbols, dx_symbol_data_ptr_t, symbol_data, symbol_index, dx_capacity_manager_halfer, failed);

		if (failed) {
			dx_mutex_unlock(&(context->subscr_guard));

			return false;
		}
	}

	return dx_mutex_unlock(&(context->subscr_guard));
}

/* -------------------------------------------------------------------------- */

bool dx_remove_symbols (dxf_subscription_t subscr_id, dxf_const_string_t* symbols, size_t symbol_count) {
	dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
	size_t cur_symbol_index = 0;
	dx_event_subscription_connection_context_t* context = NULL;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	context = subscr_data->escc;

	/* a guard mutex is required to protect the internal containers
	from the secondary data retriever threads */
	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	for (; cur_symbol_index < symbol_count; ++cur_symbol_index) {
		if (dx_string_null_or_empty(symbols[cur_symbol_index])) {
			continue;
		}

		size_t symbol_index;
		bool failed = false;
		bool found = false;

		symbol_index = dx_find_symbol_in_array(&(subscr_data->symbols), symbols[cur_symbol_index], &found);

		if (!found) {
			/* symbol wasn't subscribed */

			continue;
		}

		if (!dx_unsubscribe_symbol(context, subscr_data->symbols.elements[symbol_index], subscr_data)) {
			dx_mutex_unlock(&(context->subscr_guard));

			return false;
		}

		DX_ARRAY_DELETE(subscr_data->symbols, dx_symbol_data_ptr_t, symbol_index, dx_capacity_manager_halfer, failed);

		if (failed) {
			dx_mutex_unlock(&(context->subscr_guard));

			return false;
		}
	}

	return dx_mutex_unlock(&(context->subscr_guard));
}

/* -------------------------------------------------------------------------- */

bool dx_add_listener_impl(dxf_subscription_t subscr_id, dx_event_listener_ptr_t listener,
						dx_event_listener_version_t version, void* user_data) {
	dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
	size_t listener_index;
	bool failed;
	bool found = false;
	dx_event_subscription_connection_context_t* context = NULL;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (listener == NULL) {
		return dx_set_error_code(dx_esec_invalid_listener);
	}

	context = subscr_data->escc;

	/* a guard mutex is required to protect the internal containers
	from the secondary data retriever threads */
	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	listener_index = dx_find_listener_in_array(&(subscr_data->listeners), listener, &found);

	if (found) {
		/* listener is already added */
		return dx_mutex_unlock(&(context->subscr_guard));
	}

	{
		dx_listener_context_t listener_context = { listener, version, user_data };

		DX_ARRAY_INSERT(subscr_data->listeners, dx_listener_context_t, listener_context, listener_index, dx_capacity_manager_halfer, failed);
	}

	return dx_mutex_unlock(&(context->subscr_guard)) && !failed;
}

/* -------------------------------------------------------------------------- */

bool dx_add_listener(dxf_subscription_t subscr_id, dxf_event_listener_t listener, void* user_data) {
	return dx_add_listener_impl(subscr_id, (dx_event_listener_ptr_t)listener, dx_elv_default, user_data);
}

/* -------------------------------------------------------------------------- */

bool dx_add_listener_v2(dxf_subscription_t subscr_id, dxf_event_listener_v2_t listener, void* user_data) {
	return dx_add_listener_impl(subscr_id, (dx_event_listener_ptr_t)listener, dx_elv_v2, user_data);
}

/* -------------------------------------------------------------------------- */

bool dx_remove_listener_impl(dxf_subscription_t subscr_id, dx_event_listener_ptr_t listener) {
	dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
	size_t listener_index;
	bool failed;
	bool found = false;
	dx_event_subscription_connection_context_t* context = NULL;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (listener == NULL) {
		return dx_set_error_code(dx_esec_invalid_listener);
	}

	context = subscr_data->escc;
	/*
	a guard mutex is required to protect the internal containers
	from the secondary data retriever threads
	*/
	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	listener_index = dx_find_listener_in_array(&(subscr_data->listeners), listener, &found);

	if (!found) {
		/* listener isn't subscribed */
		return dx_mutex_unlock(&(context->subscr_guard));
	}

	DX_ARRAY_DELETE(subscr_data->listeners, dx_listener_context_t, listener_index, dx_capacity_manager_halfer, failed);

	return dx_mutex_unlock(&(context->subscr_guard)) && !failed;
}

bool dx_remove_listener(dxf_subscription_t subscr_id, dxf_event_listener_t listener) {
	return dx_remove_listener_impl(subscr_id, (dx_event_listener_ptr_t)listener);
}

/* -------------------------------------------------------------------------- */

bool dx_remove_listener_v2(dxf_subscription_t subscr_id, dxf_event_listener_v2_t listener) {
	return dx_remove_listener_impl(subscr_id, (dx_event_listener_ptr_t)listener);
}

/* -------------------------------------------------------------------------- */

bool dx_get_subscription_connection (dxf_subscription_t subscr_id, OUT dxf_connection_t* connection) {
	dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (connection == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	*connection = CTX(subscr_data->escc)->connection;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_get_event_subscription_event_types (dxf_subscription_t subscr_id, OUT int* event_types) {
	dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (event_types == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	*event_types = subscr_data->event_types;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_get_event_subscription_symbols(dxf_subscription_t subscr_id,
									OUT dxf_const_string_t** symbols, OUT size_t* symbol_count) {
	dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
	size_t i = 0;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (symbols == NULL || symbol_count == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	if (subscr_data->symbols.size > subscr_data->symbol_name_array_capacity) {
		if (subscr_data->symbol_name_array != NULL) {
			dx_free((void*)subscr_data->symbol_name_array);

			subscr_data->symbol_name_array_capacity = 0;
		}

		subscr_data->symbol_name_array = dx_calloc(subscr_data->symbols.size, sizeof(dxf_const_string_t));

		if (subscr_data->symbol_name_array == NULL) {
			return false;
		}

		subscr_data->symbol_name_array_capacity = subscr_data->symbols.size;
	}

	for (; i < subscr_data->symbols.size; ++i) {
		subscr_data->symbol_name_array[i] = subscr_data->symbols.elements[i]->name;
	}

	*symbols = subscr_data->symbol_name_array;
	*symbol_count = subscr_data->symbols.size;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_get_event_subscription_flags(dxf_subscription_t subscr_id, OUT dx_event_subscr_flag* subscr_flags) {
	dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (subscr_flags == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	*subscr_flags = subscr_data->subscr_flags;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_set_event_subscription_flags(dxf_subscription_t subscr_id, dx_event_subscr_flag subscr_flags) {
	dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
	dx_event_subscription_connection_context_t* context = NULL;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	context = subscr_data->escc;
	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));
	subscr_data->subscr_flags = subscr_flags;

	return dx_mutex_unlock(&(context->subscr_guard));
}

/* -------------------------------------------------------------------------- */

bool dx_get_event_subscription_time(dxf_subscription_t subscr_id, OUT dxf_long_t* time) {
	dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (time == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	*time = subscr_data->time;

	return true;
}

/* -------------------------------------------------------------------------- */

static void dx_call_subscr_listeners(dx_subscription_data_ptr_t subscr_data, int event_bitmask, dxf_const_string_t symbol_name, const dxf_event_data_t data, int data_count, const dxf_event_params_t* event_params) {
	size_t cur_listener_index = 0;
	for (; cur_listener_index < subscr_data->listeners.size; ++cur_listener_index) {
		dx_listener_context_t* listener_context = subscr_data->listeners.elements + cur_listener_index;
		if (listener_context->version == dx_elv_default) {
			dxf_event_listener_t listener = (dxf_event_listener_t)listener_context->listener;
			listener(event_bitmask, symbol_name, data, data_count, listener_context->user_data);
		}
		else if (listener_context->version == dx_elv_v2) {
			dxf_event_listener_v2_t listener = (dxf_event_listener_v2_t)listener_context->listener;
			listener(event_bitmask, symbol_name, data, data_count, event_params, listener_context->user_data);
		}
		else {
			dx_set_error_code(dx_esec_invalid_listener);
		}
	}
}

#define DX_ORDER_SOURCE_COMPARATOR_NONSYM(l, r) (dx_compare_strings(l.suffix, r))

void pass_event_data_to_listeners(dx_symbol_data_ptr_t symbol_data, dx_event_id_t event_id,
		dxf_const_string_t symbol_name, const dxf_event_data_t data, int data_count,
		const dxf_event_params_t* event_params) {
	size_t cur_subscr_index = 0;
	int event_bitmask = DX_EVENT_BIT_MASK(event_id);

	for (; cur_subscr_index < symbol_data->subscriptions.size; ++cur_subscr_index) {
		dx_subscription_data_ptr_t subscr_data = symbol_data->subscriptions.elements[cur_subscr_index];

		if (!(subscr_data->event_types & event_bitmask)) { /* subscription doesn't want this specific event type */
			continue;
		}

		/* Check order source, it must be done by record ids, really! */
		if (event_id == dx_eid_order && subscr_data->order_source.size) {
			/* We need to filter data in parts, unfortunately */
			dxf_order_t *orders = (dxf_order_t *)data;
			int start_index, end_index;
			end_index = start_index = 0;
			while (end_index != data_count) {
				/* Skip all non-needed orders */
				while (start_index < data_count) {
					bool found = false;
					size_t index = 0;
					DX_ARRAY_SEARCH(subscr_data->order_source.elements, 0, subscr_data->order_source.size, orders[start_index].source, DX_ORDER_SOURCE_COMPARATOR_NONSYM, false, found, index);
					if (!found)
						start_index++;
					else
						break;
				}
				if (start_index == data_count)
					break;
				/* Count all suitable orders */
				end_index = start_index;
				while (end_index < data_count) {
					bool found = false;
					size_t index = 0;
					DX_ARRAY_SEARCH(subscr_data->order_source.elements, 0, subscr_data->order_source.size, orders[end_index].source, DX_ORDER_SOURCE_COMPARATOR_NONSYM, false, found, index);
					if (found)
						end_index++;
					else
						break;
				}

				dx_call_subscr_listeners(subscr_data, event_bitmask, symbol_name, &orders[start_index], end_index - start_index, event_params);
			}
		} else {
			dx_call_subscr_listeners(subscr_data, event_bitmask, symbol_name, data, data_count, event_params);
		}
	}
}

bool dx_process_event_data (dxf_connection_t connection, dx_event_id_t event_id,
							dxf_const_string_t symbol_name, dxf_int_t symbol_cipher,
							const dxf_event_data_t data, int data_count,
							const dxf_event_params_t* event_params) {

	bool res;
	dx_event_subscription_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_event_subscription, &res);

	if (context == NULL) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return false;
	}

	if (event_id < dx_eid_begin || event_id >= dx_eid_count) {
		return dx_set_error_code(dx_esec_invalid_event_type);
	}

	/* this function is supposed to be called from a different thread than the other
	interface functions */
	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	dx_symbol_data_ptr_t symbol_data = dx_find_symbol(context, symbol_name, symbol_cipher);

	/* symbol_data == NULL is most likely a correct situation that occurred because the data is received very soon
	after the symbol subscription has been annulled */
	if (symbol_data != NULL) {
		dx_store_last_symbol_event(symbol_data, event_id, data, data_count);
		pass_event_data_to_listeners(symbol_data, event_id, symbol_name, data, data_count, event_params);
	}

	dx_symbol_data_ptr_t wildcard_symbol_data = dx_find_symbol(context, L"*", g_wildcard_cipher);

	if (wildcard_symbol_data != NULL) {
		dx_store_last_symbol_event(wildcard_symbol_data, event_id, data, data_count);
		pass_event_data_to_listeners(wildcard_symbol_data, event_id, symbol_name, data, data_count, event_params);
	}

	return dx_mutex_unlock(&(context->subscr_guard));
}

/* -------------------------------------------------------------------------- */
/*
 *	event type is a one-bit mask here
 */

bool dx_get_last_symbol_event (dxf_connection_t connection, dxf_const_string_t symbol_name, int event_type,
							OUT dxf_event_data_t* event_data) {
	dx_symbol_data_ptr_t symbol_data = NULL;
	dxf_int_t cipher = dx_encode_symbol_name(symbol_name);
	dx_event_id_t event_id;
	bool res;
	dx_event_subscription_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_event_subscription, &res);

	if (context == NULL) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return false;
	}

	if (event_data == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param);
	}

	if (!dx_is_only_single_bit_set(event_type)) {
		return dx_set_error_code(dx_esec_invalid_event_type);
	}

	event_id = dx_get_event_id_by_bitmask(event_type);

	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	if ((symbol_data = dx_find_symbol(context, symbol_name, cipher)) == NULL) {
		dx_mutex_unlock(&(context->subscr_guard));

		return dx_set_error_code(dx_esec_invalid_symbol_name);
	}

	dx_memcpy(symbol_data->last_events_accessed[event_id], symbol_data->last_events[event_id], dx_get_event_data_struct_size(event_id));

	*event_data = symbol_data->last_events_accessed[event_id];

	return dx_mutex_unlock(&(context->subscr_guard));
}

/* -------------------------------------------------------------------------- */

bool dx_process_connection_subscriptions (dxf_connection_t connection, dx_subscription_processor_t processor) {
	bool res;
	dx_event_subscription_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_event_subscription, &res);
	size_t i = 0;
	dx_subscription_data_array_t* subscriptions = NULL;

	if (context == NULL) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return false;
	}

	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	subscriptions = &(CTX(context)->subscriptions);

	for (; i < subscriptions->size; ++i) {
		dxf_const_string_t* symbols = NULL;
		size_t symbol_count = 0;
		int event_types = 0;
		dx_event_subscr_flag subscr_flags = 0;
		dxf_long_t time;

		if (!dx_get_event_subscription_symbols(subscriptions->elements[i], &symbols, &symbol_count) ||
			!dx_get_event_subscription_event_types(subscriptions->elements[i], &event_types) ||
			!dx_get_event_subscription_flags(subscriptions->elements[i], &subscr_flags) ||
			!dx_get_event_subscription_time(subscriptions->elements[i], &time) ||
			!processor(connection, dx_get_order_source(subscriptions->elements[i]), symbols, symbol_count, event_types, subscr_flags, time)) {

			dx_mutex_unlock(&(context->subscr_guard));

			return false;
		}
	}

	return dx_mutex_unlock(&(context->subscr_guard));
}

/* -------------------------------------------------------------------------- */

/* Functions for working with order source */

/* -------------------------------------------------------------------------- */

#define DX_ORDER_SOURCE_COMPARATOR(l, r) (dx_compare_strings(l.suffix, r.suffix))

bool dx_add_order_source(dxf_subscription_t subscr_id, dxf_const_string_t source) {
	dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;

	bool failed = false;
	dx_suffix_t new_source;
	bool found = false;
	size_t index = 0;
	dx_copy_string_len(new_source.suffix, source, DXF_RECORD_SUFFIX_SIZE);
	if (subscr_data->order_source.elements == NULL) {
		DX_ARRAY_INSERT(subscr_data->order_source, dx_suffix_t, new_source, index, dx_capacity_manager_halfer, failed);
	}
	else {
		DX_ARRAY_SEARCH(subscr_data->order_source.elements, 0, subscr_data->order_source.size, new_source, DX_ORDER_SOURCE_COMPARATOR, false, found, index);
		if (!found)
			DX_ARRAY_INSERT(subscr_data->order_source, dx_suffix_t, new_source, index, dx_capacity_manager_halfer, failed);
	}
	if (failed) {
		return dx_set_error_code(dx_sec_not_enough_memory);
	}
	return true;
}

/* -------------------------------------------------------------------------- */

void dx_clear_order_source(dxf_subscription_t subscr_id) {
	dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
	dx_free(subscr_data->order_source.elements);
	subscr_data->order_source.elements = NULL;
	subscr_data->order_source.size = 0;
	subscr_data->order_source.capacity = 0;
}

dx_order_source_array_ptr_t dx_get_order_source(dxf_subscription_t subscr_id) {
	dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
	return &subscr_data->order_source;
}
