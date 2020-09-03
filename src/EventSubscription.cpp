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
 * The Initial Developer of the Original Code is dxFeed Solutions DE GmbH.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "EventSubscription.h"

#include "ConnectionContextData.h"
#include "DXAlgorithms.h"
#include "DXErrorCodes.h"
#include "DXErrorHandling.h"
#include "DXFeed.h"
#include "Logger.h"
#include "DXThreads.h"
#include "SymbolCodec.h"

#ifdef __cplusplus
}
#endif

#include <new>
#include <functional>
#include <unordered_map>
#include <string>
#include <utility>
#include <cmath>
#include <vector>

/* -------------------------------------------------------------------------- */
/*
 *	Internal data structures and objects
 */
/* -------------------------------------------------------------------------- */

const dxf_const_string_t dx_all_order_sources[] = {
	L"NTV",	  /// NASDAQ Total View.
	L"ntv",	  /// NASDAQ Total View. Record for price level book.
	L"NFX",	  /// NASDAQ Futures Exchange.
	L"ESPD",  /// NASDAQ eSpeed.
	L"XNFI",  /// NASDAQ Fixed Income.
	L"ICE",	  /// Intercontinental Exchange.
	L"ISE",	  /// International Securities Exchange.
	L"DEA",	  /// Direct-Edge EDGA Exchange.
	L"DEX",	  /// Direct-Edge EDGX Exchange.
	L"BYX",	  /// Bats BYX Exchange.
	L"BZX",	  /// Bats BZX Exchange.
	L"BATE",  /// Bats Europe BXE Exchange.
	L"CHIX",  /// Bats Europe CXE Exchange.
	L"CEUX",  /// Bats Europe DXE Exchange.
	L"BXTR",  /// Bats Europe TRF.
	L"IST",	  /// Borsa Istanbul Exchange.
	L"BI20",  /// Borsa Istanbul Exchange. Record for particular top 20 order book.
	L"ABE",	  /// ABE (abe.io) exchange.
	L"FAIR",  /// FAIR (FairX) exchange.
	L"GLBX",  /// CME Globex.
	L"glbx",  /// CME Globex. Record for price level book.
	L"ERIS",  /// Eris Exchange group of companies.
	L"XEUR",  /// Eurex Exchange.
	L"xeur",  /// Eurex Exchange. Record for price level book.
	L"CFE",	  /// CBOE Futures Exchange.
	L"C2OX",  /// CBOE Options C2 Exchange.
	L"SMFE",  /// Small Exchange.
	nullptr};
const size_t dx_all_order_sources_count = sizeof(dx_all_order_sources) / sizeof(dx_all_order_sources[0]) - 1;

const size_t dx_all_regional_count = 26;

template <typename SizeT>
inline SizeT hash_combine(SizeT seed, SizeT value) noexcept {
	return seed ^ (value + 0x9e3779b9 + (seed << 6u) + (seed >> 2u));
}

struct SubscriptionData;

typedef struct {
	SubscriptionData** elements;
	size_t size;
	size_t capacity;
} dx_subscription_data_array_t;

struct SymbolData {
	dxf_const_string_t name;
	dxf_int_t cipher;
	int ref_count;
	dx_subscription_data_array_t subscriptions;
	dxf_event_data_t* last_events;
	dxf_event_data_t* last_events_accessed;
};

typedef void* dx_event_listener_ptr_t;

typedef enum { dx_elv_default = 1, dx_elv_v2 = 2 } dx_event_listener_version_t;

class ListenerContext {
	union {
		dxf_event_listener_t listener;
		dxf_event_listener_v2_t listener_v2;
	};
	dx_event_listener_version_t version;
	void* user_data;

   public:
	ListenerContext(dx_event_listener_ptr_t listener, dx_event_listener_version_t version, void* user_data)
		: version{version}, user_data{user_data} {
		if (version == dx_elv_default) {
			this->listener = (dxf_event_listener_t)listener;
		} else {
			this->listener_v2 = (dxf_event_listener_v2_t)listener;
		}
	}

	dx_event_listener_ptr_t getListener() {
		if (version == dx_elv_default) {
			return (dx_event_listener_ptr_t)listener;
		} else {
			return (dx_event_listener_ptr_t)listener_v2;
		}
	}

	dx_event_listener_version_t getVersion() { return version; }

	void* getUserData() { return user_data; }
};

typedef struct {
	ListenerContext* elements;
	size_t size;
	size_t capacity;
} dx_listener_array_t;

