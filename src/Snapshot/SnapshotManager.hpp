#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

namespace dx {

struct Snapshot {

};

struct SnapshotManager {
	using ConnectionId = std::size_t;

	static std::unordered_map<ConnectionId, std::shared_ptr<SnapshotManager>> instances;

	static std::shared_ptr<SnapshotManager> get(ConnectionId connectionId) {
		auto found = instances.find(connectionId);

		if (found != instances.end()) {
			return found->second;
		}

		auto inserted = instances.emplace(connectionId, std::make_shared<SnapshotManager>());

		if (inserted.second) {
			return inserted.first->second;
		}

		return instances[connectionId];
	}
};

}