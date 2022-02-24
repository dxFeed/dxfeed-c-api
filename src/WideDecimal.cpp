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

#include <array>
#include <cassert>
#include <iostream>
#include <limits>
#include <string>
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

struct Double {
	static const std::string NAN_STRING;
	static const std::string POSITIVE_INFINITY_STRING;
	static const std::string NEGATIVE_INFINITY_STRING;
	static const std::string ZERO_STRING;

	static const double POSITIVE_INFINITY;
	static const double NEGATIVE_INFINITY;
	static const double QUIET_NAN;
	double value;

	static Double parseDouble(const std::string& str) { return {std::stod(str)}; }

	operator double() const {  // NOLINT(google-explicit-constructor)
		return value;
	}
};

const std::string Double::NAN_STRING = "NaN";
const std::string Double::POSITIVE_INFINITY_STRING = "Infinity";
const std::string Double::NEGATIVE_INFINITY_STRING = "-Infinity";
const std::string Double::ZERO_STRING = "0";

const double Double::POSITIVE_INFINITY = std::numeric_limits<dxf_double_t>::infinity();
const double Double::NEGATIVE_INFINITY = -std::numeric_limits<dxf_double_t>::infinity();
const double Double::QUIET_NAN = std::numeric_limits<dxf_double_t>::quiet_NaN();

struct WideDecimal {
	struct Consts {
		static const dxf_long_t NaN = 0;
		static const dxf_long_t POSITIVE_INFINITY = 0x100;
		static const dxf_long_t NEGATIVE_INFINITY = -0x100;

		static const dxf_long_t MAX_DOUBLE_SIGNIFICAND = (1LL << 53) - 1;
		static const dxf_int_t BIAS = 128;

		static const dxf_long_t ZERO = BIAS;							 // canonical zero with zero scale
		static const std::size_t EXACT_LONG_POWERS = std::size_t{18};	 // max power of 10 that fit into long exactly
		static const std::size_t EXACT_DOUBLE_POWERS = std::size_t{22};	 // max power of 10 that fit into double exactly
		static const char EXPONENT_CHAR = 'E';
		static const std::size_t MAX_TRAILING_ZEROES = std::size_t{6};	// valid range: [0, 127]
		static const std::size_t MAX_LEADING_ZEROES = std::size_t{6};	// valid range: [0, 143]

		std::size_t maxStringLength =
			18 + (std::max)(std::size_t{6}, (std::max)(std::size_t{2} + MAX_LEADING_ZEROES, MAX_TRAILING_ZEROES));
		dxf_long_t scientificModulo;  // longPowers[MAX_TRAILING_ZEROES + 1] or std::numeric_limits<dxf_long_t>::max()
		std::string zeroChars;		  // aka "0.000000(0)"

		std::array<std::string, 4> nfString = {{Double::NAN_STRING, Double::POSITIVE_INFINITY_STRING, Double::NAN_STRING,
											   Double::NEGATIVE_INFINITY_STRING}};
		std::array<dxf_double_t, 4> nfDouble = {{Double::QUIET_NAN, Double::POSITIVE_INFINITY, Double::QUIET_NAN,
												Double::NEGATIVE_INFINITY}};

		std::array<dxf_long_t, EXACT_LONG_POWERS + 1> longPowers;
		// LONG_MULTIPLIERS and LONG_DIVISORS are indexed by [rank] to compliment toLong computation
		// They mimic MULTIPLIERS and DIVISORS but are defined only for EXACT_LONG_POWERS
		std::array<dxf_long_t, 256> longMultipliers;
		std::array<dxf_long_t, 256> longDivisors;

		// MULTIPLIERS and DIVISORS are indexed by [rank] to compliment toDouble computation
		// [rank]        [0]        [1]     ...  [127]   [128]   [129]  ...  [255]
		// MULTIPLIERS   infinity   1e127        1e1     1e0     1e-1        1e-127
		// DIVISORS      0          1e-127       1e-1    1e0     1e1         1e127
		std::array<dxf_double_t, 256> multipliers;
		std::array<dxf_double_t, 256> divisors;

		Consts() : multipliers{}, divisors{} {
			dxf_long_t pow10 = 1;

			for (std::size_t i = 0; i <= EXACT_LONG_POWERS; i++) {
				longPowers[i] = pow10;
				longMultipliers[BIAS - i] = pow10;
				longDivisors[BIAS + i] = pow10;
				pow10 *= 10;
			}

			scientificModulo = (MAX_TRAILING_ZEROES + 1 <= EXACT_LONG_POWERS)
				? longPowers[MAX_TRAILING_ZEROES + 1]
				: (std::numeric_limits<dxf_long_t>::max)();
			zeroChars = std::string(130, '0');
			zeroChars[1] = '.';

			multipliers[0] = Double::POSITIVE_INFINITY;

			for (std::size_t rank = 1; rank < multipliers.size(); rank++) {
				multipliers[rank] = Double::parseDouble("1E" + std::to_string(BIAS - static_cast<dxf_int_t>(rank)));
			}

			divisors[0] = 0.0;

			for (std::size_t rank = 1; rank < divisors.size(); rank++) {
				divisors[rank] = Double::parseDouble("1E" + std::to_string(static_cast<dxf_int_t>(rank) - BIAS));
			}
		}
	};

	static const Consts consts;

