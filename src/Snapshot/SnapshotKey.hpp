#pragma once

extern "C" {
#include <EventData.h>
}

#include <string>
#include <cstdint>
#include <cstddef>
#include <utility>

#include "../Utils/Hash.hpp"

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