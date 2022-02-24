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

extern "C" {

#include "EventSubscription.h"

#include "ConnectionContextData.h"
#include "DXAlgorithms.h"
#include "DXErrorCodes.h"
#include "DXErrorHandling.h"
#include "DXFeed.h"
#include "DXThreads.h"
#include "Logger.h"
#include "SymbolCodec.h"
}

#include <cmath>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "Configuration.hpp"
#include "EventSubscription.hpp"

/* -------------------------------------------------------------------------- */
/*
 *	Internal data structures and objects
 */
/* -------------------------------------------------------------------------- */

/**
 * @addtogroup event-data-structures-order-spread-order
 * @{
 */

/// Order sources
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
	L"BZX",  /// Bats BZX Exchange.
	L"BATE", /// Bats Europe BXE Exchange.
	L"CHIX", /// Bats Europe CXE Exchange.
	L"CEUX", /// Bats Europe DXE Exchange.
	L"BXTR", /// Bats Europe TRF.
	L"IST",  /// Borsa Istanbul Exchange.
	L"BI20", /// Borsa Istanbul Exchange. Record for particular top 20 order book.
	L"ABE",  /// ABE (abe.io) exchange.
	L"FAIR", /// FAIR (FairX) exchange.
	L"GLBX", /// CME Globex.
	L"glbx", /// CME Globex. Record for price level book.
	L"ERIS", /// Eris Exchange group of companies.
	L"XEUR", /// Eurex Exchange.
	L"xeur", /// Eurex Exchange. Record for price level book.
	L"CFE",  /// CBOE Futures Exchange.
	L"C2OX", /// CBOE Options C2 Exchange.
	L"SMFE", /// Small Exchange.
	L"smfe", /// Small Exchange. Record for price level book.
	L"iex",  /// Investors exchange. Record for price level book.
	L"MEMX", /// Members Exchange.
	L"memx", /// Members Exchange. Record for price level book.
	nullptr};

const dxf_const_string_t dx_all_special_order_sources[] = {
	L"DEFAULT",	 /// back compatibility with Java API

	/**
	 * Bid side of a composite Quote. It is a synthetic source. The subscription on composite Quote event is observed
	 * when this source is subscribed to.
	 */
	L"COMPOSITE_BID",

	/**
	 * Ask side of a composite Quote. It is a synthetic source. The subscription on composite Quote event is observed
	 * when this source is subscribed to.
	 */
	L"COMPOSITE_ASK",

	/**
	 * Bid side of a regional Quote. It is a synthetic source. The subscription on regional Quote event is observed
	 * when this source is subscribed to.
	 */
	L"REGIONAL_BID",

	/**
	 * Ask side of a composite Quote. It is a synthetic source. The subscription on regional Quote event is observed
	 * when this source is subscribed to.
	 */
	L"REGIONAL_ASK",

	/**
	 * Bid side of an aggregate order book (futures depth and NASDAQ Level II). It is a synthetic source. This source
	 * cannot be directly published via dxFeed API, but otherwise it is fully operational.
	 */
	L"AGGREGATE_BID",

	/**
	 * Ask side of an aggregate order book (futures depth and NASDAQ Level II). It is a synthetic source. This source
	 * cannot be directly published via dxFeed API, but otherwise it is fully operational.
	 */
	L"AGGREGATE_ASK",

	L"EMPTY",  // back compatibility with .NET API

	/**
	 * Bid and ask sides of a composite Quote. It is a synthetic source. The subscription on composite Quote event is
	 * observed when this source is subscribed to.
	 */
	L"COMPOSITE",

	/**
	 * Bid and ask sides of a regional Quote. It is a synthetic source. The subscription on regional Quote event is
	 * observed when this source is subscribed to.
	 */
	L"REGIONAL",

	/**
	 * Bid side of an aggregate order book (futures depth and NASDAQ Level II). It is a synthetic source. This source
	 * cannot be directly published via dxFeed API, but otherwise it is fully operational.
	 */
	L"AGGREGATE", nullptr};
