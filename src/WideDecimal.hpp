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

#include <array>
#include <cassert>
#include <limits>
#include <string>
#include <type_traits>
#include <cstring>

extern "C" {

#include "DXTypes.h"
#include "PrimitiveTypes.h"

}

namespace dx {

template <typename T>
struct RightShift {
	using U = typename std::make_unsigned<T>::type;

	struct Consts {
		struct SignificantBits {
			std::array<U, sizeof(U) * 8> data;

			SignificantBits() : data{} {
				size_t i = 0;

				data[i] = U(1) << (sizeof(U) * 8 - 1);

				while (++i < data.size()) {
					data[i] = (data[i - 1] >> U(1)) | data[i - 1];
				}
			}

			/// Returns a mask with the required number of significant bits for the specified shift
			/// \param shift The shift
			/// \return The mask
			const U& operator[](std::size_t shift) const {
				assert(shift > 0);

				return data[shift - 1];
			}
		};

		SignificantBits significantBits;

		Consts() : significantBits{} {}
	};

	static const Consts consts;

	inline static T apply(T value, dxf_int_t shift) {
		if (shift <= 0) {
			return value;
		}

		if (value < 0) {
			return static_cast<T>((static_cast<U>(value) >> shift) | consts.significantBits[shift]);

		} else if (value > 0) {
			return value >> shift;
		}

		return 0;
	}
};

template <typename T>
inline constexpr T rightShift(T value, dxf_int_t shift) {
	return RightShift<T>::apply(value, shift);
}

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

struct Long {
	inline static dxf_int_t compare(dxf_long_t v1, dxf_long_t v2) {
		if (v1 < v2) {
			return -1;
		}

		if (v2 < v1) {
			return 1;
		}

		return 0;
	}
};

struct Double {
	static const std::string NAN_STRING;
	static const std::string POSITIVE_INFINITY_STRING;
	static const std::string NEGATIVE_INFINITY_STRING;
	static const std::string ZERO_STRING;

	static const double POSITIVE_INFINITY;
	static const double NEGATIVE_INFINITY;
	static const double QUIET_NAN;

	static const dxf_long_t EXP_BIT_MASK;
	static const dxf_long_t SIGNIF_BIT_MASK;

	double value;

	inline static dxf_long_t toRawBits(double value) {
		dxf_long_t bits{};

		std::memcpy(&bits, &value, sizeof(bits));

		return bits;
	}

	inline static dxf_long_t toLongBits(double value) {
		dxf_long_t result = toRawBits(value);

		// Check for NaN based on values of bit fields, maximum
		// exponent and nonzero significand.
		if (((result & EXP_BIT_MASK) == EXP_BIT_MASK) && (result & SIGNIF_BIT_MASK) != 0L) {
			result = 0x7ff8000000000000LL;
		}

		return result;
	}

	inline static dxf_int_t compare(double d1, double d2) {
		if (d1 < d2) {
			return -1;	// Neither val is NaN, thisVal is smaller
		}

		if (d1 > d2) {
			return 1;  // Neither val is NaN, thisVal is larger
		}

		// Cannot use doubleToRawLongBits because of possibility of NaNs.
		dxf_long_t thisBits = toLongBits(d1);
		dxf_long_t anotherBits = toLongBits(d2);

		return (thisBits == anotherBits ? 0 :		// Values are equal
					(thisBits < anotherBits ? -1 :	// (-0.0, 0.0) or (!NaN, NaN)
						 1));						// (0.0, -0.0) or (NaN, !NaN)
	}

	static Double parseDouble(const std::string& str) { return {std::stod(str)}; }

	operator double() const {  // NOLINT(google-explicit-constructor)
		return value;
	}
};

struct WideDecimal {
	struct Consts {
		static const dxf_long_t NaN = 0;
		static const dxf_long_t POSITIVE_INFINITY = 0x100;
		static const dxf_long_t NEGATIVE_INFINITY = -0x100;

		static const dxf_long_t MAX_DOUBLE_SIGNIFICAND = (1LL << 53) - 1;
	    static const dxf_long_t MAX_DOUBLE_VALUE = 1LL << 51;
		static const dxf_long_t MIN_SIGNIFICAND;
		static const dxf_long_t MAX_SIGNIFICAND;
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

		static const std::array<std::string, 4> NF_STRING;
		static const std::array<dxf_double_t, 4> NF_DOUBLE;
		static const std::array<dxf_long_t, 4> NF_LONG;
		static const std::array<dxf_long_t, 4> NF_WIDE;

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

