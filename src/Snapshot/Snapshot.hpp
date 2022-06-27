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

extern "C" {
#include <DXFeed.h>
#include <EventData.h>
#include "../EventSubscription.h"
}

#include <atomic>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional.hpp>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant.hpp>
#include <vector>

#include "../IdGenerator.hpp"
#include "../Utils/Hash.hpp"
#include "SnapshotChanges.hpp"
#include "SnapshotKey.hpp"

namespace dx {

struct UnknownEventType {};

template <dx_event_id_t eventId>
struct EventIdToType {
	using type = UnknownEventType;
};

#define DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER(eventId, eventType) \
	template <>                                                 \
	struct EventIdToType<eventId> {                             \
		using type = eventType;                                 \
	}

DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER(dx_eid_trade, dxf_trade_t);
DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER(dx_eid_quote, dxf_quote_t);
DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER(dx_eid_summary, dxf_summary_t);
DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER(dx_eid_profile, dxf_profile_t);
DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER(dx_eid_order, dxf_order_t);
DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER(dx_eid_time_and_sale, dxf_time_and_sale_t);
DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER(dx_eid_candle, dxf_candle_t);
DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER(dx_eid_trade_eth, dxf_trade_eth_t);
DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER(dx_eid_spread_order, dxf_order_t);
DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER(dx_eid_greeks, dxf_greeks_t);
DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER(dx_eid_theo_price, dxf_theo_price_t);
DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER(dx_eid_underlying, dxf_underlying_t);
DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER(dx_eid_series, dxf_series_t);
DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER(dx_eid_configuration, dxf_configuration_t);

#undef DX_SNAPSHOT_EVENT_ID_TO_TYPE_HELPER

namespace detail {
inline static constexpr unsigned eventIdToBitmask(dx_event_id_t eventId) {
	return 1u << static_cast<unsigned>(eventId);
}

inline static constexpr bool isOnlyIndexedEvent(dx_event_id_t eventId) {
	return dx_eid_order == eventId || dx_eid_spread_order == eventId || dx_eid_series == eventId;
}

inline static constexpr bool isTimeSeriesEvent(dx_event_id_t eventId) {
	return dx_eid_candle == eventId || dx_eid_greeks == eventId || dx_eid_theo_price == eventId ||
		dx_eid_time_and_sale == eventId || dx_eid_underlying == eventId;
}

inline static dx_event_id_t bitmaskToEventId(unsigned bitmask) {
#define DX_DETAIL_BITMASK_TO_EVENT_ID_CASE(eventId) \
	case eventIdToBitmask(eventId):                 \
		return eventId
	switch (bitmask) {
		DX_DETAIL_BITMASK_TO_EVENT_ID_CASE(dx_eid_trade);
		DX_DETAIL_BITMASK_TO_EVENT_ID_CASE(dx_eid_quote);
		DX_DETAIL_BITMASK_TO_EVENT_ID_CASE(dx_eid_summary);
		DX_DETAIL_BITMASK_TO_EVENT_ID_CASE(dx_eid_profile);
		DX_DETAIL_BITMASK_TO_EVENT_ID_CASE(dx_eid_order);
		DX_DETAIL_BITMASK_TO_EVENT_ID_CASE(dx_eid_time_and_sale);
		DX_DETAIL_BITMASK_TO_EVENT_ID_CASE(dx_eid_candle);
		DX_DETAIL_BITMASK_TO_EVENT_ID_CASE(dx_eid_trade_eth);
		DX_DETAIL_BITMASK_TO_EVENT_ID_CASE(dx_eid_spread_order);
		DX_DETAIL_BITMASK_TO_EVENT_ID_CASE(dx_eid_greeks);
		DX_DETAIL_BITMASK_TO_EVENT_ID_CASE(dx_eid_theo_price);
		DX_DETAIL_BITMASK_TO_EVENT_ID_CASE(dx_eid_underlying);
		DX_DETAIL_BITMASK_TO_EVENT_ID_CASE(dx_eid_series);
		DX_DETAIL_BITMASK_TO_EVENT_ID_CASE(dx_eid_configuration);
		default:
			return dx_eid_invalid;
	}
#undef DX_DETAIL_BITMASK_TO_EVENT_ID_CASE
}

inline static bool isOnlyIndexedEvent(unsigned eventIdBitmask) {
	return isOnlyIndexedEvent(bitmaskToEventId(eventIdBitmask));
}

inline static bool isTimeSeriesEvent(unsigned eventIdBitmask) {
	return isTimeSeriesEvent(bitmaskToEventId(eventIdBitmask));
}

}  // namespace detail

using SnapshotRefId = Id;
const SnapshotRefId INVALID_SNAPSHOT_REFERENCE_ID = SnapshotRefId{-1};
using ConnectionKey = dxf_connection_t;

constexpr const char* ALL_ORDER_SOURCES[] = {
	"NTV",	 /// NASDAQ Total View.
	"ntv",	 /// NASDAQ Total View. Record for price level book.
	"NFX",	 /// NASDAQ Futures Exchange.
	"ESPD",	 /// NASDAQ eSpeed.
	"XNFI",	 /// NASDAQ Fixed Income.
	"ICE",	 /// Intercontinental Exchange.
	"ISE",	 /// International Securities Exchange.
	"DEA",	 /// Direct-Edge EDGA Exchange.
	"DEX",	 /// Direct-Edge EDGX Exchange.
	"BYX",	 /// Bats BYX Exchange.
	"BZX",	 /// Bats BZX Exchange.
	"BATE",	 /// Bats Europe BXE Exchange.
	"CHIX",	 /// Bats Europe CXE Exchange.
	"CEUX",	 /// Bats Europe DXE Exchange.
	"BXTR",	 /// Bats Europe TRF.
	"IST",	 /// Borsa Istanbul Exchange.
	"BI20",	 /// Borsa Istanbul Exchange. Record for particular top 20 order book.
	"ABE",	 /// ABE (abe.io) exchange.
	"FAIR",	 /// FAIR (FairX) exchange.
	"GLBX",	 /// CME Globex.
	"glbx",	 /// CME Globex. Record for price level book.
	"ERIS",	 /// Eris Exchange group of companies.
	"XEUR",	 /// Eurex Exchange.
	"xeur",	 /// Eurex Exchange. Record for price level book.
	"CFE",	 /// CBOE Futures Exchange.
	"C2OX",	 /// CBOE Options C2 Exchange.
	"SMFE",	 /// Small Exchange.
	"smfe",	 /// Small Exchange. Record for price level book.
	"iex",	 /// Investors exchange. Record for price level book.
	"MEMX",	 /// Members Exchange.
	"memx",	 /// Members Exchange. Record for price level book.
};

constexpr const char* ALL_SPECIAL_ORDER_SOURCES[] = {
	"DEFAULT",	/// back compatibility with Java API

	/**
	 * Bid side of a composite Quote. It is a synthetic source. The subscription on composite Quote event is observed
	 * when this source is subscribed to.
	 */
	"COMPOSITE_BID",

	/**
	 * Ask side of a composite Quote. It is a synthetic source. The subscription on composite Quote event is observed
	 * when this source is subscribed to.
	 */
	"COMPOSITE_ASK",

	/**
	 * Bid side of a regional Quote. It is a synthetic source. The subscription on regional Quote event is observed
	 * when this source is subscribed to.
	 */
	"REGIONAL_BID",

	/**
	 * Ask side of a composite Quote. It is a synthetic source. The subscription on regional Quote event is observed
	 * when this source is subscribed to.
	 */
	"REGIONAL_ASK",

	/**
	 * Bid side of an aggregate order book (futures depth and NASDAQ Level II). It is a synthetic source. This source
	 * cannot be directly published via dxFeed API, but otherwise it is fully operational.
	 */
	"AGGREGATE_BID",

	/**
	 * Ask side of an aggregate order book (futures depth and NASDAQ Level II). It is a synthetic source. This source
	 * cannot be directly published via dxFeed API, but otherwise it is fully operational.
	 */
	"AGGREGATE_ASK",

	"EMPTY",  // back compatibility with .NET API

	/**
	 * Bid and ask sides of a composite Quote. It is a synthetic source. The subscription on composite Quote event is
	 * observed when this source is subscribed to.
	 */
	"COMPOSITE",

	/**
	 * Bid and ask sides of a regional Quote. It is a synthetic source. The subscription on regional Quote event is
	 * observed when this source is subscribed to.
	 */
	"REGIONAL",

	/**
	 * Bid side of an aggregate order book (futures depth and NASDAQ Level II). It is a synthetic source. This source
	 * cannot be directly published via dxFeed API, but otherwise it is fully operational.
	 */
	"AGGREGATE"};

enum class SpecialOrderSource : int {
	DEFAULT = 0,
	COMPOSITE_BID = 1,
	COMPOSITE_ASK = 2,
	REGIONAL_BID = 3,
	REGIONAL_ASK = 4,
	AGGREGATE_BID = 5,
	AGGREGATE_ASK = 6,
	EMPTY = 7,
	COMPOSITE = 8,
	REGIONAL = 9,
	AGGREGATE = 10,
};

struct SnapshotSubscriber {
	using SnapshotHandler = std::function<void(const SnapshotChanges&, void*)>;
	using IncrementalSnapshotHandler = std::function<void(const SnapshotChangesSet&, void*)>;

private:
	std::atomic<SnapshotRefId> referenceId_;
	SnapshotKey filter_;
	void* userData_;
	mutable std::recursive_mutex mutex_;
	std::atomic<bool> isValid_;

