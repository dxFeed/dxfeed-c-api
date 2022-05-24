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

namespace dx {

struct SnapshotKey {
	dx_event_id_t eventId;
	std::string symbol;
	nonstd::optional<std::string> source;
	nonstd::optional<std::int64_t> time;
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

template <typename ConnectionKey = dxf_connection_t>
struct SnapshotManager {
private:
	static std::unordered_map<ConnectionKey, std::shared_ptr<SnapshotManager>> managers;
	std::unordered_map<SnapshotKey, std::shared_ptr<Snapshot>> snapshots;
	std::mutex snapshotsMutex;

	explicit SnapshotManager() : snapshots{}, snapshotsMutex{} {}

public:
	static std::shared_ptr<SnapshotManager> getInstance(const ConnectionKey& connectionKey) {
		auto found = managers.find(connectionKey);

		if (found != managers.end()) {
			return found->second;
		}

		auto inserted = managers.emplace(
			connectionKey, std::shared_ptr<SnapshotManager<ConnectionKey>>(new SnapshotManager<ConnectionKey>{}));

		if (inserted.second) {
			return inserted.first->second;
		}

		return managers[connectionKey];
	}

	std::shared_ptr<Snapshot> get(const SnapshotKey& snapshotKey) {
		std::lock_guard<std::mutex> guard(snapshotsMutex);

		auto found = snapshots.find(snapshotKey);

		if (found == snapshots.end()) {
			return nullptr;
		}

		return found->second;
	}

	std::shared_ptr<Snapshot> create(const ConnectionKey& connectionKey, const SnapshotKey& snapshotKey,
									 void* userData) {
		// TODO: create snapshot or SnapshotSubscriber-s.

		std::lock_guard<std::mutex> guard(snapshotsMutex);

		auto found = snapshots.find(snapshotKey);

		if (found == snapshots.end()) {
			return nullptr;
		}

		return found->second;

		return nullptr;
	}

	bool remove(const SnapshotKey& snapshotKey) {
		std::lock_guard<std::mutex> guard(snapshotsMutex);

		// TODO: unsubscribe SnapshotSubscriber-s (by key params) and delete the snapshot if there are no more
		// subscribers left

		return 0 != snapshots.erase(snapshotKey);
	}
};

template <typename ConnectionKey>
std::unordered_map<ConnectionKey, std::shared_ptr<SnapshotManager<ConnectionKey>>>
	SnapshotManager<ConnectionKey>::managers{};

}  // namespace dx
