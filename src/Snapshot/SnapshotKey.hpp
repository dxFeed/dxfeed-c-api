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
#include <EventData.h>
}

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "../Utils/Hash.hpp"

namespace dx {

/**
 *`The class responsible for identifying snapshots. Also used for filtering.
 * Not thread safe.
 */
class SnapshotKey {
	dx_event_id_t eventId_;
	std::string symbol_;
	nonstd::optional<std::string> source_;
	nonstd::optional<std::int64_t> fromTime_;

public:
	/**
	 * The default constructor. Creates a snapshot key with non valid state (Event id = -1, empty symbol)
	 */
	SnapshotKey() : eventId_(static_cast<dx_event_id_t>(-1)), symbol_(), source_(), fromTime_() {}

	/**
	 * Creates an instance of a snapshot key.
	 * @param eventId The event id
	 * @param symbol The instrument symbol (AAPL, IBM, etc.)
	 * @param source The source of events (optional, only for IndexedEvent that are not TimeSeriesEvent))
	 * @param fromTime The time from which the snapshot is requested (optional, only for TimeSeriesEvent)
	 */
	SnapshotKey(dx_event_id_t eventId, std::string symbol, nonstd::optional<std::string> source,
				nonstd::optional<std::int64_t> fromTime)
		: eventId_(eventId), symbol_(std::move(symbol)), source_(std::move(source)), fromTime_(std::move(fromTime)) {}

	/**
	 * The comparison operator
	 * @param other The other snapshot key
	 * @return true if keys match.
	 */
	bool operator==(const SnapshotKey& other) const {
		return eventId_ == other.eventId_ && symbol_ == other.symbol_ && source_ == other.source_ &&
			fromTime_ == other.fromTime_;
	}

	/**
	 * @return The event ID of the current key.
	 */
	dx_event_id getEventId() const { return eventId_; }

	/**
	 * @return The symbol of the current key.
	 */
	const std::string& getSymbol() const { return symbol_; }

	/**
	 * @return The source of the current key.
	 */
	const nonstd::optional<std::string>& getSource() const { return source_; }

	/**
	 * @return The time from which the snapshot is requested.
	 */
	const nonstd::optional<std::int64_t>& getFromTime() const { return fromTime_; }
};

}  // namespace dx

namespace std {
template <>
struct hash<dx::SnapshotKey> {
	std::size_t operator()(const dx::SnapshotKey& key) const noexcept {
		/* static_cast<std::size_t>(key.getEventId()) -- Workaround for old
		 * compilers "error: invalid use of incomplete type 'struct std::hash<dx_event_id>'" bug
		 */
		return dx::hashCombine(static_cast<std::size_t>(key.getEventId()), key.getSymbol(), key.getSource(),
							   key.getFromTime());
	}
};
}  // namespace std