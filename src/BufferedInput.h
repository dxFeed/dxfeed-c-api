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

//#define checkedCall(x) if ((x) != R_SUCCESSFUL) return R_FAILED;

extern dx_byte_t* inBuffer;
extern dx_int_t   inBufferLength;
extern dx_int_t   inBufferLimit;
extern dx_int_t   currentInBufferPosition;

void dx_set_in_buffer( dx_byte_t*, dx_int_t length );
void dx_set_in_buffer_position( dx_int_t pos);
void dx_set_in_buffer_limit( dx_int_t limit );
dx_int_t dx_get_in_buffer_position();
dx_int_t dx_get_in_buffer_limit();
dx_byte_t* dx_get_in_buffer(OUT dx_int_t* size);

//enum DXResult needData(int length);
dx_int_t skip(dx_int_t n);
//enum DXResult readFully(dx_byte_t* b, dx_int_t bLength, dx_int_t off, dx_int_t len);

enum dx_result_t dx_read_boolean       ( OUT dx_bool_t* );
enum dx_result_t dx_read_byte          ( OUT dx_byte_t* );
enum dx_result_t dx_read_unsigned_byte  ( OUT dx_int_t* );
enum dx_result_t dx_read_short         ( OUT dx_short_t* );
enum dx_result_t dx_read_unsigned_short ( OUT dx_int_t* );
enum dx_result_t dx_read_char          ( OUT dx_char_t* );
enum dx_result_t dx_read_int           ( OUT dx_int_t* );
enum dx_result_t dx_read_long          ( OUT dx_long_t* );
enum dx_result_t dx_read_float         ( OUT dx_float_t* );
enum dx_result_t dx_read_double        ( OUT dx_double_t* );
enum dx_result_t dx_read_line          ( OUT dx_char_t** );
enum dx_result_t dx_read_utf           ( OUT dx_char_t** );
//Object readObject()

// ========== Compact API ==========

/**
* Reads an dx_int_t value in a compact format.
* If actual encoded value does not fit into an dx_int_t data type,
* then it is truncated to dx_int_t value (only lower 32 bits are returned);
* the number is read entirely in this case.
*
* @param output parameter - the dx_int_t value read
*/
enum dx_result_t dx_read_compact_int( OUT dx_int_t* );

/**
* Reads a dx_long_t value in a compact format.
*
* @param output parameter - the dx_long_t value read
*/
enum dx_result_t dx_read_compact_long( OUT dx_long_t* );

/**
* Reads an array of bytes in a compact encapsulation format.
* This method defines length as a number of bytes.
*
* @param output parameter - the byte array read
*/
enum dx_result_t dx_read_byte_array( OUT dx_byte_t** );

// ========== UTF API ==========

/**
* Reads Unicode code point in a UTF-8 format.
* Overlong UTF-8 and CESU-8-encoded surrogates are accepted and read without errors.
*
* @param output parameter - the Unicode code point read
*/
enum dx_result_t dx_read_utf_char( OUT dx_int_t* );

/**
* Reads Unicode string in a UTF-8 format with compact encapsulation.
* Overlong UTF-8 and CESU-8-encoded surrogates are accepted and read without errors.
* This method defines length as a number of bytes.
*
* @param output parameter - the Unicode string read
*/
enum dx_result_t dx_read_utf_string( OUT dx_string_t* );




#endif // BUFFERED_INPUT