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

#include <limits.h>

#include "DXFeed.h"

#include "BufferedOutput.h"
#include "DXErrorHandling.h"
#include "DXMemory.h"
#include "DXAlgorithms.h"
#include "DXThreads.h"
#include "ConnectionContextData.h"

/* -------------------------------------------------------------------------- */
/*
 *	Buffered output connection context
 */
/* -------------------------------------------------------------------------- */

typedef struct {
	dxf_byte_t* out_buffer;
	int out_buffer_length;
	int current_out_buffer_position;

	dx_mutex_t guard;

	int set_field_mask;
} dx_buffered_output_connection_context_t;

#define MUTEX_FIELD_FLAG    (0x1)

#define CTX(context) \
	((dx_buffered_output_connection_context_t*)context)

#define IS_CUR_CAPACITY_ENOUGH(context, bytes_to_write) \
	(CTX(context)->current_out_buffer_position + bytes_to_write <= CTX(context)->out_buffer_length)

/* -------------------------------------------------------------------------- */

static void dx_clear_buffered_output_context_data (dx_buffered_output_connection_context_t* context);

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_buffered_output) {
	dx_buffered_output_connection_context_t* context = dx_calloc(1, sizeof(dx_buffered_output_connection_context_t));

	if (context == NULL) {
		return false;
	}

	if (!(dx_mutex_create(&context->guard) && (context->set_field_mask |= MUTEX_FIELD_FLAG)) || /* setting the flag if the function succeeded, not setting otherwise */
		!dx_set_subsystem_data(connection, dx_ccs_buffered_output, context)) {

		dx_clear_buffered_output_context_data(context);

		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_buffered_output) {
	bool res = true;
	dx_buffered_output_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_buffered_output, &res);

	if (context == NULL) {
		return res;
	}

	dx_clear_buffered_output_context_data(context);

	return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_buffered_output) {
	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Connection context functions
 */
/* -------------------------------------------------------------------------- */

static void dx_clear_buffered_output_context_data (dx_buffered_output_connection_context_t* context) {
	if (context == NULL) {
		return;
	}

	if (IS_FLAG_SET(context->set_field_mask, MUTEX_FIELD_FLAG)) {
		dx_mutex_destroy(&(context->guard));
	}

	dx_free(context);
}

/* -------------------------------------------------------------------------- */

void* dx_get_buffered_output_connection_context (dxf_connection_t connection) {
	return dx_get_subsystem_data(connection, dx_ccs_buffered_output, NULL);
}

/* -------------------------------------------------------------------------- */

bool dx_lock_buffered_output (void* context) {
	return dx_mutex_lock(&(CTX(context)->guard));
}

/* -------------------------------------------------------------------------- */

bool dx_unlock_buffered_output (void* context) {
	return dx_mutex_unlock(&(CTX(context)->guard));
}

/* -------------------------------------------------------------------------- */
/*
 *	Buffer manipulators implementation
 */
/* -------------------------------------------------------------------------- */

dxf_byte_t* dx_get_out_buffer (void* context) {
	return CTX(context)->out_buffer;
}

/* -------------------------------------------------------------------------- */

int dx_get_out_buffer_length (void* context) {
	return CTX(context)->out_buffer_length;
}

/* -------------------------------------------------------------------------- */

void dx_set_out_buffer (void* context, dxf_byte_t* new_buffer, int new_length) {
	CTX(context)->out_buffer = new_buffer;
	CTX(context)->out_buffer_length = new_length;
	CTX(context)->current_out_buffer_position = 0;
}

/* -------------------------------------------------------------------------- */

int dx_get_out_buffer_position (void* context) {
	return CTX(context)->current_out_buffer_position;
}

/* -------------------------------------------------------------------------- */

void dx_set_out_buffer_position (void* context, int new_position) {
	CTX(context)->current_out_buffer_position = new_position;
}

/* -------------------------------------------------------------------------- */

bool dx_ensure_capacity (void* context, int required_capacity) {
	if (INT_MAX - CTX(context)->current_out_buffer_position < CTX(context)->out_buffer_length) {
		return dx_set_error_code(dx_bioec_buffer_overflow);
	}

	if (!IS_CUR_CAPACITY_ENOUGH(context, required_capacity)) {
		dxf_long_t tmp_len = (dxf_long_t)CTX(context)->out_buffer_length << 1;
		int tmp_min = (int)MIN(tmp_len, (dxf_long_t)INT_MAX);
		int length = MAX(MAX(tmp_min, 1024), CTX(context)->current_out_buffer_position + required_capacity);
		dxf_byte_t* new_buffer = (dxf_byte_t*)dx_malloc(length);

		if (new_buffer == NULL) {
			return false;
		}

		dx_memcpy(new_buffer, CTX(context)->out_buffer, CTX(context)->out_buffer_length);
		dx_free(CTX(context)->out_buffer);

		CTX(context)->out_buffer = new_buffer;
		CTX(context)->out_buffer_length = length;
	}

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Write operation helpers
 */
/* -------------------------------------------------------------------------- */

static bool dx_check_write_possibility (void* context, int bytes_to_write) {
	if (CTX(context)->out_buffer == NULL) {
		return dx_set_error_code(dx_bioec_buffer_not_initialized);
	}

	return IS_CUR_CAPACITY_ENOUGH(context, bytes_to_write) || dx_ensure_capacity(context, bytes_to_write);
}

/* -------------------------------------------------------------------------- */

void dx_write_utf2_unchecked (void* context, dxf_int_t code_point) {
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0xC0 | code_point >> 6);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0x80 | code_point & 0x3F);
}

