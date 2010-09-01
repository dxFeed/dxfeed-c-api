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

#include "ParserCommon.h"
#include "BufferedInput.h"
#include "BufferedOutput.h"
#include "DXMemory.h"

#include "SymbolCodec.h"


/* -------------------------------------------------------------------------- */
/*
*	Implementation Details
*/
/* -------------------------------------------------------------------------- */

#define PENTA_LENGTH 128

/**
* Pentas for ASCII characters. Invalid pentas are set to 0.
*/
static dx_int_t pentas[PENTA_LENGTH];
static dx_char_t CHAR[1024];
/**
* Lengths (in bits) of pentas for ASCII characters. Invalid lengths are set to 64.
*/
static dx_int_t plength[PENTA_LENGTH];

/**
* ASCII characters for pentas. Invalid characters are set to 0.
*/
static dx_char_t characters[1024];

static dx_int_t wildcard_cipher;

/* -------------------------------------------------------------------------- */

void dx_init_penta(dx_int_t c, dx_int_t _penta, dx_int_t _plen) {
    pentas[c] = _penta;
    plength[c] = _plen;
    characters[_penta] = (dx_char_t)c;
}

/* -------------------------------------------------------------------------- */

enum dx_result_t dx_init_symbol_codec() {
    // initialization
    dx_int_t i = PENTA_LENGTH;
    dx_int_t penta = 0x03C0;
    wildcard_cipher = dx_encode_symbol_name(L"*");

    for (; --i >= 0;) {
        plength[i] = 64;
    }
    dx_memset(pentas, 0, PENTA_LENGTH * sizeof(dx_int_t));
    dx_memset(characters, 0, 1024 * sizeof(dx_char_t));


    for (i = (dx_int_t)(L'A'); i <= (dx_int_t)(L'Z'); ++i) {
        dx_init_penta(i, i - 'A' + 1, 5);
    }

    dx_init_penta((dx_int_t)(L'.'), 27, 5);
    dx_init_penta((dx_int_t)(L'/'), 28, 5);
    dx_init_penta((dx_int_t)(L'$'), 29, 5);
    for (i = 32; i <= 126; i++){
        if (pentas[i] == 0 && i != (dx_int_t)(L'\'') && i != (dx_int_t)(L'`')){
            dx_init_penta(i, penta++, 10);
        }
    }

    if (penta != 0x0400) {
        return setParseError(dx_pr_internal_error);
    }

    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

/**
*
* Encodes penta into cipher. Shall return 0 if encoding impossible.
* The specified penta must be valid (no more than 35 bits).
*/
dx_int_t dx_encode_penta(dx_long_t penta, dx_int_t plen) {
    dx_int_t c;
    if (plen <= 30) {
        return (dx_int_t)penta + 0x40000000;
    }
    c = (dx_int_t)((dx_ulong_t)penta >> 30);
    if (c == pentas[L'/']) // Also checks that plen == 35 (high bits == 0).
        return ((dx_int_t)penta & 0x3FFFFFFF) + 0x80000000;
    if (c == pentas[L'$']) // Also checks that plen == 35 (high bits == 0).
        return ((dx_int_t)penta & 0x3FFFFFFF) + 0xC0000000;
    return 0;
}

/* -------------------------------------------------------------------------- */

/**
* Converts penta into string.
* The specified penta must be valid (no more than 35 bits).
*/
dx_string_t dx_to_string(dx_long_t penta) {//TODO: errors handling
    dx_int_t plen = 0;
    dx_string_t chars;
    dx_int_t length = 0;

    while (((dx_ulong_t)penta >> plen) != 0) {
        plen += 5;
    }

    chars = dx_calloc(plen/5 + 1, sizeof(dx_char_t));
    while (plen > 0) {
        dx_int_t code;
        plen -= 5;
        code = (dx_int_t)((dx_ulong_t)penta >> plen) & 0x1F;
        if (code >= 30 && plen > 0) {
            plen -= 5;
            code = (dx_int_t)((dx_ulong_t)penta >> plen) & 0x3FF;
        }
        chars[length++] = characters[code];
    }

    chars[length] = 0; // end of string
    return chars;
}

/* -------------------------------------------------------------------------- */

/**
* Decodes cipher into penta code. The specified cipher must not be 0.
* The returning penta code must be valid (no more than 35 bits).
*/
enum dx_result_t dx_decode_cipher(dx_int_t cipher, OUT dx_long_t* res) {
    if (!res) {
        return setParseError(dx_pr_illegal_argument);
    };

