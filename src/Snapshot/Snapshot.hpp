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
}

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional.hpp>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../IdGenerator.hpp"

namespace dx {

class SnapshotKey {
	dx_event_id_t eventId_;
	std::string symbol_;
	nonstd::optional<std::string> source_;
	nonstd::optional<std::int64_t> time_;

public:
	SnapshotKey() : eventId_(static_cast<dx_event_id_t>(-1)), symbol_(), source_(), time_() {}

	SnapshotKey(dx_event_id_t eventId, std::string symbol, nonstd::optional<std::string> source,
				nonstd::optional<std::int64_t> time)
		: eventId_(eventId), symbol_(std::move(symbol)), source_(std::move(source)), time_(std::move(time)) {}

	bool operator==(const SnapshotKey& other) const {
		return eventId_ == other.eventId_ && symbol_ == other.symbol_ && source_ == other.source_ &&
			time_ == other.time_;
	}

	dx_event_id getEventId() const { return eventId_; }
	const std::string& getSymbol() const { return symbol_; }
	const nonstd::optional<std::string>& getSource() const { return source_; }
	const nonstd::optional<std::int64_t>& getTime() const { return time_; }
};

inline void hashCombineInPlace(std::size_t& seed) {}

template <typename T>
inline void hashCombineInPlace(std::size_t& seed, T&& v) {
	using U = typename std::decay<T>::type;

	seed ^= (std::hash<U>{}(std::forward<T>(v)) + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

template <typename T, typename... Ts>
inline void hashCombineInPlace(std::size_t& seed, T&& v, Ts&&... vs) {
	hashCombineInPlace(seed, std::forward<T>(v));
	hashCombineInPlace(seed, std::forward<Ts>(vs)...);
}

template <typename... Ts>
inline std::size_t hashCombine(std::size_t seed, Ts&&... vs) {
	hashCombineInPlace(seed, std::forward<Ts>(vs)...);

	return seed;
}

}  // namespace dx

namespace std {
template <>
struct hash<dx::SnapshotKey> {
	std::size_t operator()(const dx::SnapshotKey& key) const noexcept {
		return dx::hashCombine(static_cast<std::size_t>(key.getEventId()), key.getSymbol(), key.getSource(),
							   key.getTime());
	}
};
}  // namespace std

namespace dx {

using SnapshotRefId = Id;
const SnapshotRefId INVALID_SNAPSHOT_REFERENCE_ID = SnapshotRefId{-1};

struct SnapshotChanges {
	SnapshotKey snapshotKey;

	SnapshotChanges() = default;

	SnapshotChanges(SnapshotKey newSnapshotKey) : snapshotKey{std::move(newSnapshotKey)} {}

	bool isEmpty() const { return true; }
};

struct SnapshotChangesSet {
	SnapshotChanges removals{};
	SnapshotChanges additions{};
	SnapshotChanges updates{};

	SnapshotChangesSet() = default;

	SnapshotChangesSet(SnapshotChanges newRemovals, SnapshotChanges newAdditions, SnapshotChanges newUpdates)
		: removals{std::move(newRemovals)}, additions{std::move(newAdditions)}, updates{std::move(newUpdates)} {}

	bool isEmpty() const { return removals.isEmpty() && additions.isEmpty() && updates.isEmpty(); }
};

struct Snapshot;

struct  SnapshotSubscriber {
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

	void setOnNewSnapshot(std::function<void(const SnapshotChanges&, void*)> onNewSnapshotHandler) {
		std::lock_guard<std::recursive_mutex> guard{mutex_};
		onNewSnapshot_ = std::move(onNewSnapshotHandler);
	}

	void setOnSnapshotUpdate(std::function<void(const SnapshotChanges&, void*)> onSnapshotUpdateHandler) {
		std::lock_guard<std::recursive_mutex> guard{mutex_};
		onSnapshotUpdate_ = std::move(onSnapshotUpdateHandler);
	}

	void setOnIncrementalChange(std::function<void(const SnapshotChangesSet&, void*)> onIncrementalChangeHandler) {
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

struct Snapshot {
protected:
	std::unordered_map<SnapshotRefId, std::shared_ptr<SnapshotSubscriber>> subscribers_;
	std::mutex subscribersMutex_;

public:
	explicit Snapshot() : subscribers_{}, subscribersMutex_{} {}

	bool addSubscriber(const std::shared_ptr<SnapshotSubscriber>& subscriber) {
		std::lock_guard<std::mutex> guard{subscribersMutex_};

		if (!subscriber || !subscriber->isValid() || subscribers_.count(subscriber->getReferenceId()) > 0) {
			return false;
		}

		return subscribers_.emplace(subscriber->getReferenceId(), subscriber).second;
	}
};

struct IndexedEventsSnapshot : public Snapshot {};

struct TimeSeriesEventsSnapshot : public Snapshot {};

struct SnapshotManager {
	using ConnectionKey = dxf_connection_t;

private:
	static std::unordered_map<ConnectionKey, std::shared_ptr<SnapshotManager>> managers;
	std::unordered_map<SnapshotKey, std::shared_ptr<IndexedEventsSnapshot>> indexedEventsSnapshots;
	std::unordered_map<SnapshotRefId, std::shared_ptr<IndexedEventsSnapshot>> indexedEventsSnapshotsByRefId;
	std::mutex indexedEventsSnapshotsMutex;
	std::unordered_map<SnapshotKey, std::shared_ptr<TimeSeriesEventsSnapshot>> timeSeriesEventsSnapshots;
	std::unordered_map<SnapshotRefId, std::shared_ptr<TimeSeriesEventsSnapshot>>
		timeSeriesEventsSnapshotsByRefId;
	std::mutex timeSeriesEventsSnapshotsMutex;

	explicit SnapshotManager()
		: indexedEventsSnapshots{},
		  indexedEventsSnapshotsByRefId{},
		  indexedEventsSnapshotsMutex{},
		  timeSeriesEventsSnapshots{},
		  timeSeriesEventsSnapshotsByRefId{},
		  timeSeriesEventsSnapshotsMutex{} {}

	template <typename T>
	std::shared_ptr<T> getImpl(const SnapshotKey&) {
		return nullptr;
	}

public:
	static std::shared_ptr<SnapshotManager> getInstance(const ConnectionKey& connectionKey) {
		auto found = managers.find(connectionKey);

		if (found != managers.end()) {
			return found->second;
		}

		auto inserted = managers.emplace(connectionKey, std::shared_ptr<SnapshotManager>(new SnapshotManager{}));

		if (inserted.second) {
			return inserted.first->second;
		}

		return managers[connectionKey];
	}

	template <typename T>
	std::shared_ptr<T> get(const SnapshotKey&) {
		return nullptr;
	}

	template <typename T>
	std::pair<std::shared_ptr<T>, Id> create(const ConnectionKey&, const SnapshotKey&, void*) {
		return {nullptr, -1};
	}

	//	bool remove(const SnapshotKey& snapshotKey) {
	//		std::lock_guard<std::mutex> guard(IndexedEventsSnapshot);
	//
	//		// TODO: unsubscribe SnapshotSubscriber-s (by key params) and delete the snapshot if there are no more
	//		// subscribers left
	//
	//		return 0 != snapshots.erase(snapshotKey);
	//	}
};

std::unordered_map<SnapshotManager::ConnectionKey, std::shared_ptr<SnapshotManager>> SnapshotManager::managers{};

template <>
std::shared_ptr<IndexedEventsSnapshot> SnapshotManager::getImpl(const SnapshotKey& snapshotKey) {
	auto found = indexedEventsSnapshots.find(snapshotKey);

	if (found == indexedEventsSnapshots.end()) {
		return nullptr;
	}

	return found->second;
}

template <>
std::shared_ptr<TimeSeriesEventsSnapshot> SnapshotManager::getImpl(const SnapshotKey& snapshotKey) {
	auto found = timeSeriesEventsSnapshots.find(snapshotKey);

	if (found == timeSeriesEventsSnapshots.end()) {
		return nullptr;
	}

	return found->second;
}

template <>
std::shared_ptr<IndexedEventsSnapshot> SnapshotManager::get(const SnapshotKey& snapshotKey) {
	std::lock_guard<std::mutex> guard(indexedEventsSnapshotsMutex);

	return getImpl<IndexedEventsSnapshot>(snapshotKey);
}

template <>
std::shared_ptr<TimeSeriesEventsSnapshot> SnapshotManager::get(const SnapshotKey& snapshotKey) {
	std::lock_guard<std::mutex> guard(timeSeriesEventsSnapshotsMutex);

	return getImpl<TimeSeriesEventsSnapshot>(snapshotKey);
}

template <>
std::pair<std::shared_ptr<IndexedEventsSnapshot>, Id> SnapshotManager::create(const ConnectionKey& connectionKey,
																			  const SnapshotKey& snapshotKey,
																			  void* userData) {
	std::lock_guard<std::mutex> guard(indexedEventsSnapshotsMutex);

	auto snapshot = getImpl<IndexedEventsSnapshot>(snapshotKey);
	auto id = IdGenerator<Snapshot>::get();

	if (snapshot) {
		if (snapshot->addSubscriber(std::shared_ptr<SnapshotSubscriber>(
				new SnapshotSubscriber(id, snapshotKey, userData)))) {
			indexedEventsSnapshotsByRefId.emplace(id, snapshot);

			return {snapshot, id};
		} else {
			return {nullptr, INVALID_SNAPSHOT_REFERENCE_ID};
		}
	}

	auto inserted = indexedEventsSnapshots.emplace(snapshotKey,
												   std::shared_ptr<IndexedEventsSnapshot>(new IndexedEventsSnapshot{}));

	return {inserted.first->second, id};
}

template <>
std::pair<std::shared_ptr<TimeSeriesEventsSnapshot>, Id> SnapshotManager::create(const ConnectionKey& connectionKey,
																				 const SnapshotKey& snapshotKey,
																				 void* userData) {
	std::lock_guard<std::mutex> guard(timeSeriesEventsSnapshotsMutex);

	auto snapshot = getImpl<TimeSeriesEventsSnapshot>(snapshotKey);
	auto id = IdGenerator<Snapshot>::get();

	if (snapshot) {
		if (snapshot->addSubscriber(std::shared_ptr<SnapshotSubscriber>(
				new SnapshotSubscriber(id, snapshotKey, userData)))) {
			timeSeriesEventsSnapshotsByRefId.emplace(id, snapshot);

			return {snapshot, id};
		} else {
			return {nullptr, INVALID_SNAPSHOT_REFERENCE_ID};
		}
	}

	auto inserted = timeSeriesEventsSnapshots.emplace(
		snapshotKey, std::shared_ptr<TimeSeriesEventsSnapshot>(new TimeSeriesEventsSnapshot{}));

	return {inserted.first->second, id};
}

}  // namespace dx
