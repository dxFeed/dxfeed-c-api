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

#ifndef BUFFERED_OUTPUT_H_INCLUDED
#define BUFFERED_OUTPUT_H_INCLUDED

#include "BufferedIOCommon.h"

/* -------------------------------------------------------------------------- */
/*
 *	Connection context functions
 */
/* -------------------------------------------------------------------------- */

void* dx_get_buffered_output_connection_context (dxf_connection_t connection);
bool dx_lock_buffered_output (void* context);
bool dx_unlock_buffered_output (void* context);

/* -------------------------------------------------------------------------- */
/*
 *	Buffer manipulators
 */
/* -------------------------------------------------------------------------- */

dxf_byte_t* dx_get_out_buffer (void* context);
int dx_get_out_buffer_length (void* context);
void dx_set_out_buffer (void* context, dxf_byte_t* new_buffer, int new_length);
int dx_get_out_buffer_position (void* context);
void dx_set_out_buffer_position (void* context, int new_position);

/*
 * Ensures that the byte array used for buffering has at least the specified capacity.
 * This method reallocates buffer if needed and copies content of old buffer into new one.
 * This method also sets the limit to the capacity.
 */
bool dx_ensure_capacity (void* context, int required_capacity);

/* -------------------------------------------------------------------------- */
/*
 *	Write operations
 */
/* -------------------------------------------------------------------------- */

bool dx_write_boolean (void* context, dxf_bool_t value);
bool dx_write_byte (void* context, dxf_byte_t value);
bool dx_write_short (void* context, dxf_short_t value);
bool dx_write_char (void* context, dxf_char_t value);
bool dx_write_int (void* context, dxf_int_t value);
bool dx_write_long (void* context, dxf_long_t value);
bool dx_write_float (void* context, dxf_float_t value);
bool dx_write_double (void* context, dxf_double_t value);
bool dx_write_byte_buffer (void* context, const dxf_char_t* value);
bool dx_write_char_buffer  (void* context, const dxf_char_t* value);
bool dx_write_utf (void* context, dxf_const_string_t value);

/* -------------------------------------------------------------------------- */
/*
 *	Compact write operations
 */
/* -------------------------------------------------------------------------- */

bool dx_write_compact_int (void* context, dxf_int_t value);
bool dx_write_compact_long (void* context, dxf_long_t value);

/*
 * Writes an array of bytes in a compact encapsulation format.
 * This method defines length as a number of bytes.
 *
 * @param  bytes - the byte array to be written
 * @param  size - the size of the byte array
 */
bool dx_write_byte_array (void* context, const dxf_byte_t* buffer, int buffer_size);

/* -------------------------------------------------------------------------- */
/*
 *	UTF write operations
 */
/* -------------------------------------------------------------------------- */

/*
 * Writes a Unicode code point in a UTF-8 format.
 * The surrogate code points are accepted and written in a CESU-8 format.
 *
 * @param code_point the code point to be written
 */
bool dx_write_utf_char (void* context, dxf_int_t code_point);

/*
 * Writes a string in a UTF-8 format with compact encapsulation.
 * Unpaired surrogate code points are accepted and written in a CESU-8 format.
 * This method defines length as a number of bytes.
 *
 * @param value the string to be written
 */
bool dx_write_utf_string (void* context, dxf_const_string_t value);

#endif /* BUFFERED_OUTPUT_H_INCLUDED */