		std::array<dxf_double_t, 256> maxDoubles;

		Consts() : multipliers{}, divisors{}, maxDoubles{} {
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

			for (std::size_t rank = 0; rank < maxDoubles.size(); rank++)
				maxDoubles[rank] = MAX_DOUBLE_VALUE * multipliers[rank];
		}
	};

	static const Consts consts;

	inline static dxf_long_t toIntegerString(dxf_long_t significand, int rank) {
		assert(significand != 0 && rank != 0);

		if (rank > Consts::BIAS) {
			// fractional number with possible zero fraction and more trailing zeroes
			if (rank - Consts::BIAS <= static_cast<int>(Consts::EXACT_LONG_POWERS)) {
				dxf_long_t pow10 = consts.longPowers[rank - Consts::BIAS];
				dxf_long_t result = significand / pow10;

				if (result * pow10 == significand && result % consts.scientificModulo != 0) {
					return result;
				}
			}
		} else {
			// integer number with possible trailing zeroes
			if (Consts::BIAS - rank <= static_cast<int>(Consts::EXACT_LONG_POWERS) &&
				Consts::BIAS - rank <= static_cast<int>(Consts::MAX_TRAILING_ZEROES)) {
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
				return result.insert(static_cast<std::size_t>(dotPosition), ".");
			}

			if (firstDigit - dotPosition <= static_cast<dxf_long_t>(Consts::MAX_LEADING_ZEROES)) {
				return result.insert(static_cast<std::size_t>(firstDigit), consts.zeroChars, 0, static_cast<std::size_t>(2 + (firstDigit - dotPosition)));
			}
		} else {
			// integer number
			if (Consts::BIAS - rank <= static_cast<int>(Consts::MAX_TRAILING_ZEROES)) {
				return result.append(consts.zeroChars, 2, Consts::BIAS - rank);
			}
		}
		// use scientific notation
		dxf_long_t digits = static_cast<dxf_long_t>(result.length()) - firstDigit;

		if (digits != 1) {
			result.insert(static_cast<std::size_t>(firstDigit) + 1, ".");
		}

		result += Consts::EXPONENT_CHAR;

		return result.append(std::to_string(Consts::BIAS - rank + digits - 1));
	}