	inline static dxf_long_t toIntegerString(dxf_long_t significand, int rank) {
		assert(significand != 0 && rank != 0);

		if (rank > Consts::BIAS) {
			// fractional number with possible zero fraction and more trailing zeroes
			if (rank - Consts::BIAS <= Consts::EXACT_LONG_POWERS) {
				dxf_long_t pow10 = consts.longPowers[rank - Consts::BIAS];
				dxf_long_t result = significand / pow10;

				if (result * pow10 == significand && result % consts.scientificModulo != 0) {
					return result;
				}
			}
		} else {
			// integer number with possible trailing zeroes
			if (Consts::BIAS - rank <= Consts::EXACT_LONG_POWERS &&
				Consts::BIAS - rank <= Consts::MAX_TRAILING_ZEROES) {
				dxf_long_t pow10 = consts.longPowers[Consts::BIAS - rank];
				dxf_long_t result = significand * pow10;

				if (result / pow10 == significand && result % consts.scientificModulo != 0) {
					return result;
				}
			}
		}

		return 0;
	}

	inline static std::string toFractionalOrScientific(dxf_long_t significand, int rank) {
		assert(significand != 0 && rank != 0);

		// remove trailing zeroes
		while (significand % 10 == 0) {
			significand /= 10;
			rank--;
		}
		std::string result{};

		// append significand and note position of first digit (after potential '-' sign)
		auto firstDigit = static_cast<dxf_long_t>(significand < 0);
		result += std::to_string(significand);

		// use plain decimal number notation unless scientific notation is triggered
		if (rank > Consts::BIAS) {
			// fractional number
			dxf_long_t dotPosition = static_cast<dxf_long_t>(result.length()) - (rank - Consts::BIAS);

			if (dotPosition > firstDigit) {
				return result.insert(dotPosition, ".");
			}

			if (firstDigit - dotPosition <= Consts::MAX_LEADING_ZEROES) {
				return result.insert(firstDigit, consts.zeroChars, 0, 2 + (firstDigit - dotPosition));
			}
		} else {
			// integer number
			if (Consts::BIAS - rank <= Consts::MAX_TRAILING_ZEROES) {
				return result.append(consts.zeroChars, 2, Consts::BIAS - rank);
			}
		}
		// use scientific notation
		dxf_long_t digits = static_cast<dxf_long_t>(result.length()) - firstDigit;

		if (digits != 1) {
			result.insert(firstDigit + 1, ".");
		}

		result += Consts::EXPONENT_CHAR;

		return result.append(std::to_string(Consts::BIAS - rank + digits - 1));
	}

	inline static std::string toString(dxf_long_t wide) {
		dxf_long_t significand = wide / 256;
		dxf_int_t rank = static_cast<dxf_int_t>(wide) & 0xFF;

		if (rank == 0) {
			return consts.nfString[signum(significand) & 3];
		}

		if (significand == 0) {
			return Double::ZERO_STRING;
		}

		// if value is an integer number - use faster formatting method
		dxf_long_t integerString = toIntegerString(significand, rank);

		if (integerString != 0) {
			return std::to_string(integerString);
		}

		auto result = toFractionalOrScientific(significand, rank);

		if (result.length() > consts.maxStringLength) {
			result = result.substr(0, consts.maxStringLength);
		}

		return result;
	}

	inline static dxf_double_t toDouble(dxf_long_t wide) {
		dxf_long_t significand = wide / 256;
		dxf_int_t rank = static_cast<dxf_int_t>(wide) & 0xFF;

		// if both significand and rank coefficient can be exactly represented as double values -
		// perform a single double multiply or divide to compute properly rounded result
		if (significand <= Consts::MAX_DOUBLE_SIGNIFICAND && -significand <= Consts::MAX_DOUBLE_SIGNIFICAND) {
			if (rank > Consts::BIAS) {
				if (rank <= Consts::BIAS + Consts::EXACT_DOUBLE_POWERS) {
					return static_cast<dxf_double_t>(significand) / consts.divisors[rank];
				}
			} else if (rank == Consts::BIAS) {
				return static_cast<dxf_double_t>(significand);
			} else {
				if (rank >= Consts::BIAS - Consts::EXACT_DOUBLE_POWERS) {
					return static_cast<dxf_double_t>(significand) * consts.multipliers[rank];
				}

				if (rank == 0) {
					return consts.nfDouble[signum(significand) & 3];
				}
			}
		}

		// some component can't be exactly represented as double value -
		// extended precision computations with proper rounding are required;
		// avoid them with somewhat inefficient but guaranteed to work approach
		return Double::parseDouble(toString(wide));
	}
};

const dxf_long_t WideDecimal::Consts::NaN;
const dxf_long_t WideDecimal::Consts::POSITIVE_INFINITY;
const dxf_long_t WideDecimal::Consts::NEGATIVE_INFINITY;
const dxf_long_t WideDecimal::Consts::MAX_DOUBLE_SIGNIFICAND;
const dxf_int_t WideDecimal::Consts::BIAS;
const dxf_long_t WideDecimal::Consts::ZERO;
const std::size_t WideDecimal::Consts::EXACT_LONG_POWERS;
const std::size_t WideDecimal::Consts::EXACT_DOUBLE_POWERS;
const char WideDecimal::Consts::EXPONENT_CHAR;
const std::size_t WideDecimal::Consts::MAX_TRAILING_ZEROES;
const std::size_t WideDecimal::Consts::MAX_LEADING_ZEROES;

const WideDecimal::Consts WideDecimal::consts = WideDecimal::Consts();
}  // namespace dx

int dx_wide_decimal_long_to_double(dxf_long_t longValue, OUT dxf_double_t* decimal) {
	*decimal = dx::WideDecimal::toDouble(longValue);

	return true;
}
