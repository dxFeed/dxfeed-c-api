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

struct SnapshotKey {
	dx_event_id_t eventId;
	std::string symbol;
	nonstd::optional<std::string> source;
	nonstd::optional<std::int64_t> time;

	bool operator==(const SnapshotKey& other) const {
		return eventId == other.eventId && symbol == other.symbol && source == other.source && time == other.time;
	}
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
		return dx::hashCombine(static_cast<std::size_t>(key.eventId), key.symbol, key.source, key.time);
	}
};
}  // namespace std

namespace dx {

struct SnapshotChanges {};

struct SnapshotSubscriber {
	SnapshotKey filter;
	void* userData;

	std::function<void(const SnapshotChanges&, void*)> onNewSnapshot_;
	std::function<void(const SnapshotChanges&, void*)> onSnapshotUpdate_;
	std::function<void(const SnapshotChanges&, void*)> onIncrementalChange_;
};

struct Snapshot {
	std::vector<SnapshotSubscriber> subscribers;
	std::mutex subscribersMutex;

	explicit Snapshot() : subscribers{}, subscribersMutex{} {}
};

struct IndexedEventsSnapshot : Snapshot {};

struct TimeSeriesEventsSnapshot : Snapshot {};

struct SnapshotManager {
	using ConnectionKey = dxf_connection_t;

private:
	static std::unordered_map<ConnectionKey, std::shared_ptr<SnapshotManager>> managers;
	std::unordered_map<SnapshotKey, std::shared_ptr<IndexedEventsSnapshot>> indexedEventsSnapshots;
	std::mutex indexedEventsSnapshotsMutex;
	std::unordered_map<SnapshotKey, std::shared_ptr<TimeSeriesEventsSnapshot>> timeSeriesEventsSnapshots;
	std::mutex timeSeriesEventsSnapshotsMutex;

	explicit SnapshotManager()
		: indexedEventsSnapshots{},
		  indexedEventsSnapshotsMutex{},
		  timeSeriesEventsSnapshots{},
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

		auto inserted = managers.emplace(
			connectionKey, std::shared_ptr<SnapshotManager>(new SnapshotManager{}));

		if (inserted.second) {
			return inserted.first->second;
		}

		return managers[connectionKey];
	}

	template <typename T>
	std::shared_ptr<T> get(const SnapshotKey&) {
		return nullptr;
	}

	//
	//	template <typename T>
	//	std::shared_ptr<T> create(const ConnectionKey&, const SnapshotKey&, void*) {
	//		return nullptr;
	//	}
	//
	//	template <>
	//	std::shared_ptr<IndexedEventsSnapshot> create(const ConnectionKey& connectionKey, const SnapshotKey&
	// snapshotKey, 									 void* userData) {
	//		// TODO: create snapshot or SnapshotSubscriber-s.
	//
	//		std::lock_guard<std::mutex> guard(indexedEventsSnapshotsMutex);
	//
	//		auto snapshot = getImpl(snapshotKey);
	//
	//		if (!snapshot) {
	//		}
	//
	//		return nullptr;
	//	}
	//
	//	bool remove(const SnapshotKey& snapshotKey) {
	//		std::lock_guard<std::mutex> guard(IndexedEventsSnapshot);
	//
	//		// TODO: unsubscribe SnapshotSubscriber-s (by key params) and delete the snapshot if there are no more
	//		// subscribers left
	//
	//		return 0 != snapshots.erase(snapshotKey);
	//	}
};

std::unordered_map<SnapshotManager::ConnectionKey, std::shared_ptr<SnapshotManager>>
	SnapshotManager::managers{};

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


}  // namespace dx
