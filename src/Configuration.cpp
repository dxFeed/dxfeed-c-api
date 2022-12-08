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

#include "Configuration.h"
#include "DXErrorCodes.h"

}

#include "StringConverter.hpp"
#include "Configuration.hpp"

int dx_load_config_from_wstring(dxf_const_string_t config) {
	return dx::Configuration::getInstance()->loadFromString(dx::StringConverter::wStringToUtf8(config));
}

int dx_load_config_from_string(const char* config) { return dx::Configuration::getInstance()->loadFromString(config); }

int dx_load_config_from_file(const char* file_name) {
	return dx::Configuration::getInstance()->loadFromFile(file_name);
}

int dx_get_network_heartbeat_period(int default_heartbeat_period) {
	return dx::Configuration::getInstance()->getNetworkHeartbeatPeriod(default_heartbeat_period);
}

int dx_get_network_heartbeat_timeout(int default_heartbeat_timeout) {
	return dx::Configuration::getInstance()->getNetworkHeartbeatTimeout(default_heartbeat_timeout);
}

dx_log_level_t dx_get_minimum_logging_level(dx_log_level_t default_minimum_logging_level) {
	return dx::Configuration::getInstance()->getMinimumLoggingLevel(default_minimum_logging_level);
}

int dx_get_network_reestablish_connections() {
	return dx::Configuration::getInstance()->getNetworkReestablishConnections();
}

int dx_get_subscriptions_disable_last_event_storage() {
	return dx::Configuration::getInstance()->getSubscriptionsDisableLastEventStorage();
}
