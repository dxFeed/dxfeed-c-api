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

#include <EventData.h>
#include <fmt/format.h>

#include <string>
#include <utility>

#include "StringConverter.hpp"
#include "Utils.hpp"

/**
 * @brief Quote.
 *
 * @details Quote event is a snapshot of the best bid and ask prices, and other fields that change with each quote. It
 * represents the most recent information that is available about the best quote on the market at any given moment of
 * time.
 */
struct Quote final {
	std::string symbol{};
	/// Time of the last bid or ask change
	dxf_long_t time{};
	/// Sequence number of this quote to distinguish quotes that have the same #time.
	dxf_int_t sequence{};
	/// Microseconds and nanoseconds part of time of the last bid or ask change
	dxf_int_t time_nanos{};
	/// Time of the last bid change
	dxf_long_t bid_time{};
	/// Bid exchange code
	dxf_char_t bid_exchange_code{};
	/// Bid price
	dxf_double_t bid_price{};
	/// Bid size
	dxf_int_t bid_size{};
	/// Time of the last ask change
	dxf_long_t ask_time{};
	/// Ask exchange code
	dxf_char_t ask_exchange_code{};
	/// Ask price
	dxf_double_t ask_price{};
	/// Ask size
	dxf_int_t ask_size{};
	/**
	 * Scope of this quote.
	 *
	 * Possible values: #dxf_osc_composite (Quote events) , #dxf_osc_regional (Quote& events)
	 */
	dxf_order_scope_t scope{};

	explicit Quote(std::string symbol, dxf_quote_t quote)
		: symbol{std::move(symbol)},
		  time{quote.time},
		  sequence{quote.sequence},
		  time_nanos{quote.time_nanos},
		  bid_time{quote.bid_time},
		  bid_exchange_code{quote.bid_exchange_code},
		  bid_price{quote.bid_price},
		  bid_size{quote.bid_size},
		  ask_time{quote.ask_time},
		  ask_exchange_code{quote.ask_exchange_code},
		  ask_price{quote.ask_price},
		  ask_size{quote.ask_size},
		  scope{quote.scope} {}

	std::string toString() const {
		return fmt::format(
			"Quote{{symbol = {}, time(UTC) = {}, sequence = {}, time_nanos = {}, "
			"bid_time(UTC) = {}, bid_exchange_code = {}, bid_price = {}, bid_size = {}, "
			"ask_time(UTC) = {}, ask_exchange_code = {}, ask_price = {}, ask_size = {}, scope = {}}}",
			symbol, formatTimestampWithMillis(time), sequence, time_nanos, formatTimestampWithMillis(bid_time),
			StringConverter::toString(std::wstring() + bid_exchange_code), bid_price, bid_size,
			formatTimestampWithMillis(ask_time), StringConverter::toString(std::wstring() + ask_exchange_code),
			ask_price, ask_size, scope);
	}

	template <typename OutStream>
	friend OutStream &operator<<(OutStream &os, const Quote &quote) {
		os << quote.toString();

		return os;
	}
};

struct Greeks final {
	std::string symbol;
	dxf_event_flags_t event_flags;
	dxf_long_t index;
	dxf_long_t time;
	dxf_double_t price;
	dxf_double_t volatility;
	dxf_double_t delta;
	dxf_double_t gamma;
	dxf_double_t theta;
	dxf_double_t rho;
	dxf_double_t vega;

	Greeks() = default;

	explicit Greeks(std::string symbol, dxf_greeks_t greeks)
		: symbol{std::move(symbol)},
		  event_flags{greeks.event_flags},
		  index{greeks.index},
		  time{greeks.time},
		  price{greeks.price},
		  volatility{greeks.volatility},
		  delta{greeks.delta},
		  gamma{greeks.gamma},
		  theta{greeks.theta},
		  rho{greeks.rho},
		  vega{greeks.vega} {}

	std::string toString() const {
		return fmt::format(
			"Greeks{{symbol = {}, time = {}, index = {}, price = {}, delta = {}, gamma = {}, rho = {}, vega = {}, "
			"index(hex) = 0x{:x}}}",
			symbol, formatTimestampWithMillis(time), index, price, delta, gamma, rho, vega, index);
	}

	template <typename OutStream>
	friend OutStream &operator<<(OutStream &os, const Greeks &greeks) {
		os << greeks.toString();

		return os;
	}
};

struct Underlying final {
	std::string symbol;
	dxf_double_t volatility;
	dxf_double_t front_volatility;
	dxf_double_t back_volatility;
	dxf_double_t put_call_ratio;

	Underlying() = default;

	explicit Underlying(std::string symbol, dxf_underlying_t underlying)
		: symbol{std::move(symbol)},
		  volatility{underlying.volatility},
		  front_volatility{underlying.front_volatility},
		  back_volatility{underlying.back_volatility},
		  put_call_ratio{underlying.put_call_ratio} {}

	std::string toString() const {
		return fmt::format(
			"Underlying{{symbol = {}, volatility = {}, front_volatility = {}, back_volatility = {}, put_call_ratio = "
			"{}}}",
			symbol, volatility, front_volatility, back_volatility, put_call_ratio);
	}

	template <typename OutStream>
	friend OutStream &operator<<(OutStream &os, const Underlying &underlying) {
		os << underlying.toString();

		return os;
	}
};