	SnapshotHandler onNewSnapshot_{};
	SnapshotHandler onSnapshotUpdate_{};
	IncrementalSnapshotHandler onIncrementalChange_{};

public:
	SnapshotSubscriber(SnapshotRefId referenceId, SnapshotKey filter, void* userData)
		: referenceId_{referenceId}, filter_(std::move(filter)), userData_{userData}, mutex_(), isValid_(true) {}

	SnapshotSubscriber(SnapshotSubscriber&& sub) noexcept : mutex_() {
		referenceId_ = sub.referenceId_.load();
		sub.referenceId_ = INVALID_SNAPSHOT_REFERENCE_ID;
		filter_ = std::move(sub.filter_);
		userData_ = sub.userData_;
		onNewSnapshot_ = std::move(sub.onNewSnapshot_);
		onSnapshotUpdate_ = std::move(sub.onSnapshotUpdate_);
		onIncrementalChange_ = std::move(sub.onIncrementalChange_);
		isValid_ = sub.isValid_.load();
		sub.isValid_ = false;
	}

	SnapshotSubscriber& operator=(SnapshotSubscriber&& sub) noexcept {
		if (this == &sub) {
			return *this;
		}

		referenceId_ = sub.referenceId_.load();
		sub.referenceId_ = INVALID_SNAPSHOT_REFERENCE_ID;
		filter_ = std::move(sub.filter_);
		userData_ = sub.userData_;
		onNewSnapshot_ = std::move(sub.onNewSnapshot_);
		onSnapshotUpdate_ = std::move(sub.onSnapshotUpdate_);
		onIncrementalChange_ = std::move(sub.onIncrementalChange_);
		isValid_ = sub.isValid_.load();
		sub.isValid_ = false;

		return *this;
	}

