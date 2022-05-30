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

extern "C" {
#include "Snapshot.h"
}

#include "../StringConverter.hpp"
#include "Snapshot.hpp"

extern "C" {
dxf_snapshot_v2_t dx_create_snapshot_v2(dxf_connection_t connection, dx_event_id_t event_id, dxf_const_string_t symbol,
										const char* source, dxf_long_t time, void* userData) {
	auto m = dx::SnapshotManager::getInstance(connection);

	// IndexedEvent
	if (dx_eid_order == event_id || dx_eid_spread_order == event_id || dx_eid_series == event_id) {
		if (time > 0) {
			return dx::INVALID_SNAPSHOT_REFERENCE_ID;
		}

		auto s = m->create<dx::IndexedEventsSnapshot>(
			connection,
			dx::SnapshotKey(event_id, dx::StringConverter::wStringToUtf8(symbol), std::string(source), nonstd::nullopt),
			userData);

		return s.second;
	} else if (dx_eid_candle == event_id || dx_eid_greeks == event_id || dx_eid_theo_price == event_id ||
			   dx_eid_time_and_sale == event_id || dx_eid_underlying == event_id) {	 // TODO: TimeSeriesEvent
		return dx::INVALID_SNAPSHOT_REFERENCE_ID;
	}

	return dx::INVALID_SNAPSHOT_REFERENCE_ID;
}

int dx_set_snapshot_listeners_v2(dxf_connection_t connection, dxf_snapshot_v2_t snapshot, dxf_snapshot_listener_v2_t on_new_snapshot_listener,
								 dxf_snapshot_listener_v2_t on_snapshot_update_listener,
								 dxf_snapshot_inc_listener_v2_t on_incremental_change_listener) {

	return true;
}

void dx_close_snapshot_v2(dxf_connection_t connection, dxf_snapshot_v2_t snapshot) {
	dx::SnapshotManager::getInstance(connection)->close(snapshot);
}

}