/* -------------------------------------------------------------------------- */

void dx_write_utf3_unchecked (void* context, dxf_int_t code_point) {
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0xE0 | code_point >> 12);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0x80 | code_point >> 6 & 0x3F);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0x80 | code_point & 0x3F);
}

/* -------------------------------------------------------------------------- */

void dx_write_utf4_unchecked (void* context, dxf_int_t code_point) {
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0xF0 | code_point >> 18);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0x80 | code_point >> 12 & 0x3F);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0x80 | code_point >> 6 & 0x3F);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0x80 | code_point & 0x3F);
}

/* -------------------------------------------------------------------------- */

void dx_write_int_unchecked (void* context, dxf_int_t value) {
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 24);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 16);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 8);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)value;
}

/* -------------------------------------------------------------------------- */

bool dx_write_byte_buffer_segment (void* context, const dxf_byte_t* buffer, int buffer_length,
								int segment_offset, int segment_length) {
	if ((segment_offset | segment_length | (segment_offset + segment_length) |
		(buffer_length - (segment_offset + segment_length))) < 0) {
		/* a fast way to check if any of the values above are negative */

		return dx_set_error_code(dx_bioec_index_out_of_bounds);
	}

	if (!(IS_CUR_CAPACITY_ENOUGH(context, segment_length) || dx_ensure_capacity(context, segment_length))) {
		return false;
	}

	dx_memcpy(CTX(context)->out_buffer + CTX(context)->current_out_buffer_position, buffer + segment_offset, segment_length);

	CTX(context)->current_out_buffer_position += segment_length;

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Write operations implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_write_boolean (void* context, dxf_bool_t value) {
	CHECKED_CALL_2(dx_check_write_possibility, context, 1);

	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (value ? (dxf_byte_t)1 : (dxf_byte_t)0);

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_write_byte (void* context, dxf_byte_t value) {
	CHECKED_CALL_2(dx_check_write_possibility, context, 1);

	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)value;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_write_short (void* context, dxf_short_t value) {
	CHECKED_CALL_2(dx_check_write_possibility, context, 2);

	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 8);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)value;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_write_char (void* context, dxf_char_t value) {
	CHECKED_CALL_2(dx_check_write_possibility, context, 2);

	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 8);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)value;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_write_int (void* context, dxf_int_t value) {
	CHECKED_CALL_2(dx_check_write_possibility, context, 4);

	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 24);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 16);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 8);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)value;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_write_long (void* context, dxf_long_t value) {
	CHECKED_CALL_2(dx_check_write_possibility, context, 8);

	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 56);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 48);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 40);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 32);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 24);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 16);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 8);
	CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)value;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_write_float (void* context, dxf_float_t value) {
	return dx_write_int(context, *((dxf_int_t*)&value));
}

