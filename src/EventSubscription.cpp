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
#include "DXThreads.h"
#include "Logger.h"
#include "SymbolCodec.h"

#ifdef __cplusplus
}
#endif

#include <cmath>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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

struct SubscriptionData;

struct SymbolData {
	std::wstring name{};
	int ref_count;

	std::unordered_set<SubscriptionData*> subscriptions{};
	dxf_event_data_t* last_events;
	dxf_event_data_t* last_events_accessed;
};

typedef void* ListenerPtr;

enum class EventListenerVersion{ Default = 1, V2 = 2 };

class ListenerContext {
	union {
		dxf_event_listener_t listener;
		dxf_event_listener_v2_t listener_v2;
	};
	EventListenerVersion version;
	void* user_data;

	public:

	ListenerContext(ListenerPtr listener, EventListenerVersion version, void* user_data) noexcept
		: version{version}, user_data{user_data} {
		if (version == EventListenerVersion::Default) {
			this->listener = (dxf_event_listener_t)listener;
		} else {
			this->listener_v2 = (dxf_event_listener_v2_t)listener;
		}
	}

	ListenerPtr getListener() const noexcept {
		if (version == EventListenerVersion::Default) {
			return (ListenerPtr)listener;
		} else {
			return (ListenerPtr)listener_v2;
		}
	}

	EventListenerVersion getVersion() const noexcept { return version; }

	static ListenerContext createDummy(ListenerPtr listener) {
		return {listener, EventListenerVersion::Default, nullptr};
	}

	void* getUserData() const noexcept { return user_data; }

	friend bool operator == (const ListenerContext& lc1, const ListenerContext& lc2) {
		return lc1.getListener() == lc2.getListener();
	}
};

namespace std {

template<>
struct hash<ListenerContext> {
	bool operator()(const ListenerContext& lc) const noexcept {
		return std::hash<ListenerPtr>{}(lc.getListener());
	}
};

}

struct SubscriptionData {
	unsigned event_types;
	std::unordered_map<std::wstring, SymbolData*> symbols{};
	std::unordered_set<ListenerContext> listeners{};
	dx_order_source_array_t order_source;
	dx_event_subscr_flag subscr_flags;
	dxf_long_t time;

	std::vector<dxf_const_string_t> symbol_names{};

	void* connection_context; /* event subscription connection context */
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
	std::unordered_set<SubscriptionData*> subscriptions{};
	unsigned set_fields_flags;
};

#define MUTEX_FIELD_FLAG (0x1u)

#define CTX(context) ((EventSubscriptionConnectionContext*)context)

/* -------------------------------------------------------------------------- */

