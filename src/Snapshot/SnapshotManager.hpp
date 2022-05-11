#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>

#include "../../wrappers/cpp/thirdparty/optional-lite/include/nonstd/optional.hpp"
#include "../../wrappers/cpp/thirdparty/variant-lite/include/nonstd/variant.hpp"

namespace dx {

template <typename RecordIdType, typename SymbolType, typename SourceType, typename TimestampType>
struct SnapshotKey {
	RecordIdType recordId;
	SymbolType symbol;
	nonstd::optional<SourceType> source;
	nonstd::optional<TimestampType> fromTime;
};

// TODO std::hash

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

template <typename Event>
struct IsTimeSeriesEvent : std::false_type {
};

template <typename Event>
struct IsIndexedEvent : std::false_type {
};

struct TimeSeriesEventSnapshot {
};

struct IndexedEventSnapshot {};

/**
 * Creates, manages and accesses snapshots.
 * Creates, manages and accesses static SnapshotManager instances.
 * Thread safe.
 */
struct SnapshotManager {
	using SnapshotType = nonstd::variant<TimeSeriesEventSnapshot, IndexedEventSnapshot>;
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

	template <typename EventType, typename Symbol, typename Source>
	typename std::enable_if<!IsTimeSeriesEvent<EventType>::value && IsIndexedEvent<EventType>::value, IndexedEventSnapshot>::type
	getSnapshot(EventType&& eventType, Symbol&& symbol, Source&& source) {
		return {};
	}

	template <typename EventType, typename Symbol, typename Source, typename Timestamp>
	typename std::enable_if<IsTimeSeriesEvent<EventType>::value, TimeSeriesEventSnapshot>::type
	getSnapshot(EventType&& eventType, Symbol&& symbol, Source&& source, Timestamp&& timestamp) {
		return {};
	}
};

}  // namespace dx