#pragma once

#include <sstream>
#include <string>
#include <ctime>
#include <iomanip>
#include <fmt/format.h>

inline std::string formatTime(long long timestamp, const std::string& format = "%Y-%m-%d %H:%M:%S") {
	std::stringstream ss;

	auto t = static_cast<std::time_t>(timestamp);

	ss << std::put_time(std::gmtime(&t), format.c_str());

	return ss.str();
}

inline std::string formatTimestampWithMillis(long long timestamp) {
	long long shortTimestamp = timestamp / 1000;
	long long ms = timestamp % 1000;

	std::stringstream ss;
	auto t = static_cast<std::time_t>(shortTimestamp);

	ss << std::put_time(std::gmtime(&t), "%Y-%m-%d %H:%M:%S");

	return fmt::format("{}.{:0>3}", ss.str(), ms);
}