    switch ((dx_uint_t)cipher >> 30) {
        case 0:
            return setParseError(dx_pr_illegal_argument);
        case 1:
            *res = cipher & 0x3FFFFFFF;
            break;
        case 2:
            *res = ((dx_long_t)pentas[L'/'] << 30) + (cipher & 0x3FFFFFFF);
            break;
        case 3:
            *res = ((dx_long_t)pentas[L'$'] << 30) + (cipher & 0x3FFFFFFF);
            break;
        default:
            // WTF ?
            return setParseError(dx_pr_internal_error); // 'int' has more than 32 bits.
    }

    return parseSuccessful();
}


/* -------------------------------------------------------------------------- */
/*
*	SymbolCodec interface implementation
*/
/* -------------------------------------------------------------------------- */

dx_int_t dx_get_codec_valid_chipher() {
    return 0xC0000000;
}

/* -------------------------------------------------------------------------- */

dx_int_t dx_encode_symbol_name (const dx_string_t symbol) {
    dx_long_t penta = 0;
    dx_int_t plen = 0;
    dx_int_t i;
    dx_int_t length;

    if (symbol == NULL) {
        return 0;
    }
    length = (dx_int_t)wcslen(symbol);
    if (length > 7) {
        return 0;
    }
    for (i = 0; i < length; ++i) {
        dx_int_t c = symbol[i];
        dx_int_t l;
        if (c >= 128)
            return 0;
        l = plength[c];
        penta = (penta << l) + pentas[c];
        plen += l;
    }
    if (plen > 35) {
        return 0;
    }
    return dx_encode_penta(penta, plen);
}
enum dx_result_t dx_decode_symbol_name(dx_int_t cipher, OUT dx_string_t* symbol){
	dx_long_t penta;

	if (cipher == 0 ){
		*symbol = NULL;
		return parseSuccessful();
	}
	if (dx_decode_cipher (cipher, & penta) != R_SUCCESSFUL)
		setParseError(dx_pr_undefined_symbol); //TODO: maybe change error code?

	*symbol = dx_to_string(penta);
	return parseSuccessful();

}
/* -------------------------------------------------------------------------- */

enum dx_result_t dx_codec_read_symbol(dx_char_t* buffer, dx_int_t buf_len, OUT dx_string_t* result, OUT dx_int_t* adv_res) {
    dx_int_t i;
    dx_long_t penta;
    dx_int_t tmp_int_1;
    dx_int_t tmp_int_2;
    dx_int_t plen;
    dx_int_t cipher;

    CHECKED_CALL(dx_read_unsigned_byte, &i);
    if (i < 0x80) { // 15-bit
        CHECKED_CALL(dx_read_unsigned_byte, &tmp_int_1);
        penta = (i << 8) + tmp_int_1;
    } else if (i < 0xC0) { // 30-bit
        CHECKED_CALL(dx_read_unsigned_byte, &tmp_int_1);
        CHECKED_CALL(dx_read_unsigned_short, &tmp_int_2);
        penta = ((i & 0x3F) << 24) + (tmp_int_1 << 16) + tmp_int_2;
    } else if (i < 0xE0) { // reserved (first diapason)
        return setParseError(dx_pr_reserved_bit_sequence);
    } else if (i < 0xF0) { // 20-bit
        CHECKED_CALL(dx_read_unsigned_short, &tmp_int_1);
        penta = ((i & 0x0F) << 16) + tmp_int_1;
    } else if (i < 0xF8) { // 35-bit
        if (dx_read_int(&tmp_int_1) != R_SUCCESSFUL) {
            return R_FAILED;
        }
        penta = ((long)(i & 0x07) << 32) + (tmp_int_1 & 0xFFFFFFFFL);
    } else if (i < 0xFC) { // reserved (second diapason)
        return setParseError(dx_pr_reserved_bit_sequence);
    } else if (i == 0xFC) { // UTF-8
        // todo This is inefficient, but currently UTF-8 is not used (compatibility mode). Shall be rewritten when UTF-8 become active.
        CHECKED_CALL(dx_read_utf_string, result);
        *adv_res = 0;
        return parseSuccessful();
    } else if (i == 0xFD) { // CESU-8
        dx_long_t length;
        dx_int_t k;
        dx_char_t* chars;
        CHECKED_CALL(dx_read_compact_long, &length);
        if (length < -1 || length > INT_MAX) {
            return setParseError(dx_pr_illegal_length);
        }
        if (length == -1) {
            *result = NULL;
            *adv_res = 0;
            return parseSuccessful();
        } else if (length == 0) {
            *result = dx_malloc(1);
            if (!(*result)) {
                return R_FAILED;
            }
            **result = 0; // empty string
            *adv_res = 0;
            return parseSuccessful();
        }

        // ???
        chars = length <= buf_len ? buffer : dx_calloc((size_t)(length + 1), sizeof(dx_char_t));

        for (k = 0; k < length; ++k) {
            dx_int_t codePoint;
            CHECKED_CALL(dx_read_utf_char, &codePoint);
            if (codePoint > 0xFFFF) {
                return setParseError(dx_pr_bad_utf_data_format);
            }
            chars[k] = (dx_char_t)codePoint;
        }
        if (length <= buf_len && (length & dx_get_codec_valid_chipher()) == 0) {
            *adv_res = (dx_int_t)length;
            return parseSuccessful();
        }
        *result = dx_calloc((size_t)(length + 1), sizeof(dx_char_t));
        if (!(*result)) {
            return R_FAILED;
        }
        wcsncpy(*result, chars, (dx_int_t)length);
        *adv_res = 0;
        return parseSuccessful();
    } else if (i == 0xFE) { // 0-bit
        penta = 0;
    } else { // void (null)
        *result = NULL;
        *adv_res = 0;
        return parseSuccessful();
    }
    plen = 0;
    while (((dx_ulong_t)penta >> plen) != 0) {
        plen += 5;
    }
    cipher = dx_encode_penta(penta, plen);
    if (cipher == 0) {
        *result = dx_to_string(penta); // Generally this is inefficient, but this use-case shall not actually happen.
    }
    *adv_res = cipher;
    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

enum dx_result_t dx_codec_write_symbol(dx_byte_t* buf, dx_int_t buf_len, dx_int_t pos, dx_int_t cipher, dx_string_t symbol) {
    if (cipher != 0) {
        dx_long_t penta;

        if (dx_decode_cipher(cipher, &penta) != R_SUCCESSFUL) {
            return R_FAILED;
        }

        dx_set_out_buffer(buf, buf_len);
        dx_set_out_buffer_position(pos);

        if (penta == 0) { // 0-bit
            CHECKED_CALL(dx_write_byte, 0xFE);
        } else if (penta < 0x8000L) { // 15-bit
            CHECKED_CALL(dx_write_short, (dx_short_t)penta);
        } else if (penta < 0x100000L) { // 20-bit
            CHECKED_CALL(dx_write_byte, 0xE0 | ((dx_uint_t)penta >> 16));
            CHECKED_CALL(dx_write_short, (dx_short_t)penta);
        } else if (penta < 0x40000000L) { // 30-bit
            CHECKED_CALL(dx_write_int, 0x80000000 | (dx_int_t)penta);
        } else if (penta < 0x0800000000L) { //35-bit
            CHECKED_CALL(dx_write_byte, 0xF0 | (dx_int_t)((dx_ulong_t)penta >> 32));
            CHECKED_CALL(dx_write_int, (dx_int_t)penta);
        } else { // more than 35-bit
            return setParseError(dx_pr_internal_error); // Penta has more than 35 bits.
        }
    } else {
        if (symbol != NULL) { // CESU-8
            dx_int_t length;
            dx_int_t i;
            CHECKED_CALL(dx_write_byte, 0xFD);

            length = (dx_int_t)wcslen(symbol);
            CHECKED_CALL(dx_write_compact_int, length);

            for (i = 0; i < length; ++i) {
                CHECKED_CALL(dx_write_utf_char, symbol[i]);
            }
        } else { // void (null)
            CHECKED_CALL(dx_write_byte, 0xFF);
        }
    }

    return parseSuccessful();
}