	void setOnNewSnapshotHandler(SnapshotHandler onNewSnapshotHandler) {
		std::lock_guard<std::recursive_mutex> guard{mutex_};
		onNewSnapshot_ = std::move(onNewSnapshotHandler);
	}

	void setOnSnapshotUpdateHandler(SnapshotHandler onSnapshotUpdateHandler) {
		std::lock_guard<std::recursive_mutex> guard{mutex_};
		onSnapshotUpdate_ = std::move(onSnapshotUpdateHandler);
	}

	void setOnIncrementalChangeHandler(IncrementalSnapshotHandler onIncrementalChangeHandler) {
		std::lock_guard<std::recursive_mutex> guard{mutex_};
		onIncrementalChange_ = std::move(onIncrementalChangeHandler);
	}

	SnapshotRefId getReferenceId() const { return referenceId_; }

	SnapshotKey getFilter() const {
		std::lock_guard<std::recursive_mutex> guard{mutex_};

		return filter_;
	}

	bool isValid() { return isValid_; }
};

struct SnapshotManager;

struct Snapshot {
protected:
	std::unordered_map<SnapshotRefId, std::shared_ptr<SnapshotSubscriber>> subscribers_;
	mutable std::mutex subscribersMutex_;

	friend SnapshotManager;

	bool addSubscriber(const std::shared_ptr<SnapshotSubscriber>& subscriber) {
		std::lock_guard<std::mutex> guard{subscribersMutex_};

		if (!subscriber || !subscriber->isValid() || subscribers_.count(subscriber->getReferenceId()) > 0) {
			return false;
		}

		return subscribers_.emplace(subscriber->getReferenceId(), subscriber).second;
	}

