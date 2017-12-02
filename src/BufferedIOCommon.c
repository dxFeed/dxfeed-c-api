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

#include "BufferedIOCommon.h"
#include "DXErrorHandling.h"
#include "DXMemory.h"

/* -------------------------------------------------------------------------- */
/*
 *	Common Unicode data
 */
/* -------------------------------------------------------------------------- */

/*
 * The minimum value of a Unicode high-surrogate code unit in the
 * UTF-16 encoding. A high-surrogate is also known as a
 * leading-surrogate.
 */
static const dxf_char_t MIN_HIGH_SURROGATE = 0xD800;

/*
 * The maximum value of a Unicode high-surrogate code unit in the
 * UTF-16 encoding. A high-surrogate is also known as a
 * leading-surrogate.
 */
static const dxf_char_t MAX_HIGH_SURROGATE = 0xDBFF;

/*
 * The minimum value of a Unicode low-surrogate code unit in the
 * UTF-16 encoding. A low-surrogate is also known as a
 * trailing-surrogate.
 */
static const dxf_char_t MIN_LOW_SURROGATE  = 0xDC00;

/*
 * The maximum value of a Unicode low-surrogate code unit in the
 * UTF-16 encoding. A low-surrogate is also known as a
 * trailing-surrogate.
 */
static const dxf_char_t MAX_LOW_SURROGATE  = 0xDFFF;

/*
 * The minimum value of a supplementary code point.
 */
static const dxf_int_t MIN_SUPPLEMENTARY_CODE_POINT = 0x010000;

/*
 * The maximum value of a Unicode code point.
 */
static dxf_int_t MAX_CODE_POINT = 0x10ffff;

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary Unicode functions
 */
/* -------------------------------------------------------------------------- */

void dx_code_point_to_surrogates (dxf_int_t code_point, dxf_string_t dest_buffer, int dest_buffer_pos) {
	dxf_int_t offset = code_point - MIN_SUPPLEMENTARY_CODE_POINT;

	dest_buffer[dest_buffer_pos+1] = (dxf_char_t)((offset & 0x3ff) + MIN_LOW_SURROGATE);
	dest_buffer[dest_buffer_pos] = (dxf_char_t)((offset >> 10) + MIN_HIGH_SURROGATE);
}

/* -------------------------------------------------------------------------- */
/*
 *	Common Unicode functions implementation
 */
/* -------------------------------------------------------------------------- */

int dx_is_high_surrogate (dxf_char_t value) {
	return value >= MIN_HIGH_SURROGATE && value <= MAX_HIGH_SURROGATE;
}

/* -------------------------------------------------------------------------- */

int dx_is_low_surrogate (dxf_char_t value) {
	return value >= MIN_LOW_SURROGATE && value <= MAX_LOW_SURROGATE;
}

/* -------------------------------------------------------------------------- */

dxf_int_t dx_surrogates_to_code_point (dxf_char_t high, dxf_char_t low) {
	return ((high - MIN_HIGH_SURROGATE) << 10) + (low - MIN_LOW_SURROGATE) + MIN_SUPPLEMENTARY_CODE_POINT;
}

/* -------------------------------------------------------------------------- */

bool dx_code_point_to_utf16_chars (dxf_int_t code_point, dxf_string_t dest_buffer, int dest_buffer_pos, int dest_buffer_length, OUT int* result) {
	if (code_point < 0 || code_point > MAX_CODE_POINT || dest_buffer == NULL || result == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	if (code_point < MIN_SUPPLEMENTARY_CODE_POINT) {
		if (dest_buffer_length - dest_buffer_pos < 1) {
			return dx_set_error_code(dx_bioec_index_out_of_bounds);
		}

		dest_buffer[dest_buffer_pos] = (dxf_char_t)code_point;
		*result = 1;

		return true;
	}

	if (dest_buffer_length - dest_buffer_pos < 2) {
		return dx_set_error_code(dx_bioec_index_out_of_bounds);
	}

	dx_code_point_to_surrogates(code_point, dest_buffer, dest_buffer_pos);
	*result = 2;

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Common compact API functions implementation
 */
/* -------------------------------------------------------------------------- */

int dx_get_compact_size (dxf_long_t value) {
	if (value >= 0) {
		if (value < 0x40) {
			return 1;
		} else if (value < 0x2000) {
			return 2;
		} else if (value < 0x100000) {
			return 3;
		} else if (value < 0x08000000) {
			return 4;
		} else if (value < 0x0400000000L) {
			return 5;
		} else if (value < 0x020000000000L) {
			return 6;
		} else if (value < 0x01000000000000L) {
			return 7;
		} else if (value < 0x80000000000000L) {
			return 8;
		} else {
			return 9;
		}
	} else {
		if (value >= -0x40) {
			return 1;
		} else if (value >= -0x2000) {
			return 2;
		} else if (value >= -0x100000) {
			return 3;
		} else if (value >= -0x08000000) {
			return 4;
		} else if (value >= -0x0400000000L) {
			return 5;
		} else if (value >= -0x020000000000L) {
			return 6;
		} else if (value >= -0x01000000000000L) {
			return 7;
		} else if (value >= -0x80000000000000L) {
			return 8;
		} else {
			return 9;
		}
	}
}
