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

#include "DXFeed.h"

#include "SymbolCodec.h"
#include "BufferedIOCommon.h"
#include "BufferedInput.h"
#include "BufferedOutput.h"
#include "DXMemory.h"
#include "DXAlgorithms.h"
#include "DXErrorHandling.h"

#include <limits.h>

/* -------------------------------------------------------------------------- */
/*
 *	Penta codec data
 */
/* -------------------------------------------------------------------------- */

#define PENTA_LENGTH 128

/*
 * Pentas for ASCII characters. Invalid pentas are set to 0.
 */
static dxf_int_t g_pentas[PENTA_LENGTH];

/*
 * Lengths (in bits) of pentas for ASCII characters. Invalid lengths are set to 64.
 */
static dxf_int_t g_penta_lengths[PENTA_LENGTH];

/*
 * ASCII characters for pentas. Invalid characters are set to 0.
 */
static dxf_char_t g_penta_characters[1024];

dxf_int_t g_wildcard_cipher;

/* -------------------------------------------------------------------------- */
/*
 *	Helper functions
 */
/* -------------------------------------------------------------------------- */

void dx_init_penta (dxf_int_t c, dxf_int_t penta, dxf_int_t penta_length) {
	g_penta_characters[penta] = (dxf_char_t)c;
	g_pentas[c] = penta;
	g_penta_lengths[c] = penta_length;
}

/* -------------------------------------------------------------------------- */
/*
 * Encodes penta into cipher. Shall return 0 if encoding impossible.
 * The specified penta must be valid (no more than 35 bits).
 */
dxf_int_t dx_encode_penta (dxf_long_t penta, dxf_int_t penta_length) {
	dxf_int_t c;

	if (penta_length <= 30) {
		return (dxf_int_t)penta + 0x40000000;
	}

	c = (dxf_int_t)((dxf_ulong_t)penta >> 30);

	if (c == g_pentas[L'/']) // Also checks that penta_length == 35 (high bits == 0).
		return ((dxf_int_t)penta & 0x3FFFFFFF) + 0x80000000;
	if (c == g_pentas[L'$']) // Also checks that penta_length == 35 (high bits == 0).
		return ((dxf_int_t)penta & 0x3FFFFFFF) + 0xC0000000;

	return 0;
}

/* -------------------------------------------------------------------------- */
/*
 * Converts penta into string.
 * The specified penta must be valid (no more than 35 bits).
 */
dxf_const_string_t dx_penta_to_string (dxf_long_t penta) {
	dxf_int_t penta_length = 0;
	dxf_string_t buffer;
	dxf_int_t length = 0;

	while (((dxf_ulong_t)penta >> penta_length) != 0) {
		penta_length += 5;
	}

	buffer = dx_create_string(penta_length / 5);

	if (buffer == NULL) {
		return NULL;
	}

	while (penta_length > 0) {
		dxf_int_t code;

		penta_length -= 5;
		code = (dxf_int_t)((dxf_ulong_t)penta >> penta_length) & 0x1F;

		if (code >= 30 && penta_length > 0) {
			penta_length -= 5;
			code = (dxf_int_t)((dxf_ulong_t)penta >> penta_length) & 0x3FF;
		}

		buffer[length++] = g_penta_characters[code];
	}

	return buffer;
}

/* -------------------------------------------------------------------------- */
/*
 * Decodes cipher into penta code. The specified cipher must not be 0.
 * The returning penta code must be valid (no more than 35 bits).
 */
bool dx_decode_cipher (dxf_int_t cipher, OUT dxf_long_t* res) {
	if (res == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	switch ((dxf_uint_t)cipher >> 30) {
	case 0:
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	case 1:
		*res = cipher & 0x3FFFFFFF;
		break;
	case 2:
		*res = ((dxf_long_t)g_pentas[L'/'] << 30) + (cipher & 0x3FFFFFFF);
		break;
	case 3:
		*res = ((dxf_long_t)g_pentas[L'$'] << 30) + (cipher & 0x3FFFFFFF);
		break;
	default:
		// ???
		return dx_set_error_code(dx_ec_internal_assert_violation); /* 'dx_int_t' has more than 32 bits. */
	}

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Symbol codec functions implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_init_symbol_codec (void) {
	dxf_int_t i = PENTA_LENGTH;
	dxf_int_t penta = 0x03C0;

	for (; --i >= 0;) {
		g_penta_lengths[i] = 64;
	}

	dx_memset(g_pentas, 0, PENTA_LENGTH * sizeof(dxf_int_t));
	dx_memset(g_penta_characters, 0, 1024 * sizeof(dxf_char_t));


	for (i = (dxf_int_t)(L'A'); i <= (dxf_int_t)(L'Z'); ++i) {
		dx_init_penta(i, i - 'A' + 1, 5);
	}

	dx_init_penta((dxf_int_t)(L'.'), 27, 5);
	dx_init_penta((dxf_int_t)(L'/'), 28, 5);
	dx_init_penta((dxf_int_t)(L'$'), 29, 5);

	for (i = 32; i <= 126; i++){
		if (g_pentas[i] == 0 && i != (dxf_int_t)(L'\'') && i != (dxf_int_t)(L'`')){
			dx_init_penta(i, penta++, 10);
		}
	}

	if (penta != 0x0400) {
		return dx_set_error_code(dx_ec_internal_assert_violation);
	}

	g_wildcard_cipher = dx_encode_symbol_name(L"*");

	return true;
}

/* -------------------------------------------------------------------------- */

dxf_int_t dx_get_codec_valid_cipher (void) {
	return 0xC0000000;
}

/* -------------------------------------------------------------------------- */

dxf_int_t dx_encode_symbol_name (dxf_const_string_t symbol) {
	dxf_long_t penta = 0;
	dxf_int_t penta_length = 0;
	size_t i = 0;
	size_t length;

	if (symbol == NULL) {
		return 0;
	}

	length = dx_string_length(symbol);

	if (length > 7) {
		return 0;
	}

	for (; i < length; ++i) {
		dxf_int_t c = symbol[i];
		dxf_int_t l;

		if (c >= 128) {
			return 0;
		}

		l = g_penta_lengths[c];

		penta = (penta << l) + g_pentas[c];
		penta_length += l;
	}

	if (penta_length > 35) {
		return 0;
	}

	return dx_encode_penta(penta, penta_length);
}

/* -------------------------------------------------------------------------- */

bool dx_decode_symbol_name (dxf_int_t cipher, OUT dxf_const_string_t* symbol) {
	dxf_long_t penta;
	dxf_const_string_t str;

	if (cipher == 0){
		*symbol = NULL;

		return true;
	}

	CHECKED_CALL_2(dx_decode_cipher, cipher, &penta);

	str = dx_penta_to_string(penta);

	if (str == NULL) {
		return false;
	}

	*symbol = str;

	return true;

}
/* -------------------------------------------------------------------------- */

bool dx_codec_read_symbol (void* bicc, dxf_char_t* buffer, int buffer_length, OUT dxf_string_t* result,
						OUT dxf_int_t* cipher_result, OUT dxf_event_flags_t* flags,
						OUT dxf_event_flags_t* mru_event_flags) {
	dxf_int_t i;
	dxf_long_t penta;
	dxf_int_t tmp_int_1;
	dxf_int_t tmp_int_2;
	dxf_int_t plen;
	dxf_int_t cipher;
	int event_flags_pos = 0;
	dxf_int_t event_flags_bytes = 0;

	*flags = 0;

	while (true) {
		CHECKED_CALL_2(dx_read_unsigned_byte, bicc, &i);

		if (i < 0x80) { // 15-bit
			CHECKED_CALL_2(dx_read_unsigned_byte, bicc, &tmp_int_1);

			penta = (i << 8) + tmp_int_1;
		}
		else if (i < 0xC0) { // 30-bit
			CHECKED_CALL_2(dx_read_unsigned_byte, bicc, &tmp_int_1);
			CHECKED_CALL_2(dx_read_unsigned_short, bicc, &tmp_int_2);

			penta = ((i & 0x3F) << 24) + (tmp_int_1 << 16) + tmp_int_2;
		}
		else if (i < 0xE0) { // reserved (first diapason)
			return dx_set_error_code(dx_pcec_reserved_bit_sequence);
		}
		else if (i < 0xF0) { // 20-bit
			CHECKED_CALL_2(dx_read_unsigned_short, bicc, &tmp_int_1);

			penta = ((i & 0x0F) << 16) + tmp_int_1;
		}
		else if (i < 0xF8) { // 35-bit
			CHECKED_CALL_2(dx_read_int, bicc, &tmp_int_1);

			penta = ((dxf_long_t)(i & 0x07) << 32) + (tmp_int_1 & 0xFFFFFFFFL);
		}
		else if (i == 0xF8) { // mru event flags
			if (event_flags_bytes > 0)
				return dx_set_error_code(dx_pcec_invalid_event_flag);
			*flags = *mru_event_flags;
			event_flags_bytes = 1;
			continue; // read next byte
		}
		else if (i == 0xF9) { // new event flags
			if (event_flags_bytes > 0)
				return dx_set_error_code(dx_pcec_invalid_event_flag);
			event_flags_pos = dx_get_in_buffer_position(bicc);
			CHECKED_CALL_2(dx_read_compact_int, bicc, mru_event_flags);
			*flags = *mru_event_flags;
			event_flags_bytes = dx_get_in_buffer_position(bicc) - event_flags_pos;
			continue; // read next byte
		}
		else if (i < 0xFC) { // reserved (second diapason)
			return dx_set_error_code(dx_pcec_reserved_bit_sequence);
		}
		else if (i == 0xFC) { // UTF-8
			// todo This is inefficient, but currently UTF-8 is not used (compatibility mode). Shall be rewritten when UTF-8 become active.
			CHECKED_CALL_2(dx_read_utf_string, bicc, result);

			*cipher_result = 0;

			return true;
		}
		else if (i == 0xFD) { // CESU-8
			dxf_long_t length;
			dxf_int_t k;
			dxf_char_t* symbol_buffer;

			CHECKED_CALL_2(dx_read_compact_long, bicc, &length);

			if (length < 0 || length > INT_MAX) {
				return dx_set_error_code(dx_pcec_invalid_symbol_length);
			}
			if (length == 0) {
				*result = dx_malloc(1);

				if (*result == NULL) {
					return false;
				}

				**result = 0; // empty string
				*cipher_result = 0;

				return true;
			}

			/* storing symbol as a string */
			symbol_buffer = length <= buffer_length ? buffer : dx_create_string((int)length);

			if (symbol_buffer == NULL) {
				return false;
			}

			for (k = 0; k < length; ++k) {
				dxf_int_t code_point;

				if (!dx_read_utf_char(bicc, &code_point)) {
					if (length > buffer_length)
						dx_free(symbol_buffer);

					return false;
				}

				if (code_point > 0xFFFF) {
					if (length > buffer_length)
						dx_free(symbol_buffer);

					return dx_set_error_code(dx_utfec_bad_utf_data_format_server);
				}

				symbol_buffer[k] = (dxf_char_t)code_point;
			}

			if (length <= buffer_length && (length & dx_get_codec_valid_cipher()) == 0) {
				*cipher_result = (dxf_int_t)length;

				return true;
			}

			*result = dx_create_string_src_len(symbol_buffer, (int)length);

			if (*result == NULL) {
				return false;
			}

			*cipher_result = 0;

			return true;
		}
		else if (i == 0xFE) { // 0-bit
			penta = 0;
		}
		else { // void (null)
			*result = NULL;
			*cipher_result = 0;

			return true;
		}

		plen = 0;

		while (((dxf_ulong_t)penta >> plen) != 0) {
			plen += 5;
		}

		cipher = dx_encode_penta(penta, plen);

		if (cipher == 0) {
			dxf_string_t str = (dxf_string_t)dx_penta_to_string(penta);

			if (str == NULL) {
				return false;
			}

			*result = str; // Generally this is inefficient, but this use-case shall not actually happen.
		}

		*cipher_result = cipher;

		return true;
	}
	return false;
}

/* -------------------------------------------------------------------------- */

bool dx_codec_write_symbol (void* bocc, dxf_int_t cipher, dxf_const_string_t symbol) {
	if (cipher != 0) {
		dxf_long_t penta;

		CHECKED_CALL_2(dx_decode_cipher, cipher, &penta);

		if (penta == 0) { // 0-bit
			CHECKED_CALL_2(dx_write_byte, bocc, 0xFE);
		} else if (penta < 0x8000L) { // 15-bit
			CHECKED_CALL_2(dx_write_short, bocc, (dxf_short_t)penta);
		} else if (penta < 0x100000L) { // 20-bit
			CHECKED_CALL_2(dx_write_byte, bocc, 0xE0 | ((dxf_uint_t)penta >> 16));
			CHECKED_CALL_2(dx_write_short, bocc, (dxf_short_t)penta);
		} else if (penta < 0x40000000L) { // 30-bit
			CHECKED_CALL_2(dx_write_int, bocc, 0x80000000 | (dxf_int_t)penta);
		} else if (penta < 0x0800000000L) { //35-bit
			CHECKED_CALL_2(dx_write_byte, bocc, 0xF0 | (dxf_int_t)((dxf_ulong_t)penta >> 32));
			CHECKED_CALL_2(dx_write_int, bocc, (dxf_int_t)penta);
		} else { // more than 35-bit
			return dx_set_error_code(dx_ec_internal_assert_violation); // Penta has more than 35 bits.
		}
	} else {
		if (symbol != NULL) { // CESU-8
			dxf_int_t length;
			int i;

			CHECKED_CALL_2(dx_write_byte, bocc, 0xFD);

			length = (dxf_int_t)dx_string_length(symbol);

			CHECKED_CALL_2(dx_write_compact_int, bocc, length);

			for (i = 0; i < length; ++i) {
				CHECKED_CALL_2(dx_write_utf_char, bocc, symbol[i]);
			}
		} else { // void (null)
			CHECKED_CALL_2(dx_write_byte, bocc, 0xFF);
		}
	}

	return true;
}