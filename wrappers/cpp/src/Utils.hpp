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