	bool removeSubscriber(SnapshotRefId snapshotRefId) {
		std::lock_guard<std::mutex> guard{subscribersMutex_};

		return subscribers_.erase(snapshotRefId) > 0;
	}

public:
	explicit Snapshot() : subscribers_{}, subscribersMutex_{} {}

	bool hasSubscriptions() const {
		std::lock_guard<std::mutex> guard{subscribersMutex_};

		return !subscribers_.empty();
	}

	virtual ~Snapshot() = default;
};

struct OnlyIndexedEventKey {
	dxf_long_t index;
	dxf_long_t time;
};

}  // namespace dx

namespace std {
template <>
struct hash<dx::OnlyIndexedEventKey> {
	std::size_t operator()(const dx::OnlyIndexedEventKey& key) const noexcept {
		return dx::hashCombine(key.index, key.time);
	}
};
}  // namespace std

namespace dx {

struct OnlyIndexedEventsSnapshot final : public Snapshot,
										 public std::enable_shared_from_this<OnlyIndexedEventsSnapshot> {
	using EventType = nonstd::variant<dxf_order_t /*order + spread order*/, dxf_series_t>;

private:
	SnapshotKey key_;
	std::unordered_map<OnlyIndexedEventKey, EventType> events_;
	std::unordered_map<OnlyIndexedEventKey, EventType> transaction_;
	dxf_connection_t con_;
	dxf_subscription_t sub_;
	dxf_event_listener_v2_t listener_;
	std::atomic_bool isValid_;

	struct SubscriptionParams {
		dx_record_info_id_t recordInfoId;
		dx_event_subscr_flag subscriptionFlags;
		std::string source;
	};

	inline static SubscriptionParams collectSubscriptionParams(const SnapshotKey& key) {
		const auto& eventId = key.getEventId();
		const auto& source = key.getSource().value_or(std::string{});

		dx_record_info_id_t recordInfoId = dx_rid_invalid;
		dx_event_subscr_flag subscriptionFlags = dx_esf_default;
		std::string resultSource{};

		if (eventId == dx_eid_order) {
			if (source == ALL_SPECIAL_ORDER_SOURCES[static_cast<int>(SpecialOrderSource::AGGREGATE)] ||
				source == ALL_SPECIAL_ORDER_SOURCES[static_cast<int>(SpecialOrderSource::AGGREGATE_ASK)] ||
				source == ALL_SPECIAL_ORDER_SOURCES[static_cast<int>(SpecialOrderSource::AGGREGATE_BID)]) {
				recordInfoId = dx_rid_market_maker;
				subscriptionFlags = static_cast<dx_event_subscr_flag>(subscriptionFlags | dx_esf_sr_market_maker_order);
				resultSource = ALL_SPECIAL_ORDER_SOURCES[static_cast<int>(SpecialOrderSource::AGGREGATE)];
			} else {
				recordInfoId = dx_rid_order;
				resultSource = source;
			}
		} else if (eventId == dx_eid_spread_order) {
			recordInfoId = dx_rid_spread_order;
		} else if (eventId == dx_eid_series) {
			recordInfoId = dx_rid_series;
		} else {
			recordInfoId = dx_rid_invalid;
		}

		return {recordInfoId, subscriptionFlags, resultSource};
	}

	void handleEventsData(int eventBitMask, dxf_const_string_t symbol, const dxf_event_data_t* data, int dataCount,
						  const dxf_event_params_t* eventParams) {
		if (dataCount < 1) {
			return;
		}

		auto eventId = detail::bitmaskToEventId(eventBitMask);

		if (eventId == dx_eid_invalid) {
			return;
		}

		// TODO: Snapshot logic

		if (hasSubscriptions()) {
		}
	}

	OnlyIndexedEventsSnapshot(dxf_connection_t connection, SnapshotKey key)
		: key_(std::move(key)),
		  events_(),
		  transaction_(),
		  con_{connection},
		  sub_{nullptr},
		  listener_{nullptr},
		  isValid_{false} {}

	~OnlyIndexedEventsSnapshot() override {
		if (isValid_) {
			if (listener_) {
				dxf_detach_event_listener_v2(sub_, listener_);
			}

			dxf_close_subscription(sub_);
		}
	}

public:
	static std::shared_ptr<OnlyIndexedEventsSnapshot> create(dxf_connection_t connection, SnapshotKey key) {
		if (connection == nullptr) {
			return nullptr;
		}

		auto snapshot = new OnlyIndexedEventsSnapshot(connection, std::move(key));

		auto subscriptionParams = collectSubscriptionParams(snapshot->key_);

		if (subscriptionParams.recordInfoId == dx_rid_invalid) {
			return nullptr;
		}

		auto eventType = DX_EVENT_BIT_MASK(snapshot->key_.getEventId());
		auto sub = dx_create_event_subscription(connection, eventType, subscriptionParams.subscriptionFlags, 0, 0);

		if (sub == dx_invalid_subscription) {
			return nullptr;
		}

		if ((subscriptionParams.recordInfoId == dx_rid_order ||
			 subscriptionParams.recordInfoId == dx_rid_market_maker) &&
			!subscriptionParams.source.empty()) {
			auto sourceWideString = StringConverter::utf8ToWString(subscriptionParams.source);

			dx_add_order_source(sub, sourceWideString.c_str());
		}

		auto listener = [](int eventBitMask, dxf_const_string_t symbol, const dxf_event_data_t* data, int dataCount,
						   const dxf_event_params_t* eventParams, void* userData) {
			auto snapshot = reinterpret_cast<OnlyIndexedEventsSnapshot*>(userData);

			snapshot->handleEventsData(eventBitMask, symbol, data, dataCount, eventParams);
		};

		dx_add_listener_v2(sub, listener, reinterpret_cast<void*>(snapshot));

		auto symbolWideString = StringConverter::utf8ToWString(key.getSymbol());
		auto addSymbolResult = dxf_add_symbol(sub, symbolWideString.c_str());

		if (addSymbolResult == DXF_FAILURE) {
			dxf_detach_event_listener_v2(sub, listener);
			dxf_close_subscription(sub);
		}

		snapshot->sub_ = sub;
		snapshot->listener_ = listener;
		snapshot->isValid_ = true;

		return snapshot->shared_from_this();
	}
};

struct TimeSeriesEventsSnapshot final : public Snapshot {};

struct SnapshotManager {
	using CommonSnapshotPtr = nonstd::variant<std::shared_ptr<OnlyIndexedEventsSnapshot>,
											  std::shared_ptr<TimeSeriesEventsSnapshot>, nullptr_t>;

private:
	static std::unordered_map<ConnectionKey, std::shared_ptr<SnapshotManager>> managers_;
	// one to one
	std::unordered_map<SnapshotKey, std::shared_ptr<OnlyIndexedEventsSnapshot>> indexedEventsSnapshots_;
	// many to one
	std::unordered_map<SnapshotRefId, std::shared_ptr<OnlyIndexedEventsSnapshot>> indexedEventsSnapshotsByRefId_;
	std::mutex indexedEventsSnapshotsMutex_;
	// one to one
	std::unordered_map<SnapshotKey, std::shared_ptr<TimeSeriesEventsSnapshot>> timeSeriesEventsSnapshots_;
	// many to one
	std::unordered_map<SnapshotRefId, std::shared_ptr<TimeSeriesEventsSnapshot>> timeSeriesEventsSnapshotsByRefId_;
	std::mutex timeSeriesEventsSnapshotsMutex_;
	// one to one
	std::unordered_map<SnapshotRefId, std::shared_ptr<SnapshotSubscriber>> subscribers_;
	std::mutex subscribersMutex_;

