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
	/// Event symbol that identifies this quote
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
	dxf_double_t bid_size{};
	/// Time of the last ask change
	dxf_long_t ask_time{};
	/// Ask exchange code
	dxf_char_t ask_exchange_code{};
	/// Ask price
	dxf_double_t ask_price{};
	/// Ask size
	dxf_double_t ask_size{};
	/**
	 * Scope of this quote.
	 *
	 * Possible values: #dxf_osc_composite (Quote events) , #dxf_osc_regional (Quote& events)
	 */
	dxf_order_scope_t scope{};

	Quote() = default;

	explicit Quote(std::string symbol, const dxf_quote_t &quote)
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
			StringConverter::toString(bid_exchange_code), bid_price, bid_size, formatTimestampWithMillis(ask_time),
			StringConverter::toString(ask_exchange_code), ask_price, ask_size, orderScopeToString(scope));
	}

	template <typename OutStream>
	friend OutStream &operator<<(OutStream &os, const Quote &quote) {
		os << quote.toString();

		return os;
	}
};

struct OrderBase {
	/// Event symbol that identifies this order
	std::string symbol{};
	/// Transactional event flags.
	dxf_event_flags_t event_flags;
	/// Unique per-symbol index of this order.
	dxf_long_t index;
	/// Time of this order. Time is measured in milliseconds between the current time and midnight, January 1, 1970 UTC.
	dxf_long_t time;
	/// Microseconds and nanoseconds part of time of this order.
	dxf_int_t time_nanos;
	/// Sequence number of this order to distinguish orders that have the same #time.
	dxf_int_t sequence;
	/// Price of this order.
	dxf_double_t price;
	/// Size of this order
	dxf_double_t size;
	/// Executed size of this order
	dxf_double_t executed_size;
	/// Number of individual orders in this aggregate order.
	dxf_double_t count;
	/// Scope of this order
	dxf_order_scope_t scope;
	/// Side of this order
	dxf_order_side_t side;
	/// Exchange code of this order
	dxf_char_t exchange_code;
	/// Source of this order
	std::string source;

	explicit OrderBase(std::string symbol, const dxf_order_t &order)
		: symbol{std::move(symbol)},
		  event_flags{order.event_flags},
		  index{order.index},
		  time{order.time},
		  time_nanos{order.time_nanos},
		  sequence{order.sequence},
		  price{order.price},
		  size{order.size},
		  executed_size{order.executed_size},
		  count{order.count},
		  scope{order.scope},
		  side{order.side},
		  exchange_code{order.exchange_code},
		  source{StringConverter::toString(std::begin(order.source), std::end(order.source))} {}

	std::string toString() const {
		return fmt::format(
			"symbol = {}, event_flags = {:#x}, index = {:#x}, time(UTC) = {}, time_nanos = {}, sequence = {}, "
			"price = {}, size = {}, count = {}, scope = {}, side = {}, exchange_code = {}, source = {}",
			symbol, event_flags, index, formatTimestampWithMillis(time), time_nanos, sequence, price, size, count,
			orderScopeToString(scope), orderSideToString(side), StringConverter::toString(exchange_code), source);
	}
};

struct Greeks final {
	std::string symbol{};
	dxf_event_flags_t event_flags{};
	dxf_long_t index{};
	dxf_long_t time{};
	dxf_double_t price{};
	dxf_double_t volatility{};
	dxf_double_t delta{};
	dxf_double_t gamma{};
	dxf_double_t theta{};
	dxf_double_t rho{};
	dxf_double_t vega{};

	Greeks() = default;

	explicit Greeks(std::string symbol, const dxf_greeks_t &greeks)
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
			"Greeks{{symbol = {}, time = {}, index = {}, price = {}, volatility = {}, delta = {}, gamma = {}, "
			"theta = {}, rho = {}, vega = {}, index(hex) = 0x{:x}}}",
			symbol, formatTimestampWithMillis(time), index, price, volatility, delta, gamma, theta, rho, vega, index);
	}

	template <typename OutStream>
	friend OutStream &operator<<(OutStream &os, const Greeks &greeks) {
		os << greeks.toString();

		return os;
	}
};

/**
 * @brief Underlying
 *
 * @details Underlying event is a snapshot of computed values that are available for an option underlying symbol based
 * on the option prices on the market. It represents the most recent information that is available about the
 * corresponding values on the market at any given moment of time.
 */
struct Underlying final {
	/// Event symbol that identifies this underlying
	std::string symbol{};
	/// 30-day implied volatility for this underlying based on VIX methodology
	dxf_double_t volatility{};
	/// Front month implied volatility for this underlying based on VIX methodology;
	dxf_double_t front_volatility{};
	/// Back month implied volatility for this underlying based on VIX methodology
	dxf_double_t back_volatility{};
	/// Call options traded volume for a day
	dxf_double_t call_volume{};
	/// Put options traded volume for a day
	dxf_double_t put_volume{};
	/// Options traded volume for a day
	dxf_double_t option_volume{};
	/// Ratio of put options traded volume to call options traded volume for a day
	dxf_double_t put_call_ratio{};

	Underlying() = default;

	explicit Underlying(std::string symbol, const dxf_underlying_t &underlying)
		: symbol{std::move(symbol)},
		  volatility{underlying.volatility},
		  front_volatility{underlying.front_volatility},
		  back_volatility{underlying.back_volatility},
		  call_volume{underlying.call_volume},
		  put_volume{underlying.put_volume},
		  option_volume{underlying.option_volume},
		  put_call_ratio{underlying.put_call_ratio} {}

	std::string toString() const {
		return fmt::format(
			"Underlying{{symbol = {}, volatility = {}, front_volatility = {}, back_volatility = {}, "
			"call_volume = {}, put_volume {}, option_volume = {}, put_call_ratio = {}}}",
			symbol, volatility, front_volatility, back_volatility, call_volume, put_volume, option_volume,
			put_call_ratio);
	}

	template <typename OutStream>
	friend OutStream &operator<<(OutStream &os, const Underlying &underlying) {
		os << underlying.toString();

		return os;
	}
};
