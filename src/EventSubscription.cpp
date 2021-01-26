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
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "EventSubscription.hpp"
#include "Configuration.hpp"

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

namespace dx {

SymbolData* SymbolData::cleanup(SymbolData* symbolData) {
	if (symbolData == nullptr) {
		return nullptr;
	}

	auto cleanupEventDataArray = [](dxf_event_data_t* dataArray) {
		int i = dx_eid_begin;

		if (dataArray == nullptr) {
			return;
		}

		for (; i < dx_eid_count; ++i) {
			CHECKED_FREE(dataArray[i]);
		}

		dx_free(dataArray);
	};

	cleanupEventDataArray(symbolData->lastEvents);
	cleanupEventDataArray(symbolData->lastEventsAccessed);

	symbolData->subscriptions.clear();

	delete symbolData;

	return nullptr;
}

SymbolData* SymbolData::create(dxf_const_string_t name) {
	auto res = new (std::nothrow) dx::SymbolData{};

	if (res == nullptr) {
		return res;
	}

	res->name = std::wstring(name);
	res->lastEvents = static_cast<dxf_event_data_t*>(dx_calloc(dx_eid_count, sizeof(dxf_event_data_t)));
	res->lastEventsAccessed = static_cast<dxf_event_data_t*>(dx_calloc(dx_eid_count, sizeof(dxf_event_data_t)));

	if (res->lastEvents == nullptr || res->lastEventsAccessed == nullptr) {
		return cleanup(res);
	}

	for (int i = dx_eid_begin; i < dx_eid_count; ++i) {
		res->lastEvents[i] = dx_calloc(1, dx_get_event_data_struct_size(i));
		res->lastEventsAccessed[i] = dx_calloc(1, dx_get_event_data_struct_size(i));

		if (res->lastEvents[i] == nullptr || res->lastEventsAccessed[i] == nullptr) {
			return cleanup(res);
		}
	}

	return res;
}

void SymbolData::storeLastSymbolEvent(dx_event_id_t eventId, dxf_const_event_data_t data, int dataCount) {
	dx_memcpy(this->lastEvents[eventId],
			  dx_get_event_data_item(DX_EVENT_BIT_MASK(static_cast<unsigned>(eventId)), data, dataCount - 1),
			  dx_get_event_data_struct_size(eventId));
}

ListenerContext::ListenerContext(ListenerPtr listener, EventListenerVersion version, void* userData) noexcept
	: version{version}, userData{userData} {
	if (version == EventListenerVersion::Default) {
		this->listener = (dxf_event_listener_t)(listener);
	} else {
		this->listenerV2 = (dxf_event_listener_v2_t)listener;
	}
}

ListenerContext::ListenerPtr ListenerContext::getListener() const noexcept {
	return (version == EventListenerVersion::Default) ? (ListenerPtr)listener : (ListenerPtr)listenerV2;
}

EventListenerVersion ListenerContext::getVersion() const noexcept { return version; }

ListenerContext ListenerContext::createDummy(ListenerPtr listener) {
	return {listener, EventListenerVersion::Default, nullptr};
}

void* ListenerContext::getUserData() const noexcept { return userData; }

void SubscriptionData::free(SubscriptionData* subscriptionData) {
	if (subscriptionData == nullptr) {
		return;
	}

	/* no element freeing is performed because this array only stores the pointers to the strings allocated somewhere
	 * else */
	subscriptionData->symbolNames.clear();
	subscriptionData->clearOrderSource();

	delete subscriptionData;
}

int SubscriptionData::closeEventSubscription(dxf_subscription_t subscriptionId, bool remove_from_context) {
	if (subscriptionId == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	auto subscr_data = static_cast<dx::SubscriptionData*>(subscriptionId);
	int res = true;
	auto context = static_cast<dx::EventSubscriptionConnectionContext*>(subscr_data->connection_context);

	context->process([&res, &subscr_data, remove_from_context](dx::EventSubscriptionConnectionContext* ctx) {
		for (auto&& s : subscr_data->symbols) {
			res = ctx->unsubscribeSymbol(s.second, subscr_data) && res;
		}

		subscr_data->symbols.clear();
		subscr_data->listeners.clear();

		if (remove_from_context) {
			ctx->removeSubscription(subscr_data);
		}
	});

	SubscriptionData::free(subscr_data);

	return res;
}
void SubscriptionData::clearOrderSource() {
	dx_free(orderSource.elements);
	orderSource.elements = nullptr;
	orderSource.size = 0;
	orderSource.capacity = 0;
}

EventSubscriptionConnectionContext::EventSubscriptionConnectionContext(dxf_connection_t connectionHandle)
	: connectionHandle{connectionHandle}, mutex{}, symbols{}, subscriptions{} {}

dxf_connection_t EventSubscriptionConnectionContext::getConnectionHandle() { return connectionHandle; }

std::unordered_set<SubscriptionData*> EventSubscriptionConnectionContext::getSubscriptions() { return subscriptions; }

SymbolData* EventSubscriptionConnectionContext::subscribeSymbol(dxf_const_string_t symbolName,
																SubscriptionData* owner) {
	SymbolData* res;
	auto found = symbols.find(std::wstring(symbolName));

	if (found == symbols.end()) {
		res = SymbolData::create(symbolName);

		if (res == nullptr) {
			return nullptr;
		}

		symbols[symbolName] = res;
	} else {
		res = found->second;
	}

	if (res->subscriptions.find(owner) != res->subscriptions.end()) {
		return res;
	}

	res->subscriptions.insert(owner);
	res->refCount++;

	return res;
}

void EventSubscriptionConnectionContext::removeSymbolData(SymbolData* symbolData) {
	auto found = symbols.find(std::wstring(symbolData->name));

	if (found != symbols.end()) {
		symbols.erase(found);
	}

	SymbolData::cleanup(symbolData);
}

int EventSubscriptionConnectionContext::unsubscribeSymbol(SymbolData* symbolData, SubscriptionData* owner) {
	int res = true;

	auto found = symbolData->subscriptions.find(owner);

	if (found == symbolData->subscriptions.end()) {
		dx_set_error_code(dx_ec_internal_assert_violation);

		res = false;
	} else {
		symbolData->subscriptions.erase(owner);
	}

	if (--(symbolData->refCount) == 0) {
		removeSymbolData(symbolData);
	}

	return res;
}

SymbolData* EventSubscriptionConnectionContext::findSymbol(dxf_const_string_t symbolName) {
	auto found = symbols.find(std::wstring(symbolName));

	if (found == symbols.end()) {
		return nullptr;
	}

	return found->second;
}

void EventSubscriptionConnectionContext::addSubscription(SubscriptionData* data) {
	std::lock_guard<std::recursive_mutex> lk(mutex);

	subscriptions.insert(data);
}

void EventSubscriptionConnectionContext::removeSubscription(SubscriptionData* data) {
	std::lock_guard<std::recursive_mutex> lk(mutex);

	subscriptions.erase(data);
}

EventSubscriptionConnectionContext::~EventSubscriptionConnectionContext() {
	std::lock_guard<std::recursive_mutex> lk(mutex);

	for (auto&& subscriptionData : subscriptions) {
		SubscriptionData::closeEventSubscription(static_cast<dxf_subscription_t>(subscriptionData), false);
	}
}

}  // namespace dx

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_event_subscription) {
	CHECKED_CALL_2(dx_validate_connection_handle, connection, true);

	auto context = new (std::nothrow) dx::EventSubscriptionConnectionContext(connection);

	if (!dx_set_subsystem_data(connection, dx_ccs_event_subscription, context)) {
		delete context;

		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_event_subscription) {
	int res = true;
	auto context = static_cast<dx::EventSubscriptionConnectionContext*>(
		dx_get_subsystem_data(connection, dx_ccs_event_subscription, &res));

	if (context == nullptr) {
		return res;
	}

	delete context;

	return res;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_event_subscription) { return true; }

/* -------------------------------------------------------------------------- */
/*
 *	Helper functions
 */
/* -------------------------------------------------------------------------- */

dxf_ulong_t dx_symbol_name_hasher(dxf_const_string_t symbol_name) {
	return static_cast<dxf_ulong_t>(std::hash<std::wstring>{}(symbol_name));
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

	auto context = static_cast<dx::EventSubscriptionConnectionContext*>(
		dx_get_subsystem_data(connection, dx_ccs_event_subscription, &res));

	if (context == nullptr) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return dx_invalid_subscription;
	}

	if (event_types & static_cast<unsigned>(DXF_ET_UNUSED)) {
		dx_set_error_code(dx_esec_invalid_event_type);

		return dx_invalid_subscription;
	}

	auto subscr_data = new (std::nothrow) dx::SubscriptionData{};

	if (subscr_data == nullptr) {
		return dx_invalid_subscription;
	}

	subscr_data->event_types = event_types;
	subscr_data->connection_context = context;
	subscr_data->subscriptionFlags = subscr_flags;
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
		dx::SubscriptionData::free(subscr_data);
		return dx_invalid_subscription;
	}

	context->addSubscription(subscr_data);

	return static_cast<dxf_subscription_t>(subscr_data);
}