	inline static std::string toString(dxf_long_t wide) {
		dxf_long_t significand = rightShift(wide, 8);
		dxf_int_t rank = static_cast<dxf_int_t>(wide) & 0xFF;

		if (rank == 0) {
			return Consts::NF_STRING[dx::signum(significand) & 3];
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
		dxf_long_t significand = rightShift(wide, 8);
		dxf_int_t rank = static_cast<dxf_int_t>(wide) & 0xFF;

		// if both significand and rank coefficient can be exactly represented as double values -
		// perform a single double multiply or divide to compute properly rounded result
		if (significand <= Consts::MAX_DOUBLE_SIGNIFICAND && -significand <= Consts::MAX_DOUBLE_SIGNIFICAND) {
			if (rank > Consts::BIAS) {
				if (rank <= Consts::BIAS + static_cast<int>(Consts::EXACT_DOUBLE_POWERS)) {
					return static_cast<dxf_double_t>(significand) / consts.divisors[rank];
				}
			} else if (rank == Consts::BIAS) {
				return static_cast<dxf_double_t>(significand);
			} else {
				if (rank >= Consts::BIAS - static_cast<int>(Consts::EXACT_DOUBLE_POWERS)) {
					return static_cast<dxf_double_t>(significand) * consts.multipliers[rank];
				}

				if (rank == 0) {
					return Consts::NF_DOUBLE[dx::signum(significand) & 3];
				}
			}
		}

		// some component can't be exactly represented as double value -
		// extended precision computations with proper rounding are required;
		// avoid them with somewhat inefficient but guaranteed to work approach
		return Double::parseDouble(toString(wide));
	}

	inline static dxf_int_t signum(dxf_long_t wide) { return dx::signum<dxf_long_t>(rightShift(wide, 8)); }

	inline static dxf_long_t neg(dxf_long_t wide) {
		return (wide ^ static_cast<dxf_long_t>(-0x100)) + static_cast<dxf_long_t>(0x100);
	}

	inline static dxf_long_t abs(dxf_long_t wide) { return wide >= 0 ? wide : neg(wide); }

	inline static dxf_long_t composeWide(dxf_long_t significand, dxf_int_t scale) {
		// check exceedingly large scales to avoid rank overflows
		if (scale > 255 + static_cast<dxf_int_t>(Consts::EXACT_LONG_POWERS) -
				static_cast<dxf_int_t>(Consts::BIAS)) {
			return static_cast<dxf_long_t>(Consts::BIAS);
		}

		return composeNonFitting(significand, scale + Consts::BIAS, Consts::BIAS);
	}

	inline static bool isNaN(dxf_long_t wide) { return wide == 0; }

	inline static dxf_int_t compare(dxf_long_t w1, dxf_long_t w2) {
		if (w1 == w2) {
			return 0;
		}

		if ((w1 ^ w2) < 0) {
			return w1 > w2 ? 1 : -1;  // different signs, non-negative (including NaN) is greater
		}

		dxf_int_t r1 = static_cast<dxf_int_t>(w1) & 0xFF;
		dxf_int_t r2 = static_cast<dxf_int_t>(w2) & 0xFF;

		if (r1 > 0 && r2 > 0) {
			// fast path: for standard decimals (most frequent) use integer-only arithmetic
			if (r1 == r2) {
				return w1 > w2 ? 1 : -1;  // note: exact decimal equality is checked above
			}

			dxf_long_t s1 = rightShift(w1, 8);
			dxf_long_t s2 = rightShift(w2, 8);

			if (s1 == 0) {
				return dx::signum(-s2);
			}

			if (s2 == 0) {
				return dx::signum(s1);
			}

			if (r1 > r2) {
				if (r1 - r2 <= Consts::EXACT_LONG_POWERS) {
					dxf_long_t pow10 = consts.longPowers[r1 - r2];
					dxf_long_t scaled = s2 * pow10;

					if (scaled / pow10 == s2) {
						return Long::compare(s1, scaled);
					}
				}
			} else {
				if (r2 - r1 <= Consts::EXACT_LONG_POWERS) {
					dxf_long_t pow10 = consts.longPowers[r2 - r1];
					dxf_long_t scaled = s1 * pow10;

					if (scaled / pow10 == s1) {
						return Long::compare(scaled, s2);
					}
				}
			}
		}

		if (isNaN(w1)) return 1;
		if (isNaN(w2)) return -1;

		// slow path: for non-standard decimals (specials and extra precision) use floating arithmetic
		return Double::compare(toDouble(w1), toDouble(w2));
	}

	inline static dxf_long_t composeWide(double value) {
		return value >= 0 ? composeNonNegative(value, Consts::BIAS) :
			value < 0 ? neg(composeNonNegative(-value, Consts::BIAS)) :
					  Consts::NaN;
	}

	inline static dxf_long_t sum(dxf_long_t w1, dxf_long_t w2) {
		dxf_int_t r1 = static_cast<dxf_int_t>(w1) & 0xFF;
		dxf_int_t r2 = static_cast<dxf_int_t>(w2) & 0xFF;

		if (r1 > 0 && r2 > 0) {
			// fast path: for standard decimals (most frequent) use integer-only arithmetic
			dxf_long_t s1 = rightShift(w1, 8);
			dxf_long_t s2 = rightShift(w2, 8);

			if (s1 == 0) {
				return w2;
			}

			if (s2 == 0) {
				return w1;
			}

			if (r1 == r2) {
				return composeNonFitting(s1 + s2, r1, r1);
			}

			if (r1 > r2) {
				if (r1 - r2 <= Consts::EXACT_LONG_POWERS) {
					dxf_long_t pow10 = consts.longPowers[r1 - r2];
					dxf_long_t scaled = s2 * pow10;
					dxf_long_t result = s1 + scaled;

					if (scaled / pow10 == s2 && ((s1 ^ result) & (scaled ^ result)) >= 0) {
						return composeNonFitting(result, r1, r1);
					}
				}
			} else {
				if (r2 - r1 <= Consts::EXACT_LONG_POWERS) {
					dxf_long_t pow10 = consts.longPowers[r2 - r1];
					dxf_long_t scaled = s1 * pow10;
					dxf_long_t result = scaled + s2;

					if (scaled / pow10 == s1 && ((scaled ^ result) & (s2 ^ result)) >= 0) {
						return composeNonFitting(result, r2, r2);
					}
				}
			}
		}
		if (isNaN(w1) || isNaN(w2)) {
			return Consts::NaN;
		}

		// slow path: for non-standard decimals (specials and extra precision) use floating arithmetic
		return composeWide(toDouble(w1) + toDouble(w2));
	}

private:
	inline static dxf_long_t composeFitting(dxf_long_t significand, dxf_int_t rank, dxf_int_t targetRank) {
		assert(Consts::MIN_SIGNIFICAND <= significand && significand <= Consts::MAX_SIGNIFICAND && 1 <= rank &&
			   rank <= 255 && 1 <= targetRank && targetRank <= 255);

		// check for ZERO to avoid looping and getting de-normalized ZERO
		if (significand == 0) {
			return targetRank;
		}

		while (rank > targetRank && significand % 10 == 0) {
			significand /= 10;
			rank--;
		}

		while (rank < targetRank && Consts::MIN_SIGNIFICAND / 10 <= significand &&
			   significand <= Consts::MAX_SIGNIFICAND / 10) {
			significand *= 10;
			rank++;
		}

		if (rank == Consts::BIAS + 1 && targetRank == Consts::BIAS && Consts::MIN_SIGNIFICAND / 10 <= significand &&
			significand <= Consts::MAX_SIGNIFICAND / 10) {
			significand *= 10;
			rank++;
		}

		return (significand << 8) | rank;
	}

	inline static dxf_long_t div10(dxf_long_t significand, dxf_long_t pow10) {
		// divides significand by a power of 10 (10+) with proper rounding
		// divide by half of target divisor and then divide by 2 with proper rounding
		// this way we avoid overflows when rounding near Long.MAX_VALUE or Long.MIN_VALUE
		// it works properly because divisor is even (at least 10) and integer division truncates remainder toward 0
		// special handling of negative significand is needed to perform floor division (see MathUtil)
		return significand >= 0 ? rightShift(significand / rightShift(pow10, 1) + 1, 1)
								: rightShift((significand + 1) / rightShift(pow10, 1), 1);
	}

	inline static dxf_long_t composeNonFitting(dxf_long_t significand, dxf_int_t rank, dxf_int_t targetRank) {
		assert(1 <= targetRank && targetRank <= 255);

		// check for special rank to provide symmetrical conversion of special values
		if (rank <= 0) {
			return Consts::NF_WIDE[dx::signum(significand) & 3];
		}

		if (rank > 255 + Consts::EXACT_LONG_POWERS) {
			return targetRank;
		}

		// reduce significand to fit both significand and rank into supported ranges
		dxf_int_t reduction;

		if (Consts::MIN_SIGNIFICAND <= significand && significand <= Consts::MAX_SIGNIFICAND) {
			reduction = 0;
		} else if (Consts::MIN_SIGNIFICAND * 10 - 5 <= significand && significand < Consts::MAX_SIGNIFICAND * 10 + 5) {
			reduction = 1;
		} else if (Consts::MIN_SIGNIFICAND * 100 - 50 <= significand &&
				   significand < Consts::MAX_SIGNIFICAND * 100 + 50) {
			reduction = 2;
		} else {
			reduction = 3;
		}

		if (rank <= reduction) {
			return Consts::NF_WIDE[dx::signum(significand) & 3];
		}

		reduction = std::max(reduction, rank - 255);

		if (reduction > 0) {
			significand = div10(significand, consts.longPowers[reduction]);
			rank -= reduction;
		}

		return composeFitting(significand, rank, targetRank);
	}

	inline static dxf_int_t findRank(double value) {
		dxf_int_t rank = value > consts.maxDoubles[128] ? 0 : 128;

		rank += value > consts.maxDoubles[rank + 64] ? 0 : 64;
		rank += value > consts.maxDoubles[rank + 32] ? 0 : 32;
		rank += value > consts.maxDoubles[rank + 16] ? 0 : 16;
		rank += value > consts.maxDoubles[rank + 8] ? 0 : 8;
		rank += value > consts.maxDoubles[rank + 4] ? 0 : 4;
		rank += value > consts.maxDoubles[rank + 2] ? 0 : 2;
		rank += value > consts.maxDoubles[rank + 1] ? 0 : 1;

		return rank;
	}

	inline static dxf_long_t composeNonNegative(double value, dxf_int_t targetRank) {
		assert(value >= 0);

		dxf_int_t rank = findRank(value);
		if (rank <= 0)
			return Consts::POSITIVE_INFINITY;

		auto significand = static_cast<dxf_long_t>(value * consts.divisors[rank] + 0.5);

		return composeFitting(significand, rank, targetRank);
	}
};

}  // namespace dx