typedef struct {
	SymbolData** elements;
	size_t size;
	size_t capacity;
} dx_symbol_data_array_t;

struct SubscriptionData {
	unsigned event_types;
	std::unordered_map<std::wstring, SymbolData*> symbols;
	dx_listener_array_t listeners;
	dx_order_source_array_t order_source;
	dx_event_subscr_flag subscr_flags;
	dxf_long_t time;

	std::vector<std::wstring> symbol_names;
	dxf_const_string_t* symbol_name_array;
	size_t symbol_name_array_capacity;

	void* escc; /* event subscription connection context */
};

/* -------------------------------------------------------------------------- */
/*
 *	Symbol map structures
 */
/* -------------------------------------------------------------------------- */

#define SYMBOL_BUCKET_COUNT 1000

/* -------------------------------------------------------------------------- */
/*
 *	Event subscription connection context
 */
/* -------------------------------------------------------------------------- */

struct EventSubscriptionConnectionContext {
	dxf_connection_t connection;
	dx_mutex_t subscr_guard;
	std::unordered_map<std::wstring, SymbolData*> symbols{};
	dx_subscription_data_array_t subscriptions;
	unsigned set_fields_flags;
};

#define MUTEX_FIELD_FLAG (0x1u)

#define CTX(context) ((EventSubscriptionConnectionContext*)context)

/* -------------------------------------------------------------------------- */

