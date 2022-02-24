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

#include <sstream>
#include <string>
#include <ctime>
#include <iomanip>
#include <fmt/format.h>
#include <fmt/chrono.h>

inline std::string formatTime(long long timestamp, const std::string& format = "%Y-%m-%d %H:%M:%S") {
const std::size_t TIME_STRING_BUFFER_SIZE = 512;
	return fmt::format(fmt::format("{{:{}}}", format), fmt::gmtime(static_cast<std::time_t>(timestamp)));
}

inline std::string formatTimestampWithMillis(long long timestamp) {
	long long ms = timestamp % 1000;

	return fmt::format("{}.{:0>3}", formatTime(timestamp / 1000), ms);
}

inline std::string orderScopeToString(dxf_order_scope_t scope) {
	switch (scope) {
		case dxf_osc_composite:
			return "Composite";
		case dxf_osc_regional:
			return "Regional";
		case dxf_osc_aggregate:
			return "Aggregate";
		case dxf_osc_order:
			return "Order";
	}

	return "";
}

inline std::string orderSideToString(dxf_order_side_t side) {
	switch (side) {
		case dxf_osd_undefined:
			return "Undefined";
		case dxf_osd_buy:
			return "Buy";
		case dxf_osd_sell:
			return "Sell";
	}

	return "";
}
