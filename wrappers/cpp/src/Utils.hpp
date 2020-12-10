/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
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

inline std::string formatTime(long long timestamp, const std::string& format = "%Y-%m-%d %H:%M:%S") {
const std::size_t TIME_STRING_BUFFER_SIZE = 512;
	std::stringstream ss;
	auto t = static_cast<std::time_t>(timestamp);
	char timeString[TIME_STRING_BUFFER_SIZE];

	std::strftime(timeString, TIME_STRING_BUFFER_SIZE, format.c_str(), gmtime(&t));

	ss << timeString;

	return ss.str();
}

inline std::string formatTimestampWithMillis(long long timestamp) {
	long long ms = timestamp % 1000;
	std::stringstream ss;

	ss << formatTime(timestamp / 1000);

	return fmt::format("{}.{:0>3}", ss.str(), ms);
}