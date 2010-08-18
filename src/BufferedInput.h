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

void setInBuffer( dx_byte_t*, dx_int_t length );
void setInBufferPosition( dx_int_t pos);
void setInBufferLimit( dx_int_t limit );
dx_int_t getInBufferPosition();
dx_int_t getInBufferLimit();

//enum DXResult needData(int length);
dx_int_t skip(dx_int_t n);
//enum DXResult readFully(dx_byte_t* b, dx_int_t bLength, dx_int_t off, dx_int_t len);

enum dx_result_t readBoolean       ( OUT dx_bool_t* );
enum dx_result_t readByte          ( OUT dx_byte_t* );
enum dx_result_t readUnsignedByte  ( OUT dx_int_t* );
enum dx_result_t readShort         ( OUT dx_short_t* );
enum dx_result_t readUnsignedShort ( OUT dx_int_t* );
enum dx_result_t readChar          ( OUT dx_char_t* );
enum dx_result_t readInt           ( OUT dx_int_t* );
enum dx_result_t readLong          ( OUT dx_long_t* );
enum dx_result_t readFloat         ( OUT dx_float_t* );
enum dx_result_t readDouble        ( OUT dx_double_t* );
enum dx_result_t readLine          ( OUT dx_char_t** );
enum dx_result_t readUTF           ( OUT dx_char_t** );
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
enum dx_result_t readCompactInt( OUT dx_int_t* );

/**
* Reads a dx_long_t value in a compact format.
*
* @param output parameter - the dx_long_t value read
*/
enum dx_result_t readCompactLong( OUT dx_long_t* );

/**
* Reads an array of bytes in a compact encapsulation format.
* This method defines length as a number of bytes.
*
* @param output parameter - the byte array read
*/
enum dx_result_t readByteArray( OUT dx_byte_t** );

// ========== UTF API ==========

/**
* Reads Unicode code point in a UTF-8 format.
* Overlong UTF-8 and CESU-8-encoded surrogates are accepted and read without errors.
*
* @param output parameter - the Unicode code point read
*/
enum dx_result_t readUTFChar( OUT dx_int_t* );

/**
* Reads Unicode string in a UTF-8 format with compact encapsulation.
* Overlong UTF-8 and CESU-8-encoded surrogates are accepted and read without errors.
* This method defines length as a number of bytes.
*
* @param output parameter - the Unicode string read
*/
enum dx_result_t readUTFString( OUT dx_string_t* );




#endif // BUFFERED_INPUT