int dx_clear_event_subscription_connection_context(EventSubscriptionConnectionContext* context);

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_event_subscription) {
	CHECKED_CALL_2(dx_validate_connection_handle, connection, true);

	auto* context = new(std::nothrow) EventSubscriptionConnectionContext{};

	if (context == nullptr) {
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
	int res = true;
	auto* context = static_cast<EventSubscriptionConnectionContext*>(
		dx_get_subsystem_data(connection, dx_ccs_event_subscription, &res));

	if (context == nullptr) {
		return res;
	}

	return dx_clear_event_subscription_connection_context(context);
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_event_subscription) { return true; }

/* -------------------------------------------------------------------------- */

int dx_clear_event_subscription_connection_context(EventSubscriptionConnectionContext* context) {
	int res = true;
	size_t i = 0;

	for (; i < context->subscriptions.size; ++i) {
		res = dx_close_event_subscription((dxf_subscription_t)context->subscriptions.elements[i]) && res;
	}

	if (IS_FLAG_SET(context->set_fields_flags, MUTEX_FIELD_FLAG)) {
		res = dx_mutex_destroy(&(context->subscr_guard)) && res;
	}

	if (context->subscriptions.elements != nullptr) {
		dx_free(context->subscriptions.elements);
	}

	delete context;

	return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	Helper functions
 */
/* -------------------------------------------------------------------------- */

dxf_int_t dx_symbol_name_hasher(dxf_const_string_t symbol_name) {
	return std::hash<std::wstring>{}(symbol_name);
}

/* -------------------------------------------------------------------------- */

void dx_cleanup_event_data_array(dxf_event_data_t* data_array) {
	int i = dx_eid_begin;

	if (data_array == nullptr) {
		return;
	}

	for (; i < dx_eid_count; ++i) {
		CHECKED_FREE(data_array[i]);
	}

	dx_free(data_array);
}

/* ---------------------------------- */

SymbolData* dx_cleanup_symbol_data(SymbolData* symbol_data) {
	if (symbol_data == nullptr) {
		return nullptr;
	}

	CHECKED_FREE(symbol_data->name);

	dx_cleanup_event_data_array(symbol_data->last_events);
	dx_cleanup_event_data_array(symbol_data->last_events_accessed);

	delete symbol_data;

	return nullptr;
}

/* -------------------------------------------------------------------------- */

SymbolData* dx_create_symbol_data(dxf_const_string_t name, dxf_int_t cipher) {
	int i = dx_eid_begin;
	auto res = new (std::nothrow) SymbolData{};

	if (res == nullptr) {
		return res;
	}

	res->name = dx_create_string_src(name);
	res->cipher = cipher;

	if (res->name == nullptr) {
		return dx_cleanup_symbol_data(res);
	}

	res->last_events = static_cast<dxf_event_data_t*>(dx_calloc(dx_eid_count, sizeof(dxf_event_data_t)));
	res->last_events_accessed = static_cast<dxf_event_data_t*>(dx_calloc(dx_eid_count, sizeof(dxf_event_data_t)));

	if (res->last_events == nullptr || res->last_events_accessed == nullptr) {
		return dx_cleanup_symbol_data(res);
	}

	for (; i < dx_eid_count; ++i) {
		res->last_events[i] = dx_calloc(1, dx_get_event_data_struct_size(i));
		res->last_events_accessed[i] = dx_calloc(1, dx_get_event_data_struct_size(i));

		if (res->last_events[i] == nullptr || res->last_events_accessed[i] == nullptr) {
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

SymbolData* dx_subscribe_symbol(EventSubscriptionConnectionContext* context, dxf_const_string_t symbol_name,
								SubscriptionData* owner) {
	SymbolData* res;
	int is_just_created = false;

	{
		SymbolData dummy{};

		auto found = context->symbols.find(std::wstring(symbol_name));

		if (found == context->symbols.end()) {
			res = dx_create_symbol_data(symbol_name, dummy.cipher);

			if (res == nullptr) {
				return nullptr;
			}

			is_just_created = true;

			context->symbols[symbol_name] = res;
		} else {
			res = found->second;
		}
	}

	{
		int subscr_exists;
		size_t subscr_index;
		int failed;

		DX_ARRAY_SEARCH(res->subscriptions.elements, 0, res->subscriptions.size, owner, DX_NUMERIC_COMPARATOR, false,
						subscr_exists, subscr_index);

		if (subscr_exists) {
			return res;
		}

		DX_ARRAY_INSERT(res->subscriptions, SubscriptionData*, owner, subscr_index, dx_capacity_manager_halfer, failed);

		if (failed) {
			return (is_just_created ? dx_cleanup_symbol_data(res) : nullptr);
		}

		++(res->ref_count);
	}

	return res;
}

/* -------------------------------------------------------------------------- */

int dx_unsubscribe_symbol(EventSubscriptionConnectionContext* context, SymbolData* symbol_data,
						   SubscriptionData* owner) {
	int res = true;

	do {
		int subscr_exists;
		size_t subscr_index;
		int failed;

		DX_ARRAY_SEARCH(symbol_data->subscriptions.elements, 0, symbol_data->subscriptions.size, owner,
						DX_NUMERIC_COMPARATOR, false, subscr_exists, subscr_index);

		if (!subscr_exists) {
			/* should never be here */

			dx_set_error_code(dx_ec_internal_assert_violation);

			res = false;

			break;
		}

		DX_ARRAY_DELETE(symbol_data->subscriptions, SubscriptionData*, subscr_index, dx_capacity_manager_halfer,
						failed);

		if (failed) {
			/* most probably the memory allocation error */

			res = false;
		}
	} while (false);

	if (--(symbol_data->ref_count) == 0) {
		auto found = context->symbols.find(std::wstring(symbol_data->name));

		if (found == context->symbols.end()) {
			/* the symbol must've had been found in the container */
			dx_set_error_code(dx_ec_internal_assert_violation);

			res = false;
		} else {
			context->symbols.erase(found);
		}

		dx_cleanup_symbol_data(symbol_data);
	}

	return res;
}

/* -------------------------------------------------------------------------- */

void dx_clear_listener_array(dx_listener_array_t* listeners) {
	dx_free(listeners->elements);

	listeners->elements = nullptr;
	listeners->size = 0;
	listeners->capacity = 0;
}

/* -------------------------------------------------------------------------- */

int dx_clear_symbol_array(EventSubscriptionConnectionContext* context,
						   std::unordered_map<std::wstring, SymbolData*>& symbols, SubscriptionData* owner) {
	int res = true;

	for (auto&& s : symbols) {
		res = dx_unsubscribe_symbol(context, s.second, owner) && res;
	}

	symbols.clear();

	return res;
}

/* -------------------------------------------------------------------------- */

int dx_listener_comparator(ListenerContext e1, ListenerContext e2) {
	dx_event_listener_ptr_t l1 = e1.getListener();
	dx_event_listener_ptr_t l2 = e2.getListener();

	return DX_NUMERIC_COMPARATOR(l1, l2);
}

/* -------------------------------------------------------------------------- */

size_t dx_find_listener_in_array(dx_listener_array_t* listeners, dx_event_listener_ptr_t listener, OUT int* found) {
	size_t listener_index;
	/* There is comparing listeners by address, so fill version and user_data with default values */
	ListenerContext listener_context{listener, dx_elv_default, nullptr};

	DX_ARRAY_SEARCH(listeners->elements, 0, listeners->size, listener_context, dx_listener_comparator, false, *found,
					listener_index);

	return listener_index;
}

/* -------------------------------------------------------------------------- */

SymbolData* dx_find_symbol(EventSubscriptionConnectionContext* context, dxf_const_string_t symbol_name) {
	auto found = context->symbols.find(std::wstring(symbol_name));

	if (found == context->symbols.end()) {
		return nullptr;
	}

	return found->second;
}

/* -------------------------------------------------------------------------- */

void dx_store_last_symbol_event(SymbolData* symbol_data, dx_event_id_t event_id, dxf_const_event_data_t data,
								int data_count) {
	dx_memcpy(symbol_data->last_events[event_id],
			  dx_get_event_data_item(DX_EVENT_BIT_MASK(event_id), data, data_count - 1),
			  dx_get_event_data_struct_size(event_id));
}

/* -------------------------------------------------------------------------- */

int dx_add_subscription_to_context(SubscriptionData* subscr_data) {
	int subscr_exists;
	size_t subscr_index;
	int failed;
	dx_subscription_data_array_t* subscriptions = &(CTX(subscr_data->escc)->subscriptions);

	DX_ARRAY_SEARCH(subscriptions->elements, 0, subscriptions->size, subscr_data, DX_NUMERIC_COMPARATOR, false,
					subscr_exists, subscr_index);

	if (subscr_exists) {
		/* should never be here */

		return dx_set_error_code(dx_ec_internal_assert_violation);
	}

	DX_ARRAY_INSERT(*subscriptions, SubscriptionData*, subscr_data, subscr_index, dx_capacity_manager_halfer, failed);

	return !failed;
}

/* -------------------------------------------------------------------------- */

int dx_remove_subscription_from_context(SubscriptionData* subscr_data) {
	int subscr_exists;
	size_t subscr_index;
	int failed;
	dx_subscription_data_array_t* subscriptions = &(CTX(subscr_data->escc)->subscriptions);

	DX_ARRAY_SEARCH(subscriptions->elements, 0, subscriptions->size, subscr_data, DX_NUMERIC_COMPARATOR, false,
					subscr_exists, subscr_index);

	if (!subscr_exists) {
		/* should never be here */

		return dx_set_error_code(dx_ec_internal_assert_violation);
	}

	DX_ARRAY_DELETE(*subscriptions, SubscriptionData*, subscr_index, dx_capacity_manager_halfer, failed);

	if (failed) {
		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

void dx_free_event_subscription_data(SubscriptionData* subscr_data) {
	if (subscr_data == nullptr) {
		return;
	}

	/* no element freeing is performed because this array only stores the pointers to the strings allocated somewhere
	 * else */
	CHECKED_FREE(subscr_data->symbol_name_array);
	dx_clear_order_source(subscr_data);

	delete subscr_data;
}

/* -------------------------------------------------------------------------- */
/*
 *	Subscription functions implementation
 */
/* -------------------------------------------------------------------------- */

const dxf_subscription_t dx_invalid_subscription = (dxf_subscription_t) nullptr;

dxf_subscription_t dx_create_event_subscription(dxf_connection_t connection, unsigned event_types,
												dx_event_subscr_flag subscr_flags, dxf_long_t time) {
	if (!dx_validate_connection_handle(connection, false)) {
		return dx_invalid_subscription;
	}

	int res = true;

	auto context = static_cast<EventSubscriptionConnectionContext*>(
		dx_get_subsystem_data(connection, dx_ccs_event_subscription, &res));

	if (context == nullptr) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return dx_invalid_subscription;
	}

	if (event_types & DXF_ET_UNUSED) {
		dx_set_error_code(dx_esec_invalid_event_type);

		return dx_invalid_subscription;
	}

	auto* subscr_data = new (std::nothrow) SubscriptionData{};

	if (subscr_data == nullptr) {
		return dx_invalid_subscription;
	}

	subscr_data->event_types = event_types;
	subscr_data->escc = context;
	subscr_data->subscr_flags = subscr_flags;
	subscr_data->time = time;

	res = true;
	int i = 0;
	if (event_types & DXF_ET_ORDER) {
		for (; dx_all_order_sources[i]; i++) {
			res &= dx_add_order_source(subscr_data, dx_all_order_sources[i]);
		}
		res &= dx_add_order_source(subscr_data, L"");  // Used by MM
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

		subscr_data = (SubscriptionData*)dx_invalid_subscription;
	}

	return (dx_mutex_unlock(&(context->subscr_guard)) ? (dxf_subscription_t)subscr_data : dx_invalid_subscription);
}

/* -------------------------------------------------------------------------- */

int dx_close_event_subscription(dxf_subscription_t subscr_id) {
	auto subscr_data = (SubscriptionData*)subscr_id;
	int res = true;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	auto* context = static_cast<EventSubscriptionConnectionContext*>(subscr_data->escc);

	/* locking a guard mutex */
	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	res = dx_clear_symbol_array(context, subscr_data->symbols, subscr_data) && res;
	dx_clear_listener_array(&(subscr_data->listeners));

	if (!dx_remove_subscription_from_context(subscr_data)) {
		res = false;
	}

	res = dx_mutex_unlock(&(context->subscr_guard)) && res;

	dx_free_event_subscription_data(subscr_data);

	return res;
}

/* -------------------------------------------------------------------------- */

int dx_add_symbols(dxf_subscription_t subscr_id, dxf_const_string_t* symbols, int symbol_count) {
	auto subscr_data = (SubscriptionData*)subscr_id;
	int cur_symbol_index = 0;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	auto* context = static_cast<EventSubscriptionConnectionContext*>(subscr_data->escc);

	/* a guard mutex is required to protect the internal containers
	from the secondary data retriever threads */
	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	for (; cur_symbol_index < symbol_count; ++cur_symbol_index) {
		if (dx_string_null_or_empty(symbols[cur_symbol_index])) {
			continue;
		}

		auto found = subscr_data->symbols.find(std::wstring(symbols[cur_symbol_index]));

		if (found != subscr_data->symbols.end()) {
			/* symbol is already subscribed */
			continue;
		}

		auto symbol_data = dx_subscribe_symbol(context, symbols[cur_symbol_index], subscr_data);

		if (symbol_data == nullptr) {
			dx_mutex_unlock(&(context->subscr_guard));

			return false;
		}

		subscr_data->symbols[symbols[cur_symbol_index]] = symbol_data;
	}

	return dx_mutex_unlock(&(context->subscr_guard));
}

/* -------------------------------------------------------------------------- */

int dx_remove_symbols(dxf_subscription_t subscr_id, dxf_const_string_t* symbols, size_t symbol_count) {
	auto subscr_data = (SubscriptionData*)subscr_id;
	size_t cur_symbol_index = 0;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	auto* context = static_cast<EventSubscriptionConnectionContext*>(subscr_data->escc);

	/* a guard mutex is required to protect the internal containers
	from the secondary data retriever threads */
	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	for (; cur_symbol_index < symbol_count; ++cur_symbol_index) {
		if (dx_string_null_or_empty(symbols[cur_symbol_index])) {
			continue;
		}

		auto found = subscr_data->symbols.find(std::wstring(symbols[cur_symbol_index]));

		if (found == subscr_data->symbols.end()) {
			/* symbol wasn't subscribed */
			continue;
		}

		if (!dx_unsubscribe_symbol(context, found->second, subscr_data)) {
			dx_mutex_unlock(&(context->subscr_guard));

			return false;
		}

		subscr_data->symbols.erase(found);
	}

	return dx_mutex_unlock(&(context->subscr_guard));
}

/* -------------------------------------------------------------------------- */

int dx_add_listener_impl(dxf_subscription_t subscr_id, dx_event_listener_ptr_t listener,
						  dx_event_listener_version_t version, void* user_data) {
	auto subscr_data = (SubscriptionData*)subscr_id;
	size_t listener_index;
	int failed;
	int found = false;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (listener == nullptr) {
		return dx_set_error_code(dx_esec_invalid_listener);
	}

	auto* context = static_cast<EventSubscriptionConnectionContext*>(subscr_data->escc);

	/* a guard mutex is required to protect the internal containers
	from the secondary data retriever threads */
	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	listener_index = dx_find_listener_in_array(&(subscr_data->listeners), listener, &found);

	if (found) {
		/* listener is already added */
		return dx_mutex_unlock(&(context->subscr_guard));
	}

	{
		ListenerContext listener_context = {listener, version, user_data};

		DX_ARRAY_INSERT(subscr_data->listeners, ListenerContext, listener_context, listener_index,
						dx_capacity_manager_halfer, failed);
	}

	return dx_mutex_unlock(&(context->subscr_guard)) && !failed;
}

/* -------------------------------------------------------------------------- */

int dx_add_listener(dxf_subscription_t subscr_id, dxf_event_listener_t listener, void* user_data) {
	return dx_add_listener_impl(subscr_id, (dx_event_listener_ptr_t)listener, dx_elv_default, user_data);
}

/* -------------------------------------------------------------------------- */

int dx_add_listener_v2(dxf_subscription_t subscr_id, dxf_event_listener_v2_t listener, void* user_data) {
	return dx_add_listener_impl(subscr_id, (dx_event_listener_ptr_t)listener, dx_elv_v2, user_data);
}

/* -------------------------------------------------------------------------- */

int dx_remove_listener_impl(dxf_subscription_t subscr_id, dx_event_listener_ptr_t listener) {
	auto subscr_data = (SubscriptionData*)subscr_id;
	size_t listener_index;
	int failed;
	int found = false;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (listener == nullptr) {
		return dx_set_error_code(dx_esec_invalid_listener);
	}

	auto* context = static_cast<EventSubscriptionConnectionContext*>(subscr_data->escc);
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

	DX_ARRAY_DELETE(subscr_data->listeners, ListenerContext, listener_index, dx_capacity_manager_halfer, failed);

	return dx_mutex_unlock(&(context->subscr_guard)) && !failed;
}

int dx_remove_listener(dxf_subscription_t subscr_id, dxf_event_listener_t listener) {
	return dx_remove_listener_impl(subscr_id, (dx_event_listener_ptr_t)listener);
}

/* -------------------------------------------------------------------------- */

int dx_remove_listener_v2(dxf_subscription_t subscr_id, dxf_event_listener_v2_t listener) {
	return dx_remove_listener_impl(subscr_id, (dx_event_listener_ptr_t)listener);
}

/* -------------------------------------------------------------------------- */

int dx_get_subscription_connection(dxf_subscription_t subscr_id, OUT dxf_connection_t* connection) {
	auto subscr_data = (SubscriptionData*)subscr_id;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (connection == nullptr) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	*connection = CTX(subscr_data->escc)->connection;

	return true;
}

/* -------------------------------------------------------------------------- */

int dx_get_event_subscription_event_types(dxf_subscription_t subscr_id, OUT unsigned* event_types) {
	auto subscr_data = (SubscriptionData*)subscr_id;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (event_types == nullptr) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	*event_types = subscr_data->event_types;

	return true;
}

/* -------------------------------------------------------------------------- */

int dx_get_event_subscription_symbols(dxf_subscription_t subscr_id, OUT dxf_const_string_t** symbols,
									   OUT size_t* symbol_count) {
	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	auto subscr_data = (SubscriptionData*)subscr_id;

	if (symbols == nullptr || symbol_count == nullptr) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	if (subscr_data->symbols.size() > subscr_data->symbol_name_array_capacity) {
		if (subscr_data->symbol_name_array != nullptr) {
			dx_free((void*)subscr_data->symbol_name_array);

			subscr_data->symbol_name_array_capacity = 0;
		}

		subscr_data->symbol_name_array =
			static_cast<dxf_const_string_t*>(dx_calloc(subscr_data->symbols.size(), sizeof(dxf_const_string_t)));

		if (subscr_data->symbol_name_array == nullptr) {
			return false;
		}

		subscr_data->symbol_name_array_capacity = subscr_data->symbols.size();
	}

	std::size_t i = 0;
	for (auto&& s : subscr_data->symbols) {
		subscr_data->symbol_name_array[i] = s.second->name;
		i++;
	}

	*symbols = subscr_data->symbol_name_array;
	*symbol_count = subscr_data->symbols.size();

	return true;
}

/* -------------------------------------------------------------------------- */

int dx_get_event_subscription_flags(dxf_subscription_t subscr_id, OUT dx_event_subscr_flag* subscr_flags) {
	auto subscr_data = (SubscriptionData*)subscr_id;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (subscr_flags == nullptr) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	*subscr_flags = subscr_data->subscr_flags;

	return true;
}

/* -------------------------------------------------------------------------- */

int dx_set_event_subscription_flags(dxf_subscription_t subscr_id, dx_event_subscr_flag subscr_flags) {
	auto subscr_data = (SubscriptionData*)subscr_id;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	auto* context = static_cast<EventSubscriptionConnectionContext*>(subscr_data->escc);
	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));
	subscr_data->subscr_flags = subscr_flags;

	return dx_mutex_unlock(&(context->subscr_guard));
}

/* -------------------------------------------------------------------------- */

int dx_get_event_subscription_time(dxf_subscription_t subscr_id, OUT dxf_long_t* time) {
	auto subscr_data = (SubscriptionData*)subscr_id;

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (time == nullptr) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	*time = subscr_data->time;

	return true;
}

/* -------------------------------------------------------------------------- */

static void dx_call_subscr_listeners(SubscriptionData* subscr_data, unsigned event_bitmask,
									 dxf_const_string_t symbol_name, dxf_const_event_data_t data, int data_count,
									 const dxf_event_params_t* event_params) {
	size_t cur_listener_index = 0;
	for (; cur_listener_index < subscr_data->listeners.size; ++cur_listener_index) {
		ListenerContext* listener_context = subscr_data->listeners.elements + cur_listener_index;
		if (listener_context->getVersion() == dx_elv_default) {
			auto listener = (dxf_event_listener_t)listener_context->getListener();
			listener(event_bitmask, symbol_name, static_cast<dxf_event_data_t const*>(data), data_count,
					 listener_context->getUserData());
		} else if (listener_context->getVersion() == dx_elv_v2) {
			auto listener = (dxf_event_listener_v2_t)listener_context->getListener();
			listener(event_bitmask, symbol_name, static_cast<dxf_event_data_t const*>(data), data_count, event_params,
					 listener_context->getUserData());
		} else {
			dx_set_error_code(dx_esec_invalid_listener);
		}
	}
}

#define DX_ORDER_SOURCE_COMPARATOR_NONSYM(l, r) (dx_compare_strings(l.suffix, r))

void pass_event_data_to_listeners(SymbolData* symbol_data, dx_event_id_t event_id, dxf_const_string_t symbol_name,
								  dxf_const_event_data_t data, int data_count, const dxf_event_params_t* event_params) {
	size_t cur_subscr_index = 0;
	unsigned event_bitmask = DX_EVENT_BIT_MASK(event_id);

	for (; cur_subscr_index < symbol_data->subscriptions.size; ++cur_subscr_index) {
		SubscriptionData* subscr_data = symbol_data->subscriptions.elements[cur_subscr_index];

		if (!(subscr_data->event_types & event_bitmask)) { /* subscription doesn't want this specific event type */
			continue;
		}

		/* Check order source, it must be done by record ids, really! */
		if (event_id == dx_eid_order && subscr_data->order_source.size) {
			/* We need to filter data in parts, unfortunately */
			auto* orders = (dxf_order_t*)data;
			int start_index, end_index;
			end_index = start_index = 0;
			while (end_index != data_count) {
				/* Skip all non-needed orders */
				while (start_index < data_count) {
					int found;
					size_t index;
					DX_ARRAY_SEARCH(subscr_data->order_source.elements, 0, subscr_data->order_source.size,
									orders[start_index].source, DX_ORDER_SOURCE_COMPARATOR_NONSYM, false, found, index);
					if (!found)
						start_index++;
					else
						break;
				}
				if (start_index == data_count) break;
				/* Count all suitable orders */
				end_index = start_index;
				while (end_index < data_count) {
					int found;
					size_t index;
					DX_ARRAY_SEARCH(subscr_data->order_source.elements, 0, subscr_data->order_source.size,
									orders[end_index].source, DX_ORDER_SOURCE_COMPARATOR_NONSYM, false, found, index);
					if (found)
						end_index++;
					else
						break;
				}

				dx_call_subscr_listeners(subscr_data, event_bitmask, symbol_name, &orders[start_index],
										 end_index - start_index, event_params);
			}
		} else {
			dx_call_subscr_listeners(subscr_data, event_bitmask, symbol_name, data, data_count, event_params);
		}
	}
}

int dx_process_event_data(dxf_connection_t connection, dx_event_id_t event_id, dxf_const_string_t symbol_name,
						   dxf_int_t symbol_cipher, dxf_const_event_data_t data, int data_count,
						   const dxf_event_params_t* event_params) {
	int res;
	auto* context = static_cast<EventSubscriptionConnectionContext*>(
		dx_get_subsystem_data(connection, dx_ccs_event_subscription, &res));

	if (context == nullptr) {
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

	SymbolData* symbol_data = dx_find_symbol(context, symbol_name);

	/* symbol_data == nullptr is most likely a correct situation that occurred because the data is received very soon
	after the symbol subscription has been annulled */
	if (symbol_data != nullptr) {
		dx_store_last_symbol_event(symbol_data, event_id, data, data_count);
		pass_event_data_to_listeners(symbol_data, event_id, symbol_name, data, data_count, event_params);
	}

	SymbolData* wildcard_symbol_data = dx_find_symbol(context, L"*");

	if (wildcard_symbol_data != nullptr) {
		dx_store_last_symbol_event(wildcard_symbol_data, event_id, data, data_count);
		pass_event_data_to_listeners(wildcard_symbol_data, event_id, symbol_name, data, data_count, event_params);
	}

	return dx_mutex_unlock(&(context->subscr_guard));
}

/* -------------------------------------------------------------------------- */
/*
 *	event type is a one-bit mask here
 */

int dx_get_last_symbol_event(dxf_connection_t connection, dxf_const_string_t symbol_name, int event_type,
							  OUT dxf_event_data_t* event_data) {
	SymbolData* symbol_data;
	dxf_int_t cipher = dx_encode_symbol_name(symbol_name);
	dx_event_id_t event_id;
	int res;
	auto* context = static_cast<EventSubscriptionConnectionContext*>(
		dx_get_subsystem_data(connection, dx_ccs_event_subscription, &res));

	if (context == nullptr) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return false;
	}

	if (event_data == nullptr) {
		return dx_set_error_code(dx_ec_invalid_func_param);
	}

	if (!dx_is_only_single_bit_set(event_type)) {
		return dx_set_error_code(dx_esec_invalid_event_type);
	}

	event_id = dx_get_event_id_by_bitmask(event_type);

	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	if ((symbol_data = dx_find_symbol(context, symbol_name)) == nullptr) {
		dx_mutex_unlock(&(context->subscr_guard));

		return dx_set_error_code(dx_esec_invalid_symbol_name);
	}

	dx_memcpy(symbol_data->last_events_accessed[event_id], symbol_data->last_events[event_id],
			  dx_get_event_data_struct_size(event_id));

	*event_data = symbol_data->last_events_accessed[event_id];

	return dx_mutex_unlock(&(context->subscr_guard));
}

/* -------------------------------------------------------------------------- */

int dx_process_connection_subscriptions(dxf_connection_t connection, dx_subscription_processor_t processor) {
	int res;
	auto* context = static_cast<EventSubscriptionConnectionContext*>(
		dx_get_subsystem_data(connection, dx_ccs_event_subscription, &res));
	size_t i = 0;

	if (context == nullptr) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return false;
	}

	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	dx_subscription_data_array_t* subscriptions = &(CTX(context)->subscriptions);

	for (; i < subscriptions->size; ++i) {
		dxf_const_string_t* symbols = nullptr;
		size_t symbol_count = 0;
		unsigned event_types = 0;
		auto subscr_flags = static_cast<dx_event_subscr_flag>(0);
		dxf_long_t time;

		if (!dx_get_event_subscription_symbols(subscriptions->elements[i], &symbols, &symbol_count) ||
			!dx_get_event_subscription_event_types(subscriptions->elements[i], &event_types) ||
			!dx_get_event_subscription_flags(subscriptions->elements[i], &subscr_flags) ||
			!dx_get_event_subscription_time(subscriptions->elements[i], &time) ||
			!processor(connection, dx_get_order_source(subscriptions->elements[i]), symbols, symbol_count, event_types,
					   subscr_flags, time)) {
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

int dx_add_order_source(dxf_subscription_t subscr_id, dxf_const_string_t source) {
	auto subscr_data = (SubscriptionData*)subscr_id;

	int failed = false;
	dx_suffix_t new_source;
	int found;
	size_t index = 0;
	dx_copy_string_len(new_source.suffix, source, DXF_RECORD_SUFFIX_SIZE);
	if (subscr_data->order_source.elements == nullptr) {
		DX_ARRAY_INSERT(subscr_data->order_source, dx_suffix_t, new_source, index, dx_capacity_manager_halfer, failed);
	} else {
		DX_ARRAY_SEARCH(subscr_data->order_source.elements, 0, subscr_data->order_source.size, new_source,
						DX_ORDER_SOURCE_COMPARATOR, false, found, index);
		if (!found)
			DX_ARRAY_INSERT(subscr_data->order_source, dx_suffix_t, new_source, index, dx_capacity_manager_halfer,
							failed);
	}
	if (failed) {
		return dx_set_error_code(dx_sec_not_enough_memory);
	}
	return true;
}

/* -------------------------------------------------------------------------- */

void dx_clear_order_source(dxf_subscription_t subscr_id) {
	auto subscr_data = (SubscriptionData*)subscr_id;
	dx_free(subscr_data->order_source.elements);
	subscr_data->order_source.elements = nullptr;
	subscr_data->order_source.size = 0;
	subscr_data->order_source.capacity = 0;
}

dx_order_source_array_ptr_t dx_get_order_source(dxf_subscription_t subscr_id) {
	auto subscr_data = (SubscriptionData*)subscr_id;
	return &subscr_data->order_source;
}