int dx_clear_event_subscription_connection_context(EventSubscriptionConnectionContext* context);

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_event_subscription) {
	CHECKED_CALL_2(dx_validate_connection_handle, connection, true);

	auto context = new (std::nothrow) EventSubscriptionConnectionContext{};

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
	auto context = static_cast<EventSubscriptionConnectionContext*>(
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

	for (auto&& subscription_data : context->subscriptions) {
		res = dx_close_event_subscription((dxf_subscription_t)subscription_data) && res;
	}

	if (IS_FLAG_SET(context->set_fields_flags, MUTEX_FIELD_FLAG)) {
		res = dx_mutex_destroy(&(context->subscr_guard)) && res;
	}

	context->subscriptions.clear();

	delete context;

	return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	Helper functions
 */
/* -------------------------------------------------------------------------- */

dxf_int_t dx_symbol_name_hasher(dxf_const_string_t symbol_name) { return std::hash<std::wstring>{}(symbol_name); }

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

	dx_cleanup_event_data_array(symbol_data->last_events);
	dx_cleanup_event_data_array(symbol_data->last_events_accessed);

	symbol_data->subscriptions.clear();

	delete symbol_data;

	return nullptr;
}

/* -------------------------------------------------------------------------- */

SymbolData* dx_create_symbol_data(dxf_const_string_t name) {
	int i = dx_eid_begin;
	auto res = new (std::nothrow) SymbolData{};

	if (res == nullptr) {
		return res;
	}

	res->name = std::wstring(name);
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
	auto found_symbol_data = context->symbols.find(std::wstring(symbol_name));

	if (found_symbol_data == context->symbols.end()) {
		res = dx_create_symbol_data(symbol_name);

		if (res == nullptr) {
			return nullptr;
		}

		context->symbols[symbol_name] = res;
	} else {
		res = found_symbol_data->second;
	}

	if (res->subscriptions.find(owner) != res->subscriptions.end()) {
		return res;
	}

	res->subscriptions.insert(owner);
	++(res->ref_count);

	return res;
}

/* -------------------------------------------------------------------------- */

int dx_unsubscribe_symbol(EventSubscriptionConnectionContext* context, SymbolData* symbol_data,
						  SubscriptionData* owner) {
	int res = true;

	auto found_subscription = symbol_data->subscriptions.find(owner);

	if (found_subscription == symbol_data->subscriptions.end()) {
		dx_set_error_code(dx_ec_internal_assert_violation);

		res = false;
	} else {
		symbol_data->subscriptions.erase(owner);
	}

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
	auto subscriptions = &(CTX(subscr_data->connection_context)->subscriptions);
	auto found_subscription_data = subscriptions->find(subscr_data);

	if (found_subscription_data != subscriptions->end()) {
		/* should never be here */
		return dx_set_error_code(dx_ec_internal_assert_violation);
	}

	subscriptions->insert(subscr_data);

	return true;
}

/* -------------------------------------------------------------------------- */

int dx_remove_subscription_from_context(SubscriptionData* subscr_data) {
	auto subscriptions = &(CTX(subscr_data->connection_context)->subscriptions);
	auto found_subscription_data = subscriptions->find(subscr_data);

	if (found_subscription_data == subscriptions->end()) {
		/* should never be here */

		return dx_set_error_code(dx_ec_internal_assert_violation);
	}

	subscriptions->erase(found_subscription_data);

	return true;
}

/* -------------------------------------------------------------------------- */

void dx_free_event_subscription_data(SubscriptionData* subscr_data) {
	if (subscr_data == nullptr) {
		return;
	}

	/* no element freeing is performed because this array only stores the pointers to the strings allocated somewhere
	 * else */
	subscr_data->symbol_names.clear();
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

	auto subscr_data = new (std::nothrow) SubscriptionData{};

	if (subscr_data == nullptr) {
		return dx_invalid_subscription;
	}

	subscr_data->event_types = event_types;
	subscr_data->connection_context = context;
	subscr_data->subscr_flags = subscr_flags;
	subscr_data->time = time;

	res = true;
	int i = 0;
	if (event_types & DXF_ET_ORDER) {
		for (; dx_all_order_sources[i]; i++) {
			res = res && dx_add_order_source(subscr_data, dx_all_order_sources[i]);
		}
		res = res && dx_add_order_source(subscr_data, L"");	 // Used by MM
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

	auto context = static_cast<EventSubscriptionConnectionContext*>(subscr_data->connection_context);

	/* locking a guard mutex */
	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	res = dx_clear_symbol_array(context, subscr_data->symbols, subscr_data) && res;
	subscr_data->listeners.clear();

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

	auto context = static_cast<EventSubscriptionConnectionContext*>(subscr_data->connection_context);

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

	auto context = static_cast<EventSubscriptionConnectionContext*>(subscr_data->connection_context);

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

int dx_add_listener_impl(dxf_subscription_t subscr_id, ListenerPtr listener, EventListenerVersion version,
						 void* user_data) {
	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (listener == nullptr) {
		return dx_set_error_code(dx_esec_invalid_listener);
	}

	auto subscr_data = (SubscriptionData*)subscr_id;
	auto context = static_cast<EventSubscriptionConnectionContext*>(subscr_data->connection_context);

	/* a guard mutex is required to protect the internal containers
	from the secondary data retriever threads */
	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	auto listener_context = ListenerContext(listener, version, user_data);

	if (subscr_data->listeners.find(listener_context) != subscr_data->listeners.end()) {
		/* listener is already added */
		return dx_mutex_unlock(&(context->subscr_guard));
	}

	subscr_data->listeners.emplace(listener_context);

	return dx_mutex_unlock(&(context->subscr_guard));
}

/* -------------------------------------------------------------------------- */

int dx_add_listener(dxf_subscription_t subscr_id, dxf_event_listener_t listener, void* user_data) {
	return dx_add_listener_impl(subscr_id, (ListenerPtr)listener, EventListenerVersion::Default, user_data);
}

/* -------------------------------------------------------------------------- */

int dx_add_listener_v2(dxf_subscription_t subscr_id, dxf_event_listener_v2_t listener, void* user_data) {
	return dx_add_listener_impl(subscr_id, (ListenerPtr)listener, EventListenerVersion::V2, user_data);
}

/* -------------------------------------------------------------------------- */

int dx_remove_listener_impl(dxf_subscription_t subscr_id, ListenerPtr listener) {

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (listener == nullptr) {
		return dx_set_error_code(dx_esec_invalid_listener);
	}

	auto subscr_data = (SubscriptionData*)subscr_id;
	auto context = static_cast<EventSubscriptionConnectionContext*>(subscr_data->connection_context);
	/*
	a guard mutex is required to protect the internal containers
	from the secondary data retriever threads
	*/
	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	auto listener_context_it = subscr_data->listeners.find(ListenerContext::createDummy(listener));
	if (listener_context_it == subscr_data->listeners.end()) {
		/* listener isn't subscribed */
		return dx_mutex_unlock(&(context->subscr_guard));
	}

	subscr_data->listeners.erase(listener_context_it);

	return dx_mutex_unlock(&(context->subscr_guard));
}

int dx_remove_listener(dxf_subscription_t subscr_id, dxf_event_listener_t listener) {
	return dx_remove_listener_impl(subscr_id, (ListenerPtr)listener);
}

/* -------------------------------------------------------------------------- */

int dx_remove_listener_v2(dxf_subscription_t subscr_id, dxf_event_listener_v2_t listener) {
	return dx_remove_listener_impl(subscr_id, (ListenerPtr)listener);
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

	*connection = CTX(subscr_data->connection_context)->connection;

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

	subscr_data->symbol_names.resize(subscr_data->symbols.size());

	std::size_t i = 0;
	for (auto&& s : subscr_data->symbols) {
		subscr_data->symbol_names[i] = s.second->name.c_str();
		i++;
	}

	*symbols = subscr_data->symbol_names.data();
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

	auto context = static_cast<EventSubscriptionConnectionContext*>(subscr_data->connection_context);
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
	for (auto&& listener_context : subscr_data->listeners) {
		switch (listener_context.getVersion()) {
			case EventListenerVersion::Default: {
				auto listener = (dxf_event_listener_t)listener_context.getListener();
				listener(event_bitmask, symbol_name, static_cast<dxf_event_data_t const*>(data), data_count,
						 listener_context.getUserData());
				break;
			}

			case EventListenerVersion::V2: {
				auto listener = (dxf_event_listener_v2_t)listener_context.getListener();
				listener(event_bitmask, symbol_name, static_cast<dxf_event_data_t const*>(data), data_count,
						 event_params, listener_context.getUserData());
				break;
			}

			default:
				dx_set_error_code(dx_esec_invalid_listener);

		}
	}
}

#define DX_ORDER_SOURCE_COMPARATOR_NONSYM(l, r) (dx_compare_strings(l.suffix, r))

void pass_event_data_to_listeners(SymbolData* symbol_data, dx_event_id_t event_id, dxf_const_string_t symbol_name,
								  dxf_const_event_data_t data, int data_count, const dxf_event_params_t* event_params) {
	unsigned event_bitmask = DX_EVENT_BIT_MASK(event_id);

	for (auto&& subscription_data : symbol_data->subscriptions) {
		if (!(subscription_data->event_types &
			  event_bitmask)) { /* subscription doesn't want this specific event type */
			continue;
		}

		/* Check order source, it must be done by record ids, really! */
		if (event_id == dx_eid_order && subscription_data->order_source.size) {
			/* We need to filter data in parts, unfortunately */
			auto orders = (dxf_order_t*)data;
			int start_index, end_index;
			end_index = start_index = 0;
			while (end_index != data_count) {
				/* Skip all non-needed orders */
				while (start_index < data_count) {
					int found;
					size_t index;
					DX_ARRAY_SEARCH(subscription_data->order_source.elements, 0, subscription_data->order_source.size,
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
					DX_ARRAY_SEARCH(subscription_data->order_source.elements, 0, subscription_data->order_source.size,
									orders[end_index].source, DX_ORDER_SOURCE_COMPARATOR_NONSYM, false, found, index);
					if (found)
						end_index++;
					else
						break;
				}

				dx_call_subscr_listeners(subscription_data, event_bitmask, symbol_name, &orders[start_index],
										 end_index - start_index, event_params);
			}
		} else {
			dx_call_subscr_listeners(subscription_data, event_bitmask, symbol_name, data, data_count, event_params);
		}
	}
}

int dx_process_event_data(dxf_connection_t connection, dx_event_id_t event_id, dxf_const_string_t symbol_name,
						  dxf_const_event_data_t data, int data_count, const dxf_event_params_t* event_params) {
	int res;
	auto context = static_cast<EventSubscriptionConnectionContext*>(
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
	dx_event_id_t event_id;
	int res;
	auto context = static_cast<EventSubscriptionConnectionContext*>(
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
	auto context = static_cast<EventSubscriptionConnectionContext*>(
		dx_get_subsystem_data(connection, dx_ccs_event_subscription, &res));

	if (context == nullptr) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return false;
	}

	CHECKED_CALL(dx_mutex_lock, &(context->subscr_guard));

	auto& subscriptions = CTX(context)->subscriptions;

	for (auto&& subscription_data : subscriptions) {
		dxf_const_string_t* symbols = nullptr;
		size_t symbol_count = 0;
		unsigned event_types = 0;
		auto subscr_flags = static_cast<dx_event_subscr_flag>(0);
		dxf_long_t time;

		if (!dx_get_event_subscription_symbols(subscription_data, &symbols, &symbol_count) ||
			!dx_get_event_subscription_event_types(subscription_data, &event_types) ||
			!dx_get_event_subscription_flags(subscription_data, &subscr_flags) ||
			!dx_get_event_subscription_time(subscription_data, &time) ||
			!processor(connection, dx_get_order_source(subscription_data), symbols, symbol_count, event_types,
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
