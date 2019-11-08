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

/*
 * The PentaCodec performs symbol coding and serialization using
 * extensible 5-bit encoding. The eligible characters are assigned penta codes
 * (either single 5-bit or double 10-bit) according to the following table:
 *
 * 'A' to 'Z'                 - 5-bit pentas from 1 to 26
 * '.'                        - 5-bit penta 27
 * '/'                        - 5-bit penta 28
 * '$'                        - 5-bit penta 29
 * ''' and '`'                - none (ineligible characters)
 * ' ' to '~' except above    - 10-bit pentas from 960 to 1023
 * all other                  - none (ineligible characters)
 *
 * The 5-bit penta 0 represents empty space and is eligible only at the start.
 * The 5-bit pentas 30 and 31 are used as a transition mark to switch to 10-bit pentas.
 * The 10-bit pentas from 0 to 959 do not exist as they collide with 5-bit pentas.
 *
 * The individual penta codes for character sequence are packed into 64-bit value
 * from high bits to low bits aligned to the low bits. This allows representation
 * of up to 35-bit penta-coded character sequences. If some symbol contains one or
 * more ineligible characters or does not fit into 35-bit penta, then it is not
 * subject to penta-coding and is left as a string. The resulting penta-coded value
 * can be serialized as defined below or encoded into the 32-bit cipher if possible.
 * Please note that penta code 0 is a valid code as it represents empty character
 * sequence - do not confuse it wich cipher value 0, which means 'void' or 'null'.
 *
 * The following table defines used serial format (the first byte is given in bits
 * with 'x' representing payload bit; the remaining bytes are given in bit count):
 *
 * 0xxxxxxx  8x - for 15-bit pentas
 * 10xxxxxx 24x - for 30-bit pentas
 * 110xxxxx ??? - reserved (payload TBD)
 * 1110xxxx 16x - for 20-bit pentas
 * 11110xxx 32x - for 35-bit pentas
 * 111110xx ??? - reserved (payload TBD)
 * 11111100 zzz - for UTF-8 string with length in bytes
 * 11111101 zzz - for CESU-8 string with length in characters
 * 11111110     - for 0-bit penta (empty symbol)
 * 11111111     - for void (null)
 *
 * See "http://www.unicode.org/unicode/reports/tr26/tr26-2.html"
 * for format basics and writeUTFString and writeCharArray
 * for details of string encoding.
 */

#ifndef SYMBOL_CODEC_H_INCLUDED
#define SYMBOL_CODEC_H_INCLUDED

#include "PrimitiveTypes.h"
#include "DXTypes.h"

extern dxf_int_t g_wildcard_cipher;

/* -------------------------------------------------------------------------- */
/*
 *	Symbol codec functions
 */
/* -------------------------------------------------------------------------- */

/*
 *	Initialize of symbol codec

 *  Must be called once per application, no need to perform per-thread initialization
 */
bool dx_init_symbol_codec (void);

/* -------------------------------------------------------------------------- */
/*
 *  valid cipher defines range of valid encoded ciphers.
 */
dxf_int_t dx_get_codec_valid_cipher (void);

/* -------------------------------------------------------------------------- */
/*
 * Returns encoded cipher for specified symbol.
 * Returns 0 if specified symbol is NULL or if encoding is impossible.
 */
dxf_int_t dx_encode_symbol_name (dxf_const_string_t symbol);

/* -------------------------------------------------------------------------- */
/*
 * Restores symbol for specified cipher.
 * Restored symbol is NULL if specified cipher is 0.
 */
bool dx_decode_symbol_name (dxf_int_t cipher, OUT dxf_const_string_t* symbol);

/* -------------------------------------------------------------------------- */
/*
 * Reads symbol from connection context input buffer and returns it in several ways depending on its encodability and length.
 * If this method produces:
 *
 *      valid cipher - this is an encoded cipher of the symbol; 'buffer' content is destroyed; 'result' array is unchanged;
 *      positive number - this is a length of the symbol that is stored in 'buffer' starting at zero offset; 'result' array is unchanged;
 *      0 - symbol is stored in the 'result' array at zero index; 'buffer' content is destroyed;
 *
 * Both 'buffer' and 'result' parameters shall be _local_ to the ongoing operation;
 * they are used for local storage and to return result to the caller; they are not used by the codec after that.
 * The 'buffer' shall be large enough to accomodate common short symbols, for example 64
 * characters. The 'result' shall have length 1 because only first element is ever used.
 *
 */
bool dx_codec_read_symbol(void* bicc, dxf_char_t* buffer, int buffer_length, OUT dxf_string_t* result,
						OUT dxf_int_t* cipher_result, OUT dxf_event_flags_t* flags,
						OUT dxf_event_flags_t* mru_event_flags);

/* -------------------------------------------------------------------------- */
/*
 *	Writes the symbol into the buffered output connection context buffer
 */
bool dx_codec_write_symbol (void* bocc, dxf_int_t cipher, dxf_const_string_t symbol);

#endif /* SYMBOL_CODEC_H_INCLUDED */