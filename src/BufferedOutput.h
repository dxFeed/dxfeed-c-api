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

#ifndef BUFFERED_OUTPUT
#define BUFFERED_OUTPUT

#include "ParserCommon.h"

//extern dx_byte_t* out_buffer;
//extern dx_int_t   out_buffer_length;
//extern dx_int_t   current_out_buffer_position;

/* -------------------------------------------------------------------------- */
void dx_set_out_buffer( dx_byte_t* buf, dx_int_t buf_len);

/* -------------------------------------------------------------------------- */

dx_byte_t* dx_get_out_buffer(OUT dx_int_t* size);

/* -------------------------------------------------------------------------- */

dx_int_t dx_get_out_buffer_position();

/* -------------------------------------------------------------------------- */

void dx_set_out_buffer_position(dx_int_t pos);

/* -------------------------------------------------------------------------- */

//enum DXResult write(dx_int_t b);
dx_result_t write(const dx_byte_t* b, dx_int_t bLen, dx_int_t off, dx_int_t len);

dx_result_t dx_write_boolean( dx_bool_t );
dx_result_t dx_write_byte   ( dx_byte_t );
dx_result_t dx_write_short  ( dx_short_t );
dx_result_t dx_write_char   ( dx_char_t );
dx_result_t dx_write_int    ( dx_int_t );
dx_result_t dx_write_long   ( dx_long_t );
dx_result_t dx_write_float  ( dx_float_t );
dx_result_t dx_write_double ( dx_double_t );
dx_result_t dx_write_bytes  ( const dx_char_t* );
dx_result_t dx_write_chars  ( const dx_char_t* );
dx_result_t dx_write_utf    ( dx_const_string_t );

// ========== Compact API ==========

/**
* Writes an dx_int_t value in a compact format.
*
* @param val - value to be written
*/
dx_result_t dx_write_compact_int     ( dx_int_t val );

/**
* Writes a dx_long_t value in a compact format.
*
* @param v - value to be written
*/
dx_result_t dx_write_compact_long    ( dx_long_t v );

/**
* Writes an array of bytes in a compact encapsulation format.
* This method defines length as a number of bytes.
*
* @param  bytes - the byte array to be written
* @param  size - the size of the byte array
*/
dx_result_t dx_write_byte_array      ( const dx_byte_t* bytes, dx_int_t size );


// ========== UTF API ==========

/**
* Writes a Unicode code point in a UTF-8 format.
* The surrogate code points are accepted and written in a CESU-8 format.
*
* @param codePoint the code point to be written
*/
dx_result_t dx_write_utf_char        ( dx_int_t codePoint );

/**
* Writes a string in a UTF-8 format with compact encapsulation.
* Unpaired surrogate code points are accepted and written in a CESU-8 format.
* This method defines length as a number of bytes.
*
* @param str the string to be written
*/
dx_result_t dx_write_utf_string      (dx_const_string_t str );

/**
* Ensures that the byte array used for buffering has at least the specified capacity.
* This method reallocates buffer if needed and copies content of old buffer into new one.
* This method also sets the limit to the capacity.
*/
dx_result_t dx_ensure_capacity (int requiredCapacity);


#endif // BUFFERED_OUTPUT
