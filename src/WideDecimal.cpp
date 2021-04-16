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

#include <limits>
#include <type_traits>

extern "C" {

#include "BufferedInput.h"
#include "BufferedOutput.h"
#include "Connection.h"
#include "ConnectionContextData.h"
#include "DXErrorCodes.h"
#include "DXErrorHandling.h"
#include "DXNetwork.h"
}

#include "WideDecimal.h"

namespace dx {
template <typename T>
inline constexpr int signum(T x, std::false_type is_signed) {
	return T(0) < x;
}

template <typename T>
inline constexpr int signum(T x, std::true_type is_signed) {
	return (T(0) < x) - (x < T(0));
}

template <typename T>
inline constexpr int signum(T x) {
	return signum(x, std::is_signed<T>());
}

namespace WideDecimal {
static const dxf_long_t NaN = 0;
static const dxf_long_t POSITIVE_INFINITY = 0x100;
static const dxf_long_t NEGATIVE_INFINITY = -0x100;

static const dxf_long_t MAX_DOUBLE_SIGNIFICAND = (1LL << 53) - 1;
static const dxf_int_t BIAS = 128;

static const dxf_long_t ZERO = BIAS;			  // canonical zero with zero scale
static const dxf_int_t EXACT_DOUBLE_POWERS = 22;  // max power of 10 that fit into double exactly

// MULTIPLIERS and DIVISORS are indexed by [rank] to compliment toDouble computation
// [rank]        [0]        [1]     ...  [127]   [128]   [129]  ...  [255]
// MULTIPLIERS   infinity   1e127        1e1     1e0     1e-1        1e-127
// DIVISORS      0          1e-127       1e-1    1e0     1e1         1e127
static const dxf_double_t MULTIPLIERS[256] = {};  // TODO: FILL
static const dxf_double_t DIVISORS[256] = {};	  // TODO: FILL

static const dxf_double_t NF_DOUBLE[] = {std::numeric_limits<dxf_double_t>::quiet_NaN(),
										 std::numeric_limits<dxf_double_t>::infinity(),
										 -1.0 * std::numeric_limits<dxf_double_t>::infinity()};
// TODO: FIX shifts
inline static dxf_double_t toDouble(dxf_long_t wide) {
	dxf_long_t significand = wide >> 8;
	int rank = (int)wide & 0xFF;
	// if both significand and rank coefficient can be exactly represented as double values -
	// perform a single double multiply or divide to compute properly rounded result
	if (significand <= MAX_DOUBLE_SIGNIFICAND && -significand <= MAX_DOUBLE_SIGNIFICAND) {
		if (rank > BIAS) {
			if (rank <= BIAS + EXACT_DOUBLE_POWERS) return significand / DIVISORS[rank];
		} else if (rank == BIAS) {
			return significand;
		} else {
			if (rank >= BIAS - EXACT_DOUBLE_POWERS) return significand * MULTIPLIERS[rank];
			if (rank == 0) return NF_DOUBLE[signum(significand) & 3];
		}
	}

	// TODO: convert by toString()

	return 0.0;
}
}  // namespace WideDecimal
}  // namespace dx

int dx_wide_decimal_int_to_double(dxf_int_t integer, OUT dxf_double_t* decimal) {
	*decimal = dx::WideDecimal::toDouble(integer);
}