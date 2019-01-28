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

#include "BufferedInput.h"
#include "DXErrorHandling.h"
#include "DXMemory.h"
#include "DXAlgorithms.h"
#include "ConnectionContextData.h"

/* -------------------------------------------------------------------------- */
/*
 *	Buffered input connection context
 */
/* -------------------------------------------------------------------------- */

typedef struct {
	dxf_byte_t* in_buffer;
	int in_buffer_length;
	int in_buffer_limit;
	int current_in_buffer_position;
} dx_buffered_input_connection_context_t;

#define CTX(context) \
	((dx_buffered_input_connection_context_t*)context)

#define IS_BUF_CAPACITY_ENOUGH(context, bytes_to_read) \
	(CTX(context)->current_in_buffer_position + bytes_to_read <= CTX(context)->in_buffer_length)

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_buffered_input) {
	dx_buffered_input_connection_context_t* context = dx_calloc(1, sizeof(dx_buffered_input_connection_context_t));

	if (context == NULL) {
		return false;
	}

	if (!dx_set_subsystem_data(connection, dx_ccs_buffered_input, context)) {
		dx_free(context);

		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_buffered_input) {
	bool res = true;
	dx_buffered_input_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_buffered_input, &res);

	if (context == NULL) {
		return res;
	}

	dx_free(context);

	return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_buffered_input) {
	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Connection context functions
 */
/* -------------------------------------------------------------------------- */

void* dx_get_buffered_input_connection_context (dxf_connection_t connection) {
	return dx_get_subsystem_data(connection, dx_ccs_buffered_input, NULL);
}

/* -------------------------------------------------------------------------- */
/*
 *	Buffer manipulators implementation
 */
/* -------------------------------------------------------------------------- */

dxf_byte_t* dx_get_in_buffer (void* context) {
	return CTX(context)->in_buffer;
}

/* -------------------------------------------------------------------------- */

int dx_get_in_buffer_length (void* context) {
	return CTX(context)->in_buffer_length;
}

/* -------------------------------------------------------------------------- */

void dx_set_in_buffer (void* context, dxf_byte_t* new_buffer, int new_length) {
	CTX(context)->in_buffer = new_buffer;
	CTX(context)->in_buffer_length = new_length;
}

/* -------------------------------------------------------------------------- */

int dx_get_in_buffer_position (void* context) {
	return CTX(context)->current_in_buffer_position;
}

/* -------------------------------------------------------------------------- */

void dx_set_in_buffer_position (void* context, int new_position) {
	CTX(context)->current_in_buffer_position = new_position;
}

/* -------------------------------------------------------------------------- */

int dx_get_in_buffer_limit (void* context) {
	return CTX(context)->in_buffer_limit;
}

/* -------------------------------------------------------------------------- */

void dx_set_in_buffer_limit (void* context, int new_limit) {
	CTX(context)->in_buffer_limit = new_limit;
}

/* -------------------------------------------------------------------------- */
/*
 *	Read operation helpers
 */
/* -------------------------------------------------------------------------- */

static int dx_move_buffer_pos (void* context, int offset) {
	int actual_offset = (offset > 0 ? MIN(offset, (CTX(context)->in_buffer_length - CTX(context)->current_in_buffer_position))
									: MAX(offset, -(CTX(context)->current_in_buffer_position)));

	CTX(context)->current_in_buffer_position += actual_offset;

	return offset - actual_offset;
}

/* -------------------------------------------------------------------------- */

bool dx_validate_buffer_and_value (void* context, void* value, int value_size) {
	if (CTX(context)->in_buffer == NULL) {
		return dx_set_error_code(dx_bioec_buffer_not_initialized);
	}

	if (value == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	if (!IS_BUF_CAPACITY_ENOUGH(context, value_size)) {
		return dx_set_error_code(dx_bioec_buffer_underflow);
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_utf2 (void* context, int first, OUT dxf_int_t* value) {
	dxf_byte_t second;

	if (value == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	CHECKED_CALL_2(dx_read_byte, context, &second);

	if ((second & 0xC0) != 0x80) {
		return dx_set_error_code(dx_utfec_bad_utf_data_format_server);
	}

	*value = (dxf_char_t)(((first & 0x1F) << 6) | (second & 0x3F));

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_utf3 (void* context, int first, OUT dxf_int_t* value) {
	dxf_short_t tail;

	if (value == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	CHECKED_CALL_2(dx_read_short, context, &tail);

	if ((tail & 0xC0C0) != 0x8080) {
		return dx_set_error_code(dx_utfec_bad_utf_data_format_server);
	}

	*value = (dxf_char_t)(((first & 0x0F) << 12) | ((tail & 0x3F00) >> 2) | (tail & 0x3F));

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_utf4 (void* context, int first, OUT dxf_int_t* res) {
	dxf_byte_t second;
	dxf_short_t tail;

	if (res == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	CHECKED_CALL_2(dx_read_byte, context, &second);
	CHECKED_CALL_2(dx_read_short, context, &tail);

	if ((second & 0xC0) != 0x80 || (tail & 0xC0C0) != 0x8080) {
		return dx_set_error_code(dx_utfec_bad_utf_data_format_server);
	}

	*res = ((first & 0x07) << 18) | ((second & 0x3F) << 12) | ((tail & 0x3F00) >> 2) | (tail & 0x3F);

	if (*res > 0x10FFFF) {
		return dx_set_error_code(dx_utfec_bad_utf_data_format_server);
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_utf_sequence (void* context, int utflen, bool lenInChars, OUT dxf_string_t* value) {
	dxf_string_t buffer;
	int count = 0;
	dxf_int_t tmpCh;

	if (value == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	if (utflen == 0) {
		*value = (dxf_string_t)dx_calloc(1, sizeof(dxf_char_t));

		return (*value != NULL);
	}

	/* May be slightly large, if length is in bytes */
	buffer = dx_create_string(utflen);

	if (buffer == NULL) {
		return false;
	}

	while (utflen > 0) {
		dxf_byte_t c;

		CHECKED_CALL_2(dx_read_byte, context, &c);

		if (c >= 0) {
			--utflen;
			buffer[count++] = (dxf_char_t)c;
		} else if ((c & 0xE0) == 0xC0) {
			utflen -= lenInChars?1:2;

			CHECKED_CALL_3(dx_read_utf2, context, c, &tmpCh);

			buffer[count++] = tmpCh;
		} else if ((c & 0xF0) == 0xE0) {
			utflen -= lenInChars?1:3;

			CHECKED_CALL_3(dx_read_utf3, context, c, &tmpCh);

			buffer[count++] = tmpCh;
		} else if ((c & 0xF8) == 0xF0) {
			dxf_int_t tmpInt;
			int res;
			utflen -= lenInChars?1:4;

			CHECKED_CALL_3(dx_read_utf4, context, c, &tmpInt);
			CHECKED_CALL_5(dx_code_point_to_utf16_chars, tmpInt, buffer, count, utflen, &res);

			count += res;
		} else {
			return dx_set_error_code(dx_utfec_bad_utf_data_format_server);
		}
	}

	if (utflen < 0) {
		return dx_set_error_code(dx_utfec_bad_utf_data_format_server);
	}

	buffer[count] = 0; /* end of sequence */

	*value = buffer;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_byte_buffer_segment (void* context, dxf_byte_t* buffer, int buffer_length,
								int segment_offset, int segment_length) {
	if ((segment_offset | segment_length | (segment_offset + segment_length) |
		(buffer_length - (segment_offset + segment_length))) < 0) {
		/* a fast way to check if any of the values above are negative */

		return dx_set_error_code(dx_bioec_index_out_of_bounds);
	}

	if (!IS_BUF_CAPACITY_ENOUGH(context, segment_length)) {
		return dx_set_error_code(dx_bioec_buffer_underflow);
	}

	dx_memcpy(buffer + segment_offset, CTX(context)->in_buffer + CTX(context)->current_in_buffer_position, segment_length);

	CTX(context)->current_in_buffer_position += segment_length;

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Read operations implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_read_boolean (void* context, OUT dxf_bool_t* value) {
	CHECKED_CALL_3(dx_validate_buffer_and_value, context, value, 1);

	*value = CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++];

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_byte (void* context, OUT dxf_byte_t* value) {
	CHECKED_CALL_3(dx_validate_buffer_and_value, context, value, 1);

	*value = CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++];

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_unsigned_byte (void* context, OUT dxf_uint_t* value) {
	CHECKED_CALL_3(dx_validate_buffer_and_value, context, value, 1);

	*value = ((dxf_uint_t)CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++]) & 0xFF;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_short (void* context, OUT dxf_short_t* value) {
	CHECKED_CALL_3(dx_validate_buffer_and_value, context, value, 2);

	*value = ((dxf_short_t)CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] << 8);
	*value = *value | ((dxf_short_t)CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] & 0xFF);

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_unsigned_short (void* context, OUT dxf_uint_t* value) {
	CHECKED_CALL_3(dx_validate_buffer_and_value, context, value, 2);

	*value = ((dxf_uint_t)(CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] & 0xFF) << 8);
	*value = *value | ((dxf_uint_t)CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] & 0xFF);

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_int (void* context, OUT dxf_int_t* value) {
	CHECKED_CALL_3(dx_validate_buffer_and_value, context, value, 4);

	*value = ((dxf_int_t)(CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] & 0xFF) << 24);
	*value = *value | ((dxf_int_t)(CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] & 0xFF) << 16);
	*value = *value | ((dxf_int_t)(CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++]  & 0xFF) << 8);
	*value = *value | ((dxf_int_t)CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] & 0xFF);

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_long (void* context, OUT dxf_long_t* value) {
	CHECKED_CALL_3(dx_validate_buffer_and_value, context, value, 8);

	*value = ((dxf_long_t)CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] << 56);
	*value = *value | ((dxf_long_t)CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] << 48);
	*value = *value | ((dxf_long_t)CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] << 40);
	*value = *value | ((dxf_long_t)CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] << 32);
	*value = *value | ((dxf_long_t)CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] << 24);
	*value = *value | ((dxf_long_t)CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] << 16);
	*value = *value | ((dxf_long_t)CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] << 8);
	*value = *value | ((dxf_long_t)CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] & 0xFF);

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_float (void* context, OUT dxf_float_t* value) {
	dxf_int_t int_val;

	CHECKED_CALL_2(dx_read_int, context, &int_val);

	*value = *((dxf_float_t*)(&int_val));

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_double (void* context, OUT dxf_double_t* value) {
	dxf_long_t long_val;

	CHECKED_CALL_2(dx_read_long, context, &long_val);

	*value = *((dxf_double_t*)(&long_val));

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_line (void* context, OUT dxf_string_t* value) {
	static const int tmp_buf_size = 128;

	dxf_string_t tmp_buffer = NULL;
	int count = 0;

	CHECKED_CALL_3(dx_validate_buffer_and_value, context, value, 1);

	tmp_buffer = (dxf_char_t*)dx_calloc(tmp_buf_size, sizeof(dxf_char_t));

	if (tmp_buffer == NULL) {
		return false;
	}

	while (CTX(context)->current_in_buffer_position < CTX(context)->in_buffer_length) {
		dxf_char_t c = CTX(context)->in_buffer[CTX(context)->current_in_buffer_position++] & 0xFF;

		if (c == '\n')
			break;
		if (c == '\r') {
			if ((CTX(context)->current_in_buffer_position < CTX(context)->in_buffer_length) && CTX(context)->in_buffer[CTX(context)->current_in_buffer_position] == '\n') {
				++CTX(context)->current_in_buffer_position;
			}

			break;
		}

		if (count >= tmp_buf_size) {
			dxf_char_t* tmp = (dxf_char_t*)dx_calloc(tmp_buf_size << 1, sizeof(dxf_char_t));

			if (tmp == NULL) {
				dx_free(tmp_buffer);

				return false;
			}

			dx_memcpy(tmp, tmp_buffer, tmp_buf_size * sizeof(dxf_char_t));
			dx_free(tmp_buffer);

			tmp_buffer = tmp;
		}

		tmp_buffer[count++] = c;
	}

	tmp_buffer[count] = 0;
	*value = tmp_buffer;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_utf (void* context, OUT dxf_string_t* value) {
	dxf_int_t utflen;

	CHECKED_CALL_2(dx_read_unsigned_short, context, &utflen);

	return dx_read_utf_sequence(context, utflen, false, OUT value);
}

/* -------------------------------------------------------------------------- */
/*
 *	Compact read operations implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_read_compact_int (void* context, OUT dxf_int_t* value) {
	dxf_int_t n;

	if (value == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	/* The ((n << k) >> k) expression performs two's complement */
	CHECKED_CALL_2(dx_read_unsigned_byte, context, &n);

	if (n < 0x80) {
		*value = (n << 25) >> 25;

		return true;
	}

	if (n < 0xC0) {
		dxf_int_t m;

		CHECKED_CALL_2(dx_read_unsigned_byte, context, &m);

		*value = (((n << 8) | m) << 18) >> 18;

		return true;
	}

	if (n < 0xE0) {
		dxf_int_t m;

		CHECKED_CALL_2(dx_read_unsigned_short, context, &m);

		*value = (((n << 16) | m) << 11) >> 11;

		return true;
	}

	if (n < 0xF0) {
		dxf_int_t tmp_byte;
		dxf_int_t tmp_short;

		CHECKED_CALL_2(dx_read_unsigned_byte, context, &tmp_byte);
		CHECKED_CALL_2(dx_read_unsigned_short, context, &tmp_short);

		*value = (((n << 24) | (tmp_byte << 16) | tmp_short) << 4) >> 4;

		return true;
	}


	/* The encoded number is possibly out of range, some bytes have to be skipped. */

	while (((n <<= 1) & 0x10) != 0) {
		dx_move_buffer_pos(context, 1);
	}

	CHECKED_CALL_2(dx_read_int, context, &n);

	*value = n;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_compact_long (void* context, OUT dxf_long_t* value) {
	dxf_int_t n;
	dxf_int_t tmp_int;

	if (value == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	/* The ((n << k) >> k) expression performs two's complement */
	CHECKED_CALL_2(dx_read_unsigned_byte, context, &n);

	if (n < 0x80) {
		*value = (n << 25) >> 25;

		return true;
	}

	if (n < 0xC0) {
		dxf_uint_t tmp_byte;

		CHECKED_CALL_2(dx_read_unsigned_byte, context, &tmp_byte);

		*value = (((n << 8) | tmp_byte) << 18) >> 18;

		return true;
	}

	if (n < 0xE0) {
		dxf_int_t tmp_short;

		CHECKED_CALL_2(dx_read_unsigned_short, context, &tmp_short);

		*value = (((n << 16) | tmp_short) << 11) >> 11;

		return true;
	}

	if (n < 0xF0) {
		dxf_int_t tmp_byte;
		dxf_int_t tmp_short;

		CHECKED_CALL_2(dx_read_unsigned_byte, context, &tmp_byte);
		CHECKED_CALL_2(dx_read_unsigned_short, context, &tmp_short);

		*value = (((n << 24) | (tmp_byte << 16) | tmp_short) << 4) >> 4;

		return true;
	}

	if (n < 0xF8) {
		n = (n << 29) >> 29;
	} else if (n < 0xFC) {
		dxf_int_t tmp_byte;

		CHECKED_CALL_2(dx_read_unsigned_byte, context, &tmp_byte);

		n = (((n << 8) | tmp_byte) << 22) >> 22;
	} else if (n < 0xFE) {
		dxf_int_t tmp_short;

		CHECKED_CALL_2(dx_read_unsigned_short, context, &tmp_short);

		n = (((n << 16) | tmp_short) << 15) >> 15;
	} else if (n < 0xFF) {
		dxf_byte_t tmp_byte;
		dxf_int_t tmp_short;

		CHECKED_CALL_2(dx_read_byte, context, &tmp_byte);
		CHECKED_CALL_2(dx_read_unsigned_short, context, &tmp_short);

		n = (tmp_byte << 16) | tmp_short;
	} else {
		CHECKED_CALL_2(dx_read_int, context, &n);
	}

	CHECKED_CALL_2(dx_read_int, context, &tmp_int);

	*value = ((dxf_long_t)n << 32) | (tmp_int & 0xFFFFFFFFL);

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_read_byte_array (void* context, OUT dxf_byte_array_t* value) {
	dxf_byte_array_t buffer = DX_EMPTY_ARRAY;
	dxf_long_t buffer_length;
	bool failed = false;

	if (value == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	CHECKED_CALL_2(dx_read_compact_long, context, &buffer_length);

	if (buffer_length < -1 || buffer_length > INT_MAX) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	if (buffer_length == -1) {
		*value = buffer;
		return true;
	}

	if (buffer_length == 0) {
		*value = buffer;
		return true;
	}

	DX_ARRAY_RESERVE(buffer, dxf_byte_t, (int)buffer_length, failed);

	if (failed) {
		return false;
	}

	if (!dx_read_byte_buffer_segment(context, buffer.elements, (int)buffer_length, 0, (int)buffer_length)) {
		CHECKED_FREE(buffer.elements);
		return false;
	}

	*value = buffer;

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	UTF read operations implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_read_utf_char (void* context, OUT dxf_int_t* value) {
	dxf_byte_t c;

	if (value == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	CHECKED_CALL_2(dx_read_byte, context, &c);

	if (c >= 0) {
		*value = (dxf_char_t)c;

		return true;
	}

	if ((c & 0xE0) == 0xC0) {
		return dx_read_utf2(context, c, value);
	}

	if ((c & 0xF0) == 0xE0) {
		return dx_read_utf3(context, c, value);
	}

	if ((c & 0xF8) == 0xF0) {
		return dx_read_utf4(context, c, value);
	}

	return dx_set_error_code(dx_utfec_bad_utf_data_format_server);
}

/* -------------------------------------------------------------------------- */

bool dx_read_utf_char_array (void* context, OUT dxf_string_t* value) {
	dxf_long_t utflen;

	CHECKED_CALL_2(dx_read_compact_long, context, &utflen);

	if (utflen < -1 || utflen > INT_MAX) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	if (utflen == -1) {
		*value = NULL;

		return true;
	}

	return dx_read_utf_sequence(context, (int)utflen, true, value);
}

bool dx_read_utf_string (void* context, OUT dxf_string_t* value) {
	dxf_long_t utflen;

	CHECKED_CALL_2(dx_read_compact_long, context, &utflen);

	if (utflen < -1 || utflen > INT_MAX) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	if (utflen == -1) {
		*value = NULL;

		return true;
	}

	return dx_read_utf_sequence(context, (int)utflen, false, value);
}

void dx_get_raw(void* context, OUT dxf_ubyte_t** raw, OUT dxf_int_t* len) {
	int pos = CTX(context)->current_in_buffer_position;
	dxf_int_t count = CTX(context)->in_buffer_limit - pos;
	dxf_ubyte_t* buf = dx_calloc(count, sizeof(dxf_ubyte_t));
	int i = 0;
	for (; pos < CTX(context)->in_buffer_limit; pos++) {
		buf[i++] = CTX(context)->in_buffer[pos];
	}
	*raw = buf;
	*len = count;
}