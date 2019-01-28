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

#ifndef BUFFERED_INPUT_H_INCLUDED
#define BUFFERED_INPUT_H_INCLUDED

#include "BufferedIOCommon.h"

/* -------------------------------------------------------------------------- */
/*
 *	Connection context functions
 */
/* -------------------------------------------------------------------------- */

void* dx_get_buffered_input_connection_context (dxf_connection_t connection);

/* -------------------------------------------------------------------------- */
/*
 *	Buffer manipulators
 */
/* -------------------------------------------------------------------------- */

dxf_byte_t* dx_get_in_buffer (void* context);
int dx_get_in_buffer_length (void* context);
void dx_set_in_buffer (void* context, dxf_byte_t* new_buffer, int new_length);
int dx_get_in_buffer_position (void* context);
void dx_set_in_buffer_position (void* context, int new_position);
int dx_get_in_buffer_limit (void* context);
void dx_set_in_buffer_limit (void* context, int new_limit);

/* -------------------------------------------------------------------------- */
/*
 *	Read operations
 */
/* -------------------------------------------------------------------------- */

bool dx_read_boolean (void* context, OUT dxf_bool_t* value);
bool dx_read_byte (void* context, OUT dxf_byte_t* value);
bool dx_read_unsigned_byte (void* context, OUT dxf_uint_t* value);
bool dx_read_short (void* context, OUT dxf_short_t* value);
bool dx_read_unsigned_short (void* context, OUT dxf_uint_t* value);
bool dx_read_char (void* context, OUT dxf_char_t* value);
bool dx_read_int (void* context, OUT dxf_int_t* value);
bool dx_read_long (void* context, OUT dxf_long_t* value);
bool dx_read_float (void* context, OUT dxf_float_t* value);
bool dx_read_double (void* context, OUT dxf_double_t* value);
bool dx_read_line (void* context, OUT dxf_string_t* value);
/*
bool dx_read_utf (void* context, OUT dxf_string_t* value);
 */

/* -------------------------------------------------------------------------- */
/*
 *	Compact read operations
 */
/* -------------------------------------------------------------------------- */

/*
 * Reads an dx_int_t value in a compact format.
 * If actual encoded value does not fit into an dx_int_t data type,
 * then it is truncated to dx_int_t value (only lower 32 bits are returned);
 * the number is read entirely in this case.
 *
 * @param value - the dx_int_t value read
 */
bool dx_read_compact_int (void* context, OUT dxf_int_t* value);

/*
 * Reads a dx_long_t value in a compact format.
 *
 * @param value - the dx_long_t value read
 */
bool dx_read_compact_long (void* context, OUT dxf_long_t* value);

/*
 * Reads an array of bytes in a compact encapsulation format.
 * This method defines length as a number of bytes.
 *
 * @param value - the byte array read
 *
 * Note: you must to free returned byte array itself.
 */
bool dx_read_byte_array (void* context, OUT dxf_byte_array_t* value);

/* -------------------------------------------------------------------------- */
/*
 *	UTF read operations
 */
/* -------------------------------------------------------------------------- */

/*
 * Reads Unicode code point in a UTF-8 format.
 * Overlong UTF-8 and CESU-8-encoded surrogates are accepted and read without errors.
 *
 * @param value - the Unicode code point read
 */
bool dx_read_utf_char (void* context, OUT dxf_int_t* value);

/*
 * Reads Unicode string in a UTF-8 format with compact encapsulation.
 * Overlong UTF-8 and CESU-8-encoded surrogates are accepted and read without errors.
 * This method defines length as a number of characters.
 *
 * @param value - the Unicode string read
 */
bool dx_read_utf_char_array (void* context, OUT dxf_string_t* value);

/*
 * Reads Unicode string in a UTF-8 format with compact encapsulation.
 * Overlong UTF-8 and CESU-8-encoded surrogates are accepted and read without errors.
 * This method defines length as a number of bytes.
 *
 * @param value - the Unicode string read
 */
bool dx_read_utf_string (void* context, OUT dxf_string_t* value);

void dx_get_raw(void* context, OUT dxf_ubyte_t** raw, OUT dxf_int_t* len);

#endif /* BUFFERED_INPUT_H_INCLUDED */