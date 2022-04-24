#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include "../../wrappers/cpp/thirdparty/variant-lite/include/nonstd/variant.hpp"
#include "../../wrappers/cpp/thirdparty/optional-lite/include/nonstd/optional.hpp"

namespace dx {

template <typename RecordIdType, typename SymbolType, typename SourceType, typename TimestampType>
struct SnapshotKey {
	RecordIdType recordId;
	SymbolType symbol;
	nonstd::optional<SourceType> source;
	nonstd::optional<TimestampType> fromTime;
};

//TODO std::hash

template <typename IndexType, typename TimestampType>
struct EventKey {
	nonstd::optional<IndexType> index;
	nonstd::optional<TimestampType> time;
};

template <typename SnapshotKey, typename EventKey, typename... EventTypes>
struct Snapshot {
	using EventType = nonstd::variant<EventTypes...>;

private:

	SnapshotKey key_;
	std::unordered_map<EventKey, EventType> events_;

public:

	explicit Snapshot() = default;

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