/* -------------------------------------------------------------------------- */

bool dx_write_double (void* context, dxf_double_t value) {
	return dx_write_long(context, *((dxf_long_t*)&value));
}

/* -------------------------------------------------------------------------- */

bool dx_write_byte_buffer (void* context, const dxf_char_t* value) {
	size_t length = dx_string_length(value);
	size_t i = 0;

	for (; i < length; ++i) {
		CHECKED_CALL_2(dx_write_byte, context, (dxf_byte_t)value[i]);
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_write_char_buffer (void* context, const dxf_char_t* value) {
	size_t length = dx_string_length(value);
	size_t i = 0;

	for (; i < length; ++i) {
		CHECKED_CALL_2(dx_write_char, context, value[i]);
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_write_utf (void* context, dxf_const_string_t value) {
	size_t strlen = dx_string_length(value);
	dxf_short_t utflen = 0;
	size_t i = 0;

	for (; i < strlen; ++i) {
		dxf_char_t c = value[i];

		if (c > 0 && c <= 0x007F) {
			++utflen;
		} else if (c <= 0x07FF) {
			utflen += 2;
		} else {
			utflen += 3;
		}
	}

	if ((size_t)utflen < strlen || utflen > 65535) {
		return dx_set_error_code(dx_utfec_bad_utf_data_format);
	}

	CHECKED_CALL_2(dx_write_short, context, utflen);

	for (i = 0; i < strlen; ++i) {
		dxf_char_t c;

		if (!(IS_CUR_CAPACITY_ENOUGH(context, 3) || dx_ensure_capacity(context, 3))) {
			return false;
		}

		c = value[i];

		if (c > 0 && c <= 0x007F) {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)c;
		} else if (c <= 0x07FF) {
			dx_write_utf2_unchecked(context, c);
		} else {
			dx_write_utf3_unchecked(context, c);
		}
	}

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Compact write operations implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_write_compact_int (void* context, dxf_int_t value) {
	CHECKED_CALL_2(dx_check_write_possibility, context, 5);

	if (value >= 0) {
		if (value < 0x40) {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)value;
		} else if (value < 0x2000) {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0x80 | value >> 8);
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)value;
		} else if (value < 0x100000) {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0xC0 | value >> 16);
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 8);
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)value;
		} else if (value < 0x08000000) {
			dx_write_int_unchecked(context, 0xE0000000 | value);
		} else {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)0xF0;
			dx_write_int_unchecked(context, value);
		}
	} else {
		if (value >= -0x40) {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0x7F & value);
		} else if (value >= -0x2000) {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0xBF & value >> 8);
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)value;
		} else if (value >= -0x100000) {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0xDF & value >> 16);
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(value >> 8);
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)value;
		} else if (value >= -0x08000000) {
			dx_write_int_unchecked(context, 0xEFFFFFFF & value);
		} else {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)0xF7;
			dx_write_int_unchecked(context, value);
		}
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_write_compact_long (void* context, dxf_long_t value) {
	dxf_int_t hi = (dxf_int_t)(value >> 32);

	if (value == (dxf_long_t)(dxf_int_t)value) {
		return dx_write_compact_int(context, (dxf_int_t)value);
	}

	CHECKED_CALL_2(dx_check_write_possibility, context, 9);

	if (hi >= 0) {
		if (hi < 0x04) {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0xF0 | hi);
		} else if (hi < 0x0200) {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0xF8 | hi >> 8);
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)hi;
		} else if (hi < 0x010000) {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)0xFC;
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(hi >> 8);
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)hi;
		} else if (hi < 0x800000) {
			dx_write_int_unchecked(context, 0xFE000000 | hi);
		} else {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)0xFF;
			dx_write_int_unchecked(context, hi);
		}
	} else {
		if (hi >= -0x04) {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0xF7 & hi);
		} else if (hi >= -0x0200) {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(0xFB & hi >> 8);
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)hi;
		} else if (hi >= -0x010000) {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)0xFD;
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)(hi >> 8);
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)hi;
		} else if (hi >= -0x800000) {
			dx_write_int_unchecked(context, 0xFEFFFFFF & hi);
		} else {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)0xFF;
			dx_write_int_unchecked(context, hi);
		}
	}

	dx_write_int_unchecked(context, (dxf_int_t)value);

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_write_byte_array (void* context, const dxf_byte_t* buffer, dxf_int_t buffer_size) {
	if (buffer == NULL) {
		return dx_write_compact_int(context, -1);
	}

	CHECKED_CALL_2(dx_write_compact_int, context, buffer_size);
	CHECKED_CALL_5(dx_write_byte_buffer_segment, context, buffer, buffer_size, 0, buffer_size);

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	UTF write operations implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_write_utf_char (void* context, dxf_int_t code_point) {
	CHECKED_CALL_2(dx_check_write_possibility, context, 4);

	if (code_point < 0) {
		return dx_set_error_code(dx_utfec_bad_utf_data_format);
	}

	if (code_point <= 0x007F) {
		CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)code_point;
	} else if (code_point <= 0x07FF) {
		dx_write_utf2_unchecked(context, code_point);
	} else if (code_point <= 0xFFFF) {
		dx_write_utf3_unchecked(context, code_point);
	} else if (code_point <= 0x10FFFF) {
		dx_write_utf4_unchecked(context, code_point);
	} else {
		return dx_set_error_code(dx_utfec_bad_utf_data_format);
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_write_utf_string (void* context, dxf_const_string_t value) {
	size_t strlen;
	dxf_int_t utflen;
	size_t i = 0;

	if (value == NULL) {
		return dx_write_compact_int(context, -1);
	}

	strlen = dx_string_length(value);
	utflen = 0;

	for (; i < strlen; ) {
		dxf_char_t c = value[i++];

		if (c <= 0x007F) {
			utflen++;
		} else if (c <= 0x07FF) {
			utflen += 2;
		} else if (dx_is_high_surrogate(c) && i < strlen && dx_is_low_surrogate(value[i])) {
			++i;
			utflen += 4;
		} else {
			utflen += 3;
		}
	}

	if ((size_t)utflen < strlen) {
		return dx_set_error_code(dx_utfec_bad_utf_data_format);
	}

	CHECKED_CALL_2(dx_write_compact_int, context, utflen);

	for (i = 0; i < strlen; ) {
		dxf_char_t c;

		if (!(IS_CUR_CAPACITY_ENOUGH(context, 4) || dx_ensure_capacity(context, 4))) {
			return false;
		}

		c = value[i++];

		if (c <= 0x007F) {
			CTX(context)->out_buffer[CTX(context)->current_out_buffer_position++] = (dxf_byte_t)c;
		} else if (c <= 0x07FF) {
			dx_write_utf2_unchecked(context, c);
		} else if (dx_is_high_surrogate(c) && i < strlen && dx_is_low_surrogate(value[i])) {
			dx_write_utf4_unchecked(context, dx_surrogates_to_code_point(c, value[i++]));
		} else {
			dx_write_utf3_unchecked(context, c);
		}
	}

	return true;
}
