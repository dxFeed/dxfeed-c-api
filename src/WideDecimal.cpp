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

#include "WideDecimal.h"

#include "WideDecimal.hpp"

namespace dx {
const std::string Double::NAN_STRING = "NaN";
const std::string Double::POSITIVE_INFINITY_STRING = "Infinity";
const std::string Double::NEGATIVE_INFINITY_STRING = "-Infinity";
const std::string Double::ZERO_STRING = "0";
const double Double::POSITIVE_INFINITY = std::numeric_limits<dxf_double_t>::infinity();
const double Double::NEGATIVE_INFINITY = -std::numeric_limits<dxf_double_t>::infinity();
const double Double::QUIET_NAN = std::numeric_limits<dxf_double_t>::quiet_NaN();

const dxf_long_t Double::EXP_BIT_MASK = 9218868437227405312LL;
const dxf_long_t Double::SIGNIF_BIT_MASK = 4503599627370495LL;

const dxf_long_t WideDecimal::Consts::NaN;
const dxf_long_t WideDecimal::Consts::POSITIVE_INFINITY;
const dxf_long_t WideDecimal::Consts::NEGATIVE_INFINITY;
const dxf_long_t WideDecimal::Consts::MAX_DOUBLE_SIGNIFICAND;
const dxf_long_t WideDecimal::Consts::MIN_SIGNIFICAND = -36028797018963968LL;
const dxf_long_t WideDecimal::Consts::MAX_SIGNIFICAND = 36028797018963967LL;
const dxf_int_t WideDecimal::Consts::BIAS;
const dxf_long_t WideDecimal::Consts::ZERO;
const std::size_t WideDecimal::Consts::EXACT_LONG_POWERS;
const std::size_t WideDecimal::Consts::EXACT_DOUBLE_POWERS;
const char WideDecimal::Consts::EXPONENT_CHAR;
const std::size_t WideDecimal::Consts::MAX_TRAILING_ZEROES;
const std::size_t WideDecimal::Consts::MAX_LEADING_ZEROES;

const std::array<std::string, 4> WideDecimal::Consts::NF_STRING = {
	{Double::NAN_STRING, Double::POSITIVE_INFINITY_STRING, Double::NAN_STRING, Double::NEGATIVE_INFINITY_STRING}};

const std::array<dxf_double_t, 4> WideDecimal::Consts::NF_DOUBLE = {
	{Double::QUIET_NAN, Double::POSITIVE_INFINITY, Double::QUIET_NAN, Double::NEGATIVE_INFINITY}};

const std::array<dxf_long_t, 4> WideDecimal::Consts::NF_LONG = {
	{0, std::numeric_limits<dxf_long_t>::max(), 0, std::numeric_limits<dxf_long_t>::min()}};

const std::array<dxf_long_t, 4> WideDecimal::Consts::NF_WIDE = {
	{WideDecimal::Consts::NaN, WideDecimal::Consts::POSITIVE_INFINITY, WideDecimal::Consts::NaN,
	 WideDecimal::Consts::NEGATIVE_INFINITY}};

template <>
const RightShift<dxf_long_t>::Consts RightShift<dxf_long_t>::consts{};

template <>
const RightShift<dxf_int_t>::Consts RightShift<dxf_int_t>::consts{};

const WideDecimal::Consts WideDecimal::consts{};
}  // namespace dx

int dx_wide_decimal_long_to_double(dxf_long_t longValue, OUT dxf_double_t* decimal) {
	*decimal = dx::WideDecimal::toDouble(longValue);

	return true;
}

dxf_int_t dx_right_shift_int(dxf_int_t int_value, dxf_int_t shift) {
	return dx::rightShift(int_value, shift);
}