	explicit SnapshotManager()
		: indexedEventsSnapshots_{},
		  indexedEventsSnapshotsByRefId_{},
		  indexedEventsSnapshotsMutex_{},
		  timeSeriesEventsSnapshots_{},
		  timeSeriesEventsSnapshotsByRefId_{},
		  timeSeriesEventsSnapshotsMutex_{},
		  subscribers_{},
		  subscribersMutex_{} {}

	template <typename T>
	std::shared_ptr<T> getImpl(const SnapshotKey&) {
		return nullptr;
	}

	template <typename T>
	std::shared_ptr<T> getImpl(SnapshotRefId) {
		return nullptr;
	}

	bool closeImpl(SnapshotRefId snapshotRefId, CommonSnapshotPtr snapshotPtr) {
		// TODO: overloaded "idiom" + generic lambda
		struct CloseImplVisitor {
			SnapshotRefId snapshotRefId;

			bool operator()(std::shared_ptr<OnlyIndexedEventsSnapshot> indexedEventsSnapshotPtr) const { return false; }

			bool operator()(std::shared_ptr<TimeSeriesEventsSnapshot> timeSeriesEventsSnapshotPtr) const {
				return false;
			}

			bool operator()(nullptr_t) const {
				// TODO: warning
				return true;
			}
		};

		return nonstd::visit(CloseImplVisitor{snapshotRefId}, snapshotPtr);
	}

public:
	/// Returns the SnapshotManager instance by a connection key or creates the new one and returns it.
	/// \param connectionKey The connection key (i.e. the pointer to connection context structure)
	/// \return The SnapshotManager instance.
	static std::shared_ptr<SnapshotManager> getInstance(const ConnectionKey& connectionKey) {
		auto found = managers_.find(connectionKey);

		if (found != managers_.end()) {
			return found->second;
		}

		auto inserted = managers_.emplace(connectionKey, std::shared_ptr<SnapshotManager>(new SnapshotManager{}));

		if (inserted.second) {
			return inserted.first->second;
		}

		return managers_[connectionKey];
	}

