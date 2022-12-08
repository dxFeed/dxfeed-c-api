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

#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <toml.hpp>

#include "StringUtils.hpp"

extern "C" {
#include "DXErrorCodes.h"
#include "DXErrorHandling.h"
#include "Logger.h"
}

namespace dx {

struct Configuration : std::enable_shared_from_this<Configuration> {
	enum class Type { None, String, File };

private:
	Type type_;
	std::string config_;
	mutable std::recursive_mutex mutex_;
	bool loaded_ = false;
	toml::value properties_;

	Configuration() : type_{Type::None}, config_{}, mutex_{}, properties_{} {}

public:
	static std::shared_ptr<Configuration> getInstance() {
		static std::shared_ptr<Configuration> instance{new Configuration()};

		return instance;
	}

	bool isLoaded() const {
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		return loaded_;
	}

	void dump() {
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		std::cerr << "\nC-API Configuration:\n" << toml::format(properties_, 120) << std::endl;
		std::cerr << "Loaded defaults:\n";
		std::cerr << "dump = " << std::boolalpha << getDump() << std::endl;
		std::cerr << "network.heartbeatPeriod = " << getNetworkHeartbeatPeriod() << std::endl;
		std::cerr << "network.heartbeatTimeout = " << getNetworkHeartbeatTimeout() << std::endl;
		std::cerr << "network.reestablishConnections = " << getNetworkReestablishConnections() << std::endl
				  << std::endl;
	}

	bool loadFromFile(const std::string& fileName) {
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		if (loaded_) {
			return true;
		}

		if (StringUtils::trimCopy(fileName).empty()) {
			dx_set_error_code(dx_cfgec_empty_config_file_name);

			return false;
		}

		try {
			properties_ = toml::parse(fileName);
			loaded_ = true;
			type_ = Type::File;
			config_ = fileName;

			if (getDump()) {
				dump();
			}

			return true;
		} catch (const std::exception& e) {
			dx_set_error_code(dx_cfgec_toml_parser_error);
			dx_logging_verbose(dx_ll_warn, L"Configuration::loadFromFile: %hs", e.what());
			loaded_ = false;
		}

		return false;
	}

	bool loadFromString(const std::string& config) {
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		if (loaded_) {
			return true;
		}

		if (StringUtils::trimCopy(config).empty()) {
			dx_set_error_code(dx_cfgec_empty_config_string);

			return false;
		}

		try {
			std::istringstream iss(config);

			properties_ = toml::parse(iss);
			loaded_ = true;
			type_ = Type::String;
			config_ = config;

			if (getDump()) {
				dump();
			}

			return true;
		} catch (const std::exception& e) {
			dx_set_error_code(dx_cfgec_toml_parser_error);
			dx_logging_verbose(dx_ll_warn, L"Configuration::loadFromString: %hs", e.what());
			loaded_ = false;
		}

		return false;
	}

	template <typename T>
	T getProperty(const std::string& groupName, const std::string& fieldName, T defaultValue) const {
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		if (!loaded_) {
			return defaultValue;
		}

		try {
			return toml::find_or((groupName.empty()) ? properties_ : toml::find(properties_, groupName), fieldName,
								 defaultValue);
		} catch (const std::exception&) {
			return defaultValue;
		}
	}

	int getNetworkHeartbeatPeriod(int defaultValue = 10) const {
		return getProperty("network", "heartbeatPeriod", defaultValue);
	}

	int getNetworkHeartbeatTimeout(int defaultValue = 120) const {
		return getProperty("network", "heartbeatTimeout", defaultValue);
	}

	bool getDump(bool defaultValue = false) const { return getProperty("", "dump", defaultValue); }

	dx_log_level_t getMinimumLoggingLevel(dx_log_level_t defaultValue = dx_ll_info) const {
		return StringUtils::stringToLoggingLevel<dx_log_level_t>(getProperty("logger", "level", StringUtils::loggingLevelToString(defaultValue)));
	}

	bool getNetworkReestablishConnections(bool defaultValue = true) const {
		return getProperty("network", "reestablishConnections", defaultValue);
	}

	bool getSubscriptionsDisableLastEventStorage(bool defaultValue = true) const {
		return getProperty("subscriptions", "disableLastEventStorage", defaultValue);
	}
};
}  // namespace dx
