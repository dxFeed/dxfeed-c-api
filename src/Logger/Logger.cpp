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

#include "Logger.hpp"

#include <cwchar>
#include <cstdarg>

#include "../StringConverter.hpp"

extern "C" {

#include "LoggerNG.h"

void dxf_logs(dx_log_level_t log_level, dxf_const_string_t string) {
	auto level = dx::LoggerFactory::dxLogLevelToLevel(log_level);

	dx::LoggerFactory::getLogger()->log(level, dx::StringConverter::wStringToUtf8(string));
}

void dxf_log(dx_log_level_t log_level, dxf_const_string_t format, ...) {
	auto level = dx::LoggerFactory::dxLogLevelToLevel(log_level);

	wchar_t buffer[2048] = {0};

	std::va_list ap;
	va_start(ap, format);
	std::vswprintf(buffer, sizeof buffer / sizeof *buffer, format, ap);
	va_end(ap);

	dx::LoggerFactory::getLogger()->log(level, dx::StringConverter::wStringToUtf8(buffer));
}

dxf_const_string_t dxf_event_id_to_wstring(dx_event_id_t event_id) {
	switch (event_id) {
		case dx_eid_trade:
			return L"Trade";
		case dx_eid_quote:
			return L"Quote";
		case dx_eid_summary:
			return L"Summary";
		case dx_eid_profile:
			return L"Profile";
		case dx_eid_order:
			return L"Order";
		case dx_eid_time_and_sale:
			return L"Time&Sale";
		case dx_eid_candle:
			return L"Candle";
		case dx_eid_trade_eth:
			return L"TradeETH";
		case dx_eid_spread_order:
			return L"SpreadOrder";
		case dx_eid_greeks:
			return L"Greeks";
		case dx_eid_theo_price:
			return L"TheoPrice";
		case dx_eid_underlying:
			return L"Underlying";
		case dx_eid_series:
			return L"Series";
		case dx_eid_configuration:
			return L"Configuration";
		default:
			return L"";
	}
}
}