///@}

const size_t dx_all_order_sources_count = sizeof(dx_all_order_sources) / sizeof(dx_all_order_sources[0]) - 1;

const size_t dx_all_special_order_sources_count =
	sizeof(dx_all_special_order_sources) / sizeof(dx_all_special_order_sources[0]) - 1;

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

	// TODO: remove in 9.0.0
	if (!Configuration::getInstance()->getSubscriptionsDisableLastEventStorage()) {
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
	} else {
		res->lastEvents = nullptr;
		res->lastEventsAccessed = nullptr;
	}

	return res;
}

void SymbolData::storeLastSymbolEvent(dx_event_id_t eventId, dxf_const_event_data_t data) {
	if (this->lastEvents != nullptr) {
		dx_memcpy(this->lastEvents[eventId],
				  dx_get_event_data_item(DX_EVENT_BIT_MASK(static_cast<unsigned>(eventId)), data, 0),
				  dx_get_event_data_struct_size(eventId));
	}
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
	subscriptionData->clearRawOrderSources();

	delete subscriptionData;
}

int SubscriptionData::closeEventSubscription(dxf_subscription_t subscriptionId, bool remove_from_context) {
	if (subscriptionId == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	auto subscriptionData = static_cast<dx::SubscriptionData*>(subscriptionId);
	int res = true;
	auto context = static_cast<dx::EventSubscriptionConnectionContext*>(subscriptionData->connection_context);

	context->process([&res, &subscriptionData, remove_from_context](dx::EventSubscriptionConnectionContext* ctx) {
		for (auto&& s : subscriptionData->symbols) {
			res = ctx->unsubscribeSymbol(s.second, subscriptionData) && res;
		}

		subscriptionData->symbols.clear();
		subscriptionData->listeners.clear();

		if (remove_from_context) {
			ctx->removeSubscription(subscriptionData);
		}
	});

	SubscriptionData::free(subscriptionData);

	return res;
}

void SubscriptionData::fillRawOrderSources() {
	for (auto&& source : orderSources) {
		if (EventSubscriptionConnectionContext::specialOrderSources.count(source) > 0) {
			continue;
		}

		dx_suffix_t new_source = {};

		dx_copy_string_len(new_source.suffix, source.c_str(), DXF_RECORD_SUFFIX_SIZE);

		int failed = false;
		DX_ARRAY_INSERT(rawOrderSources, dx_suffix_t, new_source, rawOrderSources.size, dx_capacity_manager_halfer,
						failed);

		if (failed) {
			return;
		}
	}
}

void SubscriptionData::clearRawOrderSources() {
	dx_free(rawOrderSources.elements);
	rawOrderSources.elements = nullptr;
	rawOrderSources.size = 0;
	rawOrderSources.capacity = 0;
}

const std::unordered_set<std::wstring> EventSubscriptionConnectionContext::specialOrderSources{
	std::begin(dx_all_special_order_sources), std::begin(dx_all_special_order_sources) + dx_all_special_order_sources_count};

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
bool EventSubscriptionConnectionContext::hasAnySymbol() {
	return process([this](dx::EventSubscriptionConnectionContext* ctx) { return !symbols.empty(); });
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
	if (event_types & DXF_ET_ORDER) {
		for (int i = 0; dx_all_order_sources[i]; i++) {
			res = res && dx_add_order_source(subscr_data, dx_all_order_sources[i]);
		}

		res = res && dx_add_order_source(subscr_data, dx_all_special_order_sources[dxf_sos_AGGREGATE]);
		res = res && dx_add_order_source(subscr_data, dx_all_special_order_sources[dxf_sos_REGIONAL]);
		res = res && dx_add_order_source(subscr_data, dx_all_special_order_sources[dxf_sos_COMPOSITE]);
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

int dx_add_symbols_impl(dxf_subscription_t subscr_id, dxf_const_string_t* symbols, int symbol_count,
						int* added_symbols_indices, int* added_symbols_count) {
	auto subscr_data = static_cast<dx::SubscriptionData*>(subscr_id);

	if (subscr_id == dx_invalid_subscription) {
		return dx_set_error_code(dx_esec_invalid_subscr_id);
	}

	auto context = static_cast<dx::EventSubscriptionConnectionContext*>(subscr_data->connection_context);

	return context->process([symbols, symbol_count, &subscr_data, added_symbols_indices,
							 added_symbols_count](dx::EventSubscriptionConnectionContext* ctx) {
		if (added_symbols_indices != nullptr && added_symbols_count != nullptr) {
			*added_symbols_count = 0;
		}

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

			if (added_symbols_indices != nullptr && added_symbols_count != nullptr) {
				added_symbols_indices[static_cast<std::size_t>(*added_symbols_count)] = cur_symbol_index;
				(*added_symbols_count)++;
			}

			subscr_data->symbols[symbols[cur_symbol_index]] = symbol_data;
		}

		return true;
	});
}

int dx_add_symbols_v2(dxf_subscription_t subscr_id, dxf_const_string_t* symbols, int symbol_count,
					  int* added_symbols_indices, int* added_symbols_count) {
	return dx_add_symbols_impl(subscr_id, symbols, symbol_count, added_symbols_indices, added_symbols_count);
}

int dx_add_symbols(dxf_subscription_t subscr_id, dxf_const_string_t* symbols, int symbol_count) {
	return dx_add_symbols_impl(subscr_id, symbols, symbol_count, nullptr, nullptr);
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
									 dxf_const_string_t symbol_name, dxf_const_event_data_t data,
									 const dxf_event_params_t* event_params) {
	if (subscr_data == nullptr) {
		return;
	}

	for (auto&& listener_context : subscr_data->listeners) {
		switch (listener_context.getVersion()) {
			case dx::EventListenerVersion::Default: {
				auto listener = (dxf_event_listener_t)listener_context.getListener();
				listener(event_bitmask, symbol_name, static_cast<dxf_event_data_t const*>(data), 1,
						 listener_context.getUserData());
				break;
			}

			case dx::EventListenerVersion::V2: {
				auto listener = (dxf_event_listener_v2_t)listener_context.getListener();
				listener(event_bitmask, symbol_name, static_cast<dxf_event_data_t const*>(data), 1, event_params,
						 listener_context.getUserData());
				break;
			}

			default:
				dx_set_error_code(dx_esec_invalid_listener);
		}
	}
}

void pass_event_data_to_listeners(dx::EventSubscriptionConnectionContext* ctx, dx::SymbolData* symbol_data,
								  dx_event_id_t event_id, dxf_const_string_t symbol_name, dxf_const_event_data_t data,
								  const dxf_event_params_t* event_params) {
	symbol_data->refCount++;  // TODO replace by std::shared_ptr\std::weak_ptr
	unsigned event_bitmask = DX_EVENT_BIT_MASK(static_cast<unsigned>(event_id));
	const auto& subscriptions = symbol_data->subscriptions;	 // number of subscriptions may be changed in the listeners

	for (auto* subscription_data : subscriptions) {
		if (!(subscription_data->event_types &
			  event_bitmask)) { /* subscription doesn't want this specific event type */
			continue;
		}

		auto call = false;
		// Check order source
		if (event_id == dx_eid_order && !subscription_data->orderSources.empty()) {
			auto order = *static_cast<const dxf_order_t*>(data);
			auto sourceStr = std::wstring(order.source);

			call = subscription_data->orderSources.find(sourceStr) != subscription_data->orderSources.end();

			if (!call) {
				if (sourceStr == dx_all_special_order_sources[dxf_sos_AGGREGATE_ASK] || sourceStr == dx_all_special_order_sources[dxf_sos_AGGREGATE_BID]) {
					call = subscription_data->orderSources.find(dx_all_special_order_sources[dxf_sos_AGGREGATE]) !=
							subscription_data->orderSources.end();
				} else if (sourceStr == dx_all_special_order_sources[dxf_sos_COMPOSITE_ASK] || sourceStr == dx_all_special_order_sources[dxf_sos_COMPOSITE_BID]) {
					call = subscription_data->orderSources.find(dx_all_special_order_sources[dxf_sos_COMPOSITE]) !=
							subscription_data->orderSources.end();
				} else if (sourceStr == dx_all_special_order_sources[dxf_sos_REGIONAL_ASK] || sourceStr == dx_all_special_order_sources[dxf_sos_REGIONAL_BID]) {
					call = subscription_data->orderSources.find(dx_all_special_order_sources[dxf_sos_REGIONAL]) !=
							subscription_data->orderSources.end();
				}
			}
		} else {
			call = true;
		}

		if (call) {
			dx_call_subscr_listeners(subscription_data, event_bitmask, symbol_name, data, event_params);
		}
	}

	// TODO replace by std::shared_ptr\std::weak_ptr
	symbol_data->refCount--;
	if (symbol_data->refCount == 0) {
		ctx->removeSymbolData(symbol_data);
	}
}

int dx_process_event_data(dxf_connection_t connection, dx_event_id_t event_id, dxf_const_string_t symbol_name,
						  dxf_const_event_data_t data, const dxf_event_params_t* event_params) {
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

	context->process([event_id, symbol_name, data, event_params](dx::EventSubscriptionConnectionContext* ctx) {
		dx::SymbolData* symbol_data = ctx->findSymbol(symbol_name);

		/* symbol_data == nullptr is most likely a correct situation that occurred because the data is received very
		soon after the symbol subscription has been annulled */
		if (symbol_data != nullptr) {
			symbol_data->storeLastSymbolEvent(event_id, data);
			pass_event_data_to_listeners(ctx, symbol_data, event_id, symbol_name, data, event_params);
		}

		dx::SymbolData* wildcard_symbol_data = ctx->findSymbol(L"*");

		if (wildcard_symbol_data != nullptr) {
			wildcard_symbol_data->storeLastSymbolEvent(event_id, data);
			pass_event_data_to_listeners(ctx, wildcard_symbol_data, event_id, symbol_name, data, event_params);
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

		if (symbol_data->lastEventsAccessed != nullptr) {
			dx_memcpy(symbol_data->lastEventsAccessed[event_id], symbol_data->lastEvents[event_id],
					  dx_get_event_data_struct_size(event_id));
			*event_data = symbol_data->lastEventsAccessed[event_id];
		}

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
				!processor(connection, dx_get_order_sources(subscription_data), symbols, symbol_count, event_types,
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

int dx_add_order_source(dxf_subscription_t subscr_id, dxf_const_string_t source) {
	auto subscriptionData = static_cast<dx::SubscriptionData*>(subscr_id);

	subscriptionData->orderSources.emplace(source);

	return true;
}

/* -------------------------------------------------------------------------- */

void dx_clear_order_sources(dxf_subscription_t subscr_id) {
	if (subscr_id == nullptr) {
		return;
	}

	auto subscriptionData = static_cast<dx::SubscriptionData*>(subscr_id);

	subscriptionData->orderSources.clear();
	subscriptionData->clearRawOrderSources();
}

dx_order_source_array_ptr_t dx_get_order_sources(dxf_subscription_t subscr_id) {
	auto subscriptionData = static_cast<dx::SubscriptionData*>(subscr_id);

	subscriptionData->clearRawOrderSources();
	subscriptionData->fillRawOrderSources();

	return &subscriptionData->rawOrderSources;
}

int dx_has_any_subscribed_symbol(dxf_connection_t connection) {
	int res;
	auto context = static_cast<dx::EventSubscriptionConnectionContext*>(
		dx_get_subsystem_data(connection, dx_ccs_event_subscription, &res));

	if (context == nullptr) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return false;
	}

	return context->hasAnySymbol();
}