	/// Returns a pointer to the snapshot instance by the snapshot key (an event id, a symbol, an optional source, an
	/// optional `timeFrom`) \tparam T The Snapshot type (IndexedEventsSnapshot or TimeSeriesEventsSnapshot) \return A
	/// shared pointer to the snapshot instance or std::shared_ptr{nullptr}
	template <typename T>
	std::shared_ptr<T> get(const SnapshotKey&) {
		return nullptr;
	}

	/// Returns a pointer to the snapshot instance (indexed or time series) by the snapshot reference id (1 to 1 for a
	/// SnapshotSubscriber) \param snapshotRefId The snapshot reference id \return A shared pointer to the snapshot
	/// instance or nullptr
	CommonSnapshotPtr get(SnapshotRefId snapshotRefId);

	/// Creates the new snapshot instance and the new SnapshotSubscriber instance or adds new SnapshotSubscriber
	/// instance to the existing snapshot. \tparam T The Snapshot type (IndexedEventsSnapshot or
	/// TimeSeriesEventsSnapshot) \return The pair of shared pointer to the snapshot instance and reference id to the
	/// snapshot (1 to 1 for a SnapshotSubscriber)
	template <typename T>
	std::pair<std::shared_ptr<T>, SnapshotRefId> create(const ConnectionKey&, const SnapshotKey&, void*) {
		return {nullptr, INVALID_SNAPSHOT_REFERENCE_ID};
	}

	bool setOnNewSnapshotHandler(SnapshotRefId snapshotRefId,
								 SnapshotSubscriber::SnapshotHandler onNewSnapshotHandler) {
		std::lock_guard<std::mutex> guard{subscribersMutex_};

		auto found = subscribers_.find(snapshotRefId);

		if (found != subscribers_.end()) {
			found->second->setOnNewSnapshotHandler(std::move(onNewSnapshotHandler));

			return true;
		}

		return false;
	}

	bool setOnSnapshotUpdateHandler(SnapshotRefId snapshotRefId,
									SnapshotSubscriber::SnapshotHandler onSnapshotUpdateHandler) {
		std::lock_guard<std::mutex> guard{subscribersMutex_};

		auto found = subscribers_.find(snapshotRefId);

		if (found != subscribers_.end()) {
			found->second->setOnSnapshotUpdateHandler(std::move(onSnapshotUpdateHandler));

			return true;
		}

		return false;
	}

	bool setOnIncrementalChangeHandler(SnapshotRefId snapshotRefId,
									   SnapshotSubscriber::IncrementalSnapshotHandler onIncrementalChangeHandler) {
		std::lock_guard<std::mutex> guard{subscribersMutex_};

		auto found = subscribers_.find(snapshotRefId);

		if (found != subscribers_.end()) {
			found->second->setOnIncrementalChangeHandler(std::move(onIncrementalChangeHandler));

			return true;
		}

		return false;
	}

	bool close(SnapshotRefId snapshotRefId) {
		auto snapshotPtr = get(snapshotRefId);

		return closeImpl(snapshotRefId, snapshotPtr);
	}
};

std::unordered_map<ConnectionKey, std::shared_ptr<SnapshotManager>> SnapshotManager::managers_{};

template <>
std::shared_ptr<OnlyIndexedEventsSnapshot> SnapshotManager::getImpl(const SnapshotKey& snapshotKey) {
	auto found = indexedEventsSnapshots_.find(snapshotKey);

	if (found == indexedEventsSnapshots_.end()) {
		return nullptr;
	}

	return found->second;
}

template <>
std::shared_ptr<TimeSeriesEventsSnapshot> SnapshotManager::getImpl(const SnapshotKey& snapshotKey) {
	auto found = timeSeriesEventsSnapshots_.find(snapshotKey);

	if (found == timeSeriesEventsSnapshots_.end()) {
		return nullptr;
	}

	return found->second;
}

template <>
std::shared_ptr<OnlyIndexedEventsSnapshot> SnapshotManager::getImpl(SnapshotRefId snapshotRefId) {
	auto found = indexedEventsSnapshotsByRefId_.find(snapshotRefId);

	if (found == indexedEventsSnapshotsByRefId_.end()) {
		return nullptr;
	}

	return found->second;
}

template <>
std::shared_ptr<TimeSeriesEventsSnapshot> SnapshotManager::getImpl(SnapshotRefId snapshotRefId) {
	auto found = timeSeriesEventsSnapshotsByRefId_.find(snapshotRefId);

	if (found == timeSeriesEventsSnapshotsByRefId_.end()) {
		return nullptr;
	}

	return found->second;
}

template <>
std::shared_ptr<OnlyIndexedEventsSnapshot> SnapshotManager::get(const SnapshotKey& snapshotKey) {
	std::lock_guard<std::mutex> guard(indexedEventsSnapshotsMutex_);

	return getImpl<OnlyIndexedEventsSnapshot>(snapshotKey);
}

template <>
std::shared_ptr<TimeSeriesEventsSnapshot> SnapshotManager::get(const SnapshotKey& snapshotKey) {
	std::lock_guard<std::mutex> guard(timeSeriesEventsSnapshotsMutex_);

	return getImpl<TimeSeriesEventsSnapshot>(snapshotKey);
}

SnapshotManager::CommonSnapshotPtr SnapshotManager::get(SnapshotRefId snapshotRefId) {
	{
		std::lock_guard<std::mutex> guard(indexedEventsSnapshotsMutex_);

		auto snapshot = getImpl<OnlyIndexedEventsSnapshot>(snapshotRefId);

		if (snapshot) {
			return snapshot;
		}
	}
	{
		std::lock_guard<std::mutex> guard(timeSeriesEventsSnapshotsMutex_);

		auto snapshot = getImpl<TimeSeriesEventsSnapshot>(snapshotRefId);

		if (snapshot) {
			return snapshot;
		}
	}

	return nullptr;
}

template <>
Id IdGenerator<Snapshot>::id{0};

template <>
std::pair<std::shared_ptr<OnlyIndexedEventsSnapshot>, Id> SnapshotManager::create(const ConnectionKey& connectionKey,
																				  const SnapshotKey& snapshotKey,
																				  void* userData) {
	std::lock_guard<std::mutex> guard(indexedEventsSnapshotsMutex_);

	auto snapshot = getImpl<OnlyIndexedEventsSnapshot>(snapshotKey);
	auto id = IdGenerator<Snapshot>::get();

	if (snapshot) {
		std::shared_ptr<SnapshotSubscriber> subscriber(new SnapshotSubscriber(id, snapshotKey, userData));

		if (snapshot->addSubscriber(subscriber)) {
			indexedEventsSnapshotsByRefId_.emplace(id, snapshot);

			std::lock_guard<std::mutex> subscribersGuard(subscribersMutex_);

			subscribers_.emplace(id, subscriber);

			return {snapshot, id};
		} else {
			return {nullptr, INVALID_SNAPSHOT_REFERENCE_ID};
		}
	}

	auto newSnapshot = OnlyIndexedEventsSnapshot::create(connectionKey, snapshotKey);

	if (!newSnapshot) {
		return {nullptr, INVALID_SNAPSHOT_REFERENCE_ID};
	}

	auto inserted = indexedEventsSnapshots_.emplace(snapshotKey, newSnapshot);

	return {inserted.first->second, id};
}

template <>
std::pair<std::shared_ptr<TimeSeriesEventsSnapshot>, Id> SnapshotManager::create(const ConnectionKey& connectionKey,
																				 const SnapshotKey& snapshotKey,
																				 void* userData) {
	std::lock_guard<std::mutex> guard(timeSeriesEventsSnapshotsMutex_);

	auto snapshot = getImpl<TimeSeriesEventsSnapshot>(snapshotKey);
	auto id = IdGenerator<Snapshot>::get();

	if (snapshot) {
		std::shared_ptr<SnapshotSubscriber> subscriber(new SnapshotSubscriber(id, snapshotKey, userData));

		if (snapshot->addSubscriber(subscriber)) {
			timeSeriesEventsSnapshotsByRefId_.emplace(id, snapshot);

			std::lock_guard<std::mutex> subscribersGuard(subscribersMutex_);

			subscribers_.emplace(id, subscriber);

			return {snapshot, id};
		} else {
			return {nullptr, INVALID_SNAPSHOT_REFERENCE_ID};
		}
	}

	auto inserted = timeSeriesEventsSnapshots_.emplace(
		snapshotKey, std::shared_ptr<TimeSeriesEventsSnapshot>(new TimeSeriesEventsSnapshot{}));

	return {inserted.first->second, id};
}

}  // namespace dx
