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

#include <memory>
#include <optional.hpp>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <mutex>

namespace dx {

struct Snapshot {

};

struct SnapshotKey {
	dx_event_id_t eventId;
	std::string symbol;
	nonstd::optional<std::string> source;
	nonstd::optional<std::int64_t> time;
};

template <typename ConnectionKey = dxf_connection_t>
struct SnapshotManager {
private:

	static std::unordered_map<ConnectionKey, std::shared_ptr<SnapshotManager>> managers;
	std::unordered_map<SnapshotKey, std::shared_ptr<Snapshot>> snapshots;
	std::mutex snapshotsMutex;

public:

	static std::shared_ptr<SnapshotManager> getInstance(const ConnectionKey& connectionKey) {
		auto found = managers.find(connectionKey);

		if (found != managers.end()) {
			return found->second;
		}

		auto inserted = managers.emplace(connectionKey, std::make_shared<SnapshotManager>());

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

	std::shared_ptr<Snapshot> create(const ConnectionKey& connectionKey, const SnapshotKey& snapshotKey) {
		//TODO: create snapshot or SnapshotSubscriber-s.

		return nullptr;
	}

	bool remove(const SnapshotKey& snapshotKey) {
		std::lock_guard<std::mutex> guard(snapshotsMutex);

		//TODO: unsubscribe SnapshotSubscriber-s (by key params) and delete the snapshot if there are no more subscribers left

		return 0 != snapshots.erase(snapshotKey);
	}
};

}  // namespace dx