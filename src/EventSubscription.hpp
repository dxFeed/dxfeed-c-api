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

#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dx {

struct SubscriptionData;

struct SymbolData {
	std::wstring name{};
	int refCount;

	std::unordered_set<SubscriptionData*> subscriptions{};
	dxf_event_data_t* lastEvents;
	dxf_event_data_t* lastEventsAccessed;

	static SymbolData* cleanup(SymbolData* dataArray);

	static SymbolData* create(dxf_const_string_t name);

	void storeLastSymbolEvent(dx_event_id_t eventId, dxf_const_event_data_t data);
};

enum class EventListenerVersion { Default = 1, V2 = 2 };

class ListenerContext {
	union {
		dxf_event_listener_t listener;
		dxf_event_listener_v2_t listenerV2;
	};
	EventListenerVersion version;
	void* userData;

public:
	ListenerContext() = default;

	using ListenerPtr = void*;

	ListenerContext(ListenerPtr listener, EventListenerVersion version, void* userData) noexcept;

	ListenerPtr getListener() const noexcept;

	EventListenerVersion getVersion() const noexcept;

	static ListenerContext createDummy(ListenerPtr listener);

	void* getUserData() const noexcept;

	friend bool operator==(const ListenerContext& listenerContext1, const ListenerContext& listenerContext2) {
		return listenerContext1.getListener() == listenerContext2.getListener();
	}
};

}  // namespace dx

namespace std {

template <>
struct hash<dx::ListenerContext> {
	std::size_t operator()(const dx::ListenerContext& lc) const noexcept {
		return std::hash<dx::ListenerContext::ListenerPtr>{}(lc.getListener());
	}
};

}  // namespace std

namespace dx {

struct SubscriptionData {
	unsigned event_types;
	std::unordered_map<std::wstring, SymbolData*> symbols{};
	std::unordered_set<ListenerContext> listeners{};
	std::unordered_set<std::wstring> orderSources{};
	dx_order_source_array_t rawOrderSources{};
	dx_event_subscr_flag subscriptionFlags;
	dxf_long_t time;

	std::vector<dxf_const_string_t> symbolNames{};

	void* connection_context; /* event subscription connection context */

	static void free(SubscriptionData* subscriptionData);

	static int closeEventSubscription(dxf_subscription_t subscriptionId, bool removeFromContext);

	//Fills the non-special order sources array.
	void fillRawOrderSources();

	void clearRawOrderSources();
};

class EventSubscriptionConnectionContext {
	dxf_connection_t connectionHandle;
	std::recursive_mutex mutex{};
	std::unordered_map<std::wstring, SymbolData*> symbols{};
	std::unordered_set<SubscriptionData*> subscriptions{};
public:
	static const std::unordered_set<std::wstring> specialOrderSources;

	explicit EventSubscriptionConnectionContext(dxf_connection_t connectionHandle);

	dxf_connection_t getConnectionHandle();

	std::unordered_set<SubscriptionData*> getSubscriptions();

	SymbolData* subscribeSymbol(dxf_const_string_t symbolName, SubscriptionData* owner);

	void removeSymbolData(SymbolData* symbolData);

	int unsubscribeSymbol(SymbolData* symbolData, SubscriptionData* owner);

	SymbolData* findSymbol(dxf_const_string_t symbolName);

	bool hasAnySymbol();

	template <typename F>
	auto process(F&& f) -> decltype(f(this)) {
		std::lock_guard<std::recursive_mutex> lk(mutex);

		return f(this);
	}

	void addSubscription(SubscriptionData* data);

	void removeSubscription(SubscriptionData* data);

	~EventSubscriptionConnectionContext();
};

}  // namespace dx
