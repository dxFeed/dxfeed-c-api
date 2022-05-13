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

#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Configuration.hpp"

namespace dx {

using LoggerPtr = std::shared_ptr<spdlog::logger>;

struct LoggerFactory {
	static constexpr const char* NULL_LOGGER_NAME = "Null";
	static constexpr const char* DXF_LOGGER_NAME = "dxf::Logger";

	static spdlog::level::level_enum dxLogLevelToLevel(dx_log_level_t level) {
		const std::unordered_map<dx_log_level_t, spdlog::level::level_enum> converter = {
			{dx_ll_trace, spdlog::level::trace}, {dx_ll_debug, spdlog::level::debug},
			{dx_ll_info, spdlog::level::info},	 {dx_ll_warn, spdlog::level::warn},
			{dx_ll_error, spdlog::level::err},	 {dx_ll_critical, spdlog::level::critical},
			{dx_ll_off, spdlog::level::off}};

		auto found = converter.find(level);

		if (found != converter.end()) {
			return found->second;
		}

		return spdlog::level::info;
	}

	static LoggerPtr getLogger(bool test = false) {
		static bool initialized = false;

		if (initialized) {
			return spdlog::get(DXF_LOGGER_NAME);
		}

		auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

		spdlog::create<spdlog::sinks::null_sink_mt>(NULL_LOGGER_NAME);
		std::shared_ptr<spdlog::logger> logger;

		std::vector<spdlog::sink_ptr> sinks;
		sinks.push_back(consoleSink);

		if (test) {
			logger = std::make_shared<spdlog::logger>(DXF_LOGGER_NAME, sinks.begin(), sinks.end());
			logger->set_level(spdlog::level::debug);
		} else {
			auto cfg = dx::Configuration::getInstance();

			if (!cfg->isLoaded()) {
				cfg->load();
			}

			if (cfg->isLoaded()) {
				auto fileName = cfg->getLoggerFileName();
				auto maxFileSize = cfg->getLoggerRotatingMaxFileSize();
				auto maxFiles = cfg->getLoggerRotatingMaxFiles();
				auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(fileName, maxFileSize, maxFiles);

				sinks.push_back(fileSink);
				logger = std::make_shared<spdlog::logger>(DXF_LOGGER_NAME, sinks.begin(), sinks.end());

				logger->set_level(dxLogLevelToLevel(cfg->getMinimumLoggingLevel()));
			} else {
				logger = std::make_shared<spdlog::logger>(DXF_LOGGER_NAME, sinks.begin(), sinks.end());
				logger->set_level(spdlog::level::info);
			}
		}

		logger->set_pattern("%L %Y-%m-%d %H:%M:%S.%f TID:%t %v");
		spdlog::register_logger(logger);
		spdlog::flush_on(spdlog::level::info);

		initialized = true;

		return logger;
	}
};

}  // namespace dx