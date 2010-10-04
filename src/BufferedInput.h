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

#ifndef BUFFERED_INPUT
#define BUFFERED_INPUT

#include "ParserCommon.h"

void dx_set_in_buffer (void* context, dx_byte_t* buf, dx_int_t length);
void dx_set_in_buffer_position (void* context, dx_int_t pos);
void dx_set_in_buffer_limit (void* context, dx_int_t limit);
dx_int_t dx_get_in_buffer_position (void* context);
dx_int_t dx_get_in_buffer_limit (void* context);
dx_byte_t* dx_get_in_buffer (void* context, OUT dx_int_t* size);

dx_int_t skip (void* context, dx_int_t n);

dx_result_t dx_read_boolean (void* context, OUT dx_bool_t*);
dx_result_t dx_read_byte (void* context, OUT dx_byte_t*);
dx_result_t dx_read_unsigned_byte (void* context, OUT dx_uint_t*);
dx_result_t dx_read_short (void* context, OUT dx_short_t*);
dx_result_t dx_read_unsigned_short (void* context, OUT dx_uint_t*);
dx_result_t dx_read_char (void* context, OUT dx_char_t*);
dx_result_t dx_read_int (void* context, OUT dx_int_t*);
dx_result_t dx_read_long (void* context, OUT dx_long_t*);
dx_result_t dx_read_float (void* context, OUT dx_float_t*);
dx_result_t dx_read_double (void* context, OUT dx_double_t*);
dx_result_t dx_read_line (void* context, OUT dx_char_t**);
dx_result_t dx_read_utf (void* context, OUT dx_char_t**);

// ========== Compact API ==========

/**
* Reads an dx_int_t value in a compact format.
* If actual encoded value does not fit into an dx_int_t data type,
* then it is truncated to dx_int_t value (only lower 32 bits are returned);
* the number is read entirely in this case.
*
* @param output parameter - the dx_int_t value read
*/
dx_result_t dx_read_compact_int (void* context, OUT dx_int_t*);

/**
* Reads a dx_long_t value in a compact format.
*
* @param output parameter - the dx_long_t value read
*/
dx_result_t dx_read_compact_long (void* context, OUT dx_long_t*);

/**
* Reads an array of bytes in a compact encapsulation format.
* This method defines length as a number of bytes.
*
* @param output parameter - the byte array read
*/
dx_result_t dx_read_byte_array (void* context, OUT dx_byte_t**);

// ========== UTF API ==========

/**
* Reads Unicode code point in a UTF-8 format.
* Overlong UTF-8 and CESU-8-encoded surrogates are accepted and read without errors.
*
* @param output parameter - the Unicode code point read
*/
dx_result_t dx_read_utf_char (void* context, OUT dx_int_t*);

/**
* Reads Unicode string in a UTF-8 format with compact encapsulation.
* Overlong UTF-8 and CESU-8-encoded surrogates are accepted and read without errors.
* This method defines length as a number of bytes.
*
* @param output parameter - the Unicode string read
*/
dx_result_t dx_read_utf_string (void* context, OUT dx_string_t*);

/* -------------------------------------------------------------------------- */
/*
 *	Connection context functions
 */
/* -------------------------------------------------------------------------- */

void* dx_get_buffered_input_connection_context (dxf_connection_t connection);

#endif // BUFFERED_INPUT