int dx_close_event_subscription(dxf_subscription_t subscr_id) {
	return dx::SubscriptionData::closeEventSubscription(subscr_id, true);
}

/* -------------------------------------------------------------------------- */

int dx_add_symbols(dxf_subscription_t subscr_id, dxf_const_string_t* symbols, int symbol_count) {
	auto subscr_data = static_cast<dx::SubscriptionData*>(subscr_id);

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	auto context = static_cast<dx::EventSubscriptionConnectionContext*>(subscr_data->connection_context);

	return context->process([symbols, symbol_count, &subscr_data](dx::EventSubscriptionConnectionContext* ctx) {
		for (int cur_symbol_index = 0; cur_symbol_index < symbol_count; ++cur_symbol_index) {
			if (dx_string_null_or_empty(symbols[cur_symbol_index])) {
				continue;
			}

			auto found = subscr_data->symbols.find(std::wstring(symbols[cur_symbol_index]));

			if (found != subscr_data->symbols.end()) {
				/* symbol is already subscribed */
				continue;
			}

			auto symbol_data = ctx->subscribeSymbol(symbols[cur_symbol_index], subscr_data);

			if (symbol_data == nullptr) {
				return false;
			}

			subscr_data->symbols[symbols[cur_symbol_index]] = symbol_data;
		}

		return true;
	});
}

/* -------------------------------------------------------------------------- */

int dx_remove_symbols(dxf_subscription_t subscr_id, dxf_const_string_t* symbols, size_t symbol_count) {
	auto subscr_data = static_cast<dx::SubscriptionData*>(subscr_id);

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	auto context = static_cast<dx::EventSubscriptionConnectionContext*>(subscr_data->connection_context);

	return context->process([&subscr_data, symbols, symbol_count](dx::EventSubscriptionConnectionContext* ctx) {
		for (size_t cur_symbol_index = 0; cur_symbol_index < symbol_count; ++cur_symbol_index) {
			if (dx_string_null_or_empty(symbols[cur_symbol_index])) {
				continue;
			}

			auto found = subscr_data->symbols.find(std::wstring(symbols[cur_symbol_index]));

			if (found == subscr_data->symbols.end()) {
				/* symbol wasn't subscribed */
				continue;
			}

			if (!ctx->unsubscribeSymbol(found->second, subscr_data)) {
				return false;
			}

			subscr_data->symbols.erase(found);
		}

		return true;
	});
}

/* -------------------------------------------------------------------------- */

int dx_add_listener_impl(dxf_subscription_t subscr_id, dx::ListenerContext::ListenerPtr listener,
						 dx::EventListenerVersion version, void* user_data) {
	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (listener == nullptr) {
		return dx_set_error_code(dx_esec_invalid_listener);
	}

	auto subscr_data = static_cast<dx::SubscriptionData*>(subscr_id);
	auto context = static_cast<dx::EventSubscriptionConnectionContext*>(subscr_data->connection_context);
	auto listener_context = dx::ListenerContext(listener, version, user_data);

	context->process([&subscr_data, listener_context](dx::EventSubscriptionConnectionContext* ctx) {
		if (subscr_data->listeners.find(listener_context) == subscr_data->listeners.end()) {
			subscr_data->listeners.emplace(listener_context);
		}
	});

	return true;
}

/* -------------------------------------------------------------------------- */

int dx_add_listener(dxf_subscription_t subscr_id, dxf_event_listener_t listener, void* user_data) {
	return dx_add_listener_impl(subscr_id, (dx::ListenerContext::ListenerPtr)listener,
								dx::EventListenerVersion::Default, user_data);
}

/* -------------------------------------------------------------------------- */

int dx_add_listener_v2(dxf_subscription_t subscr_id, dxf_event_listener_v2_t listener, void* user_data) {
	return dx_add_listener_impl(subscr_id, (dx::ListenerContext::ListenerPtr)listener, dx::EventListenerVersion::V2,
								user_data);
}

/* -------------------------------------------------------------------------- */

int dx_remove_listener_impl(dxf_subscription_t subscr_id, dx::ListenerContext::ListenerPtr listener) {
	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (listener == nullptr) {
		return dx_set_error_code(dx_esec_invalid_listener);
	}

	auto subscr_data = static_cast<dx::SubscriptionData*>(subscr_id);
	auto context = static_cast<dx::EventSubscriptionConnectionContext*>(subscr_data->connection_context);

	context->process([&subscr_data, listener](dx::EventSubscriptionConnectionContext* ctx) {
		auto listener_context_it = subscr_data->listeners.find(dx::ListenerContext::createDummy(listener));
		if (listener_context_it != subscr_data->listeners.end()) {
			subscr_data->listeners.erase(listener_context_it);
		}
	});

	return true;
}

int dx_remove_listener(dxf_subscription_t subscr_id, dxf_event_listener_t listener) {
	return dx_remove_listener_impl(subscr_id, (dx::ListenerContext::ListenerPtr)listener);
}

/* -------------------------------------------------------------------------- */

int dx_remove_listener_v2(dxf_subscription_t subscr_id, dxf_event_listener_v2_t listener) {
	return dx_remove_listener_impl(subscr_id, (dx::ListenerContext::ListenerPtr)listener);
}

/* -------------------------------------------------------------------------- */

int dx_get_subscription_connection(dxf_subscription_t subscr_id, OUT dxf_connection_t* connection) {
	auto subscr_data = static_cast<dx::SubscriptionData*>(subscr_id);

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (connection == nullptr) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	*connection =
		static_cast<dx::EventSubscriptionConnectionContext*>(subscr_data->connection_context)->getConnectionHandle();

	return true;
}

/* -------------------------------------------------------------------------- */

int dx_get_event_subscription_event_types(dxf_subscription_t subscr_id, OUT unsigned* event_types) {
	auto subscr_data = static_cast<dx::SubscriptionData*>(subscr_id);

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (event_types == nullptr) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	*event_types = subscr_data->event_types;

	return true;
}

int dx_get_event_subscription_symbols_count(dxf_subscription_t subscr_id, OUT size_t* symbol_count) {
	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (symbol_count == nullptr) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	*symbol_count = static_cast<dx::SubscriptionData*>(subscr_id)->symbols.size();

	return true;
}

int dx_get_event_subscription_symbols(dxf_subscription_t subscr_id, OUT dxf_const_string_t** symbols,
									  OUT size_t* symbol_count) {
	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	auto subscr_data = static_cast<dx::SubscriptionData*>(subscr_id);

	if (symbols == nullptr || symbol_count == nullptr) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	// Lazyly copy symbols to the intermediate buffer
	subscr_data->symbolNames.resize(subscr_data->symbols.size());

	std::size_t i = 0;
	for (auto&& s : subscr_data->symbols) {
		subscr_data->symbolNames[i] = s.second->name.c_str();
		i++;
	}

	*symbols = subscr_data->symbolNames.data();
	*symbol_count = subscr_data->symbols.size();

	return true;
}

/* -------------------------------------------------------------------------- */

int dx_get_event_subscription_flags(dxf_subscription_t subscr_id, OUT dx_event_subscr_flag* subscr_flags) {
	auto subscr_data = static_cast<dx::SubscriptionData*>(subscr_id);

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	if (subscr_flags == nullptr) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	*subscr_flags = subscr_data->subscriptionFlags;

	return true;
}

/* -------------------------------------------------------------------------- */

int dx_set_event_subscription_flags(dxf_subscription_t subscr_id, dx_event_subscr_flag subscr_flags) {
	auto subscr_data = static_cast<dx::SubscriptionData*>(subscr_id);

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	auto context = static_cast<dx::EventSubscriptionConnectionContext*>(subscr_data->connection_context);

	context->process([&subscr_data, subscr_flags](dx::EventSubscriptionConnectionContext* ctx) {
		subscr_data->subscriptionFlags = subscr_flags;
	});

	return true;
}

/* -------------------------------------------------------------------------- */

int dx_get_event_subscription_time(dxf_subscription_t subscr_id, OUT dxf_long_t* time) {
	auto subscr_data = static_cast<dx::SubscriptionData*>(subscr_id);

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

static void dx_call_subscr_listeners(dx::SubscriptionData* subscr_data, unsigned event_bitmask,
									 dxf_const_string_t symbol_name, dxf_const_event_data_t data, int data_count,
									 const dxf_event_params_t* event_params) {
	if (subscr_data == nullptr || subscr_data->symbols.find(symbol_name) == subscr_data->symbols.end()) {
		return;
	}

	for (auto&& listener_context : subscr_data->listeners) {
		switch (listener_context.getVersion()) {
			case dx::EventListenerVersion::Default: {
				auto listener = (dxf_event_listener_t)listener_context.getListener();
				listener(event_bitmask, symbol_name, static_cast<dxf_event_data_t const*>(data), data_count,
						 listener_context.getUserData());
				break;
			}

			case dx::EventListenerVersion::V2: {
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

void pass_event_data_to_listeners(dx::EventSubscriptionConnectionContext* ctx, dx::SymbolData* symbol_data,
								  dx_event_id_t event_id, dxf_const_string_t symbol_name, dxf_const_event_data_t data,
								  int data_count, const dxf_event_params_t* event_params) {
	symbol_data->refCount++;  // TODO replace by std::shared_ptr\std::weak_ptr
	unsigned event_bitmask = DX_EVENT_BIT_MASK(static_cast<unsigned>(event_id));
	auto subscriptions = symbol_data->subscriptions;  // number of subscriptions may be changed in the listeners

	for (auto&& subscription_data : subscriptions) {
		if (!(subscription_data->event_types &
			  event_bitmask)) { /* subscription doesn't want this specific event type */
			continue;
		}

		/* Check order source, it must be done by record ids, really! */
		if (event_id == dx_eid_order && subscription_data->orderSource.size) {
			/* We need to filter data in parts, unfortunately */
			auto orders = (dxf_order_t*)data;
			int start_index, end_index;
			end_index = start_index = 0;
			while (end_index != data_count) {
				/* Skip all non-needed orders */
				while (start_index < data_count) {
					int found;
					size_t index;
					DX_ARRAY_SEARCH(subscription_data->orderSource.elements, 0, subscription_data->orderSource.size,
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
					DX_ARRAY_SEARCH(subscription_data->orderSource.elements, 0, subscription_data->orderSource.size,
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

	// TODO replace by std::shared_ptr\std::weak_ptr
	symbol_data->refCount--;
	if (symbol_data->refCount == 0) {
		ctx->removeSymbolData(symbol_data);
	}
}

int dx_process_event_data(dxf_connection_t connection, dx_event_id_t event_id, dxf_const_string_t symbol_name,
						  dxf_const_event_data_t data, int data_count, const dxf_event_params_t* event_params) {
	int res;
	auto context = static_cast<dx::EventSubscriptionConnectionContext*>(
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

	context->process(
		[event_id, symbol_name, data, data_count, event_params](dx::EventSubscriptionConnectionContext* ctx) {
			dx::SymbolData* symbol_data = ctx->findSymbol(symbol_name);

			/* symbol_data == nullptr is most likely a correct situation that occurred because the data is received very
			soon after the symbol subscription has been annulled */
			if (symbol_data != nullptr) {
				symbol_data->storeLastSymbolEvent(event_id, data, data_count);
				pass_event_data_to_listeners(ctx, symbol_data, event_id, symbol_name, data, data_count, event_params);
			}

			dx::SymbolData* wildcard_symbol_data = ctx->findSymbol(L"*");

			if (wildcard_symbol_data != nullptr) {
				wildcard_symbol_data->storeLastSymbolEvent(event_id, data, data_count);
				pass_event_data_to_listeners(ctx, wildcard_symbol_data, event_id, symbol_name, data, data_count,
											 event_params);
			}
		});

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	event type is a one-bit mask here
 */

int dx_get_last_symbol_event(dxf_connection_t connection, dxf_const_string_t symbol_name, int event_type,
							 OUT dxf_event_data_t* event_data) {
	dx_event_id_t event_id;
	int res;
	auto context = static_cast<dx::EventSubscriptionConnectionContext*>(
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

	return context->process([symbol_name, event_id, &event_data](dx::EventSubscriptionConnectionContext* ctx) -> int {
		dx::SymbolData* symbol_data;

		if ((symbol_data = ctx->findSymbol(symbol_name)) == nullptr) {
			return dx_set_error_code(dx_esec_invalid_symbol_name);
		}

		dx_memcpy(symbol_data->lastEventsAccessed[event_id], symbol_data->lastEvents[event_id],
				  dx_get_event_data_struct_size(event_id));
		*event_data = symbol_data->lastEventsAccessed[event_id];

		return true;
	});
}

/* -------------------------------------------------------------------------- */

int dx_process_connection_subscriptions(dxf_connection_t connection, dx_subscription_processor_t processor) {
	int res;
	auto context = static_cast<dx::EventSubscriptionConnectionContext*>(
		dx_get_subsystem_data(connection, dx_ccs_event_subscription, &res));

	if (context == nullptr) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return false;
	}

	return context->process([connection, processor](dx::EventSubscriptionConnectionContext* ctx) {
		const auto& subscriptions = ctx->getSubscriptions();

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
				return false;
			}
		}

		return true;
	});
}

/* -------------------------------------------------------------------------- */

/* Functions for working with order source */

/* -------------------------------------------------------------------------- */

#define DX_ORDER_SOURCE_COMPARATOR(l, r) (dx_compare_strings(l.suffix, r.suffix))

int dx_add_order_source(dxf_subscription_t subscr_id, dxf_const_string_t source) {
	auto subscr_data = (dx::SubscriptionData*)subscr_id;

	int failed = false;
	dx_suffix_t new_source;
	int found;
	size_t index = 0;
	dx_copy_string_len(new_source.suffix, source, DXF_RECORD_SUFFIX_SIZE);
	if (subscr_data->orderSource.elements == nullptr) {
		DX_ARRAY_INSERT(subscr_data->orderSource, dx_suffix_t, new_source, index, dx_capacity_manager_halfer, failed);
	} else {
		DX_ARRAY_SEARCH(subscr_data->orderSource.elements, 0, subscr_data->orderSource.size, new_source,
						DX_ORDER_SOURCE_COMPARATOR, false, found, index);
		if (!found)
			DX_ARRAY_INSERT(subscr_data->orderSource, dx_suffix_t, new_source, index, dx_capacity_manager_halfer,
							failed);
	}
	if (failed) {
		return dx_set_error_code(dx_sec_not_enough_memory);
	}
	return true;
}

/* -------------------------------------------------------------------------- */

void dx_clear_order_source(dxf_subscription_t subscr_id) {
	if (subscr_id == nullptr) {
		return;
	}

	static_cast<dx::SubscriptionData*>(subscr_id)->clearOrderSource();
}

dx_order_source_array_ptr_t dx_get_order_source(dxf_subscription_t subscr_id) {
	auto subscr_data = (dx::SubscriptionData*)subscr_id;
	return &subscr_data->orderSource;
}
