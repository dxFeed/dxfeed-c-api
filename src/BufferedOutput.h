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

extern jByte* outBuffer;
extern jInt   outBufferLength;
extern jInt   currentOutBufferPosition;

void setOutBuffer( jByte* );

//enum DXResult write(jInt b);
enum dx_result_t write(const jByte* b, jInt bLen, jInt off, jInt len);

enum dx_result_t writeBoolean        ( jBool );
enum dx_result_t writeByte           ( jByte );
enum dx_result_t writeShort          ( jShort );
enum dx_result_t writeChar           ( jChar );
enum dx_result_t writeInt            ( jInt );
enum dx_result_t writeLong           ( jLong );
enum dx_result_t writeFloat          ( jFloat );
enum dx_result_t writeDouble         ( jDouble );
enum dx_result_t writeBytes          ( const jChar* );
enum dx_result_t writeChars          ( const jChar* );
enum dx_result_t writeUTF            ( const dx_string );

// ========== Compact API ==========

/**
* Writes an jInt value in a compact format.
*
* @param val - value to be written
*/
enum dx_result_t writeCompactInt     ( jInt val );

/**
* Writes a jLong value in a compact format.
*
* @param v - value to be written
*/
enum dx_result_t writeCompactLong    ( jLong v );

/**
* Writes an array of bytes in a compact encapsulation format.
* This method defines length as a number of bytes.
*
* @param  bytes - the byte array to be written
* @param  size - the size of the byte array
*/
enum dx_result_t writeByteArray      ( const jByte* bytes, jInt size );


// ========== UTF API ==========

/**
* Writes a Unicode code point in a UTF-8 format.
* The surrogate code points are accepted and written in a CESU-8 format.
*
* @param codePoint the code point to be written
*/
enum dx_result_t writeUTFChar        ( jInt codePoint );

/**
* Writes a string in a UTF-8 format with compact encapsulation.
* Unpaired surrogate code points are accepted and written in a CESU-8 format.
* This method defines length as a number of bytes.
*
* @param str the string to be written
*/
enum dx_result_t writeUTFString      ( const dx_string_t str );

/**
* Ensures that the byte array used for buffering has at least the specified capacity.
* This method reallocates buffer if needed and copies content of old buffer into new one.
* This method also sets the limit to the capacity.
*/
enum dx_result_t ensureCapacity (int requiredCapacity);


#endif // BUFFERED_OUTPUT
