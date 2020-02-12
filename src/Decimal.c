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

#include "Decimal.h"

#include <math.h>
#include <float.h>

#define DX_POSITIVE_INFINITY DBL_MAX * 10

static const dxf_int_t STANDARD_MANTISSA_SHIFT = 4;
static const dxf_int_t FLAG_MASK = 0x7F;
static const dxf_int_t M128_EXTRA = 6;
static const dxf_int_t M128_FLAG = 0x60;  //M128_EXTRA << STANDARD_MANTISSA_SHIFT;
static const dxf_int_t EXTRA_PRECISION_MANTISSA_SHIFT = 7;
static const dxf_long_t P7_M128_CONVERTER = 10000000;
static const dxf_int_t UNITY_POWER = 9;

#ifndef MACOSX
static const dxf_ulong_t __inf = 0x7f80000000000000;
#endif

static dxf_double_t MULTIPLIERS[] = {
	DX_POSITIVE_INFINITY,
	100000000.0,
	10000000.0,
	1000000.0,
	100000.0,
	10000.0,
	1000.0,
	100.0,
	10.0,
	1.0,
	0.1,
	0.01,
	0.001,
	0.0001,
	0.00001,
	0.000001
};

static const double DIVISORS[] = {
	0.0,
	0.00000001,
	0.0000001,
	0.000001,
	0.00001,
	0.0001,
	0.001,
	0.01,
	0.1,
	1,
	10,
	100,
	1000,
	10000,
	100000,
	1000000
};

static const dxf_int_t EXTRA_PRECISION_DIVISORS[] = {
	0, // for canonical NaN
	0, // for canonical positive infinity
	10000000,  // 10^7
	100000000, // 10^8
	0, // reserved
	0, // reserved
	128,
	0, // for canonical negative infinity
};

bool dx_int_to_double (dxf_int_t integer, OUT dxf_double_t* decimal) {
	dxf_int_t power = integer & 0x0F;
	dxf_int_t mantissa ;

	if (power == 0) {
		// extra precision and special cases
		dxf_int_t divisor = EXTRA_PRECISION_DIVISORS[((integer >> STANDARD_MANTISSA_SHIFT) & 0x07)];

		if (divisor != 0) {
			// mantissa in highest 25 bits for supported extra precision formats
			*decimal  = (dxf_double_t)(integer >> EXTRA_PRECISION_MANTISSA_SHIFT) / divisor;
			return true;
		}
	}

	mantissa = (integer >> STANDARD_MANTISSA_SHIFT);
	*decimal = power <= UNITY_POWER ? mantissa * MULTIPLIERS[power] : mantissa / DIVISORS[power];

	return true;
}
