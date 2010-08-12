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

extern jByte* inBuffer;
extern jInt   inBufferLength;
extern jInt   currentInBufferPosition;

void setBuffer( jByte* );

//enum DXResult needData(int length);
jInt skip(jInt n);
//enum DXResult readFully(jByte* b, jInt bLength, jInt off, jInt len);

enum DXResult readBoolean       ( OUT jBool* );
enum DXResult readByte          ( OUT jByte* );
enum DXResult readUnsignedByte  ( OUT jInt* );
enum DXResult readShort         ( OUT jShort* );
enum DXResult readUnsignedShort ( OUT jInt* );
enum DXResult readChar          ( OUT jChar* );
enum DXResult readInt           ( OUT jInt* );
enum DXResult readLong          ( OUT jLong* );
enum DXResult readFloat         ( OUT jFloat* );
enum DXResult readDouble        ( OUT jDouble* );
enum DXResult readLine          ( OUT jChar** );
enum DXResult readUTF           ( OUT jChar** );
//Object readObject()

// ========== Compact API ==========

/**
* Reads an jInt value in a compact format.
* If actual encoded value does not fit into an jInt data type,
* then it is truncated to jInt value (only lower 32 bits are returned);
* the number is read entirely in this case.
*
* @param output parameter - the jInt value read
*/
enum DXResult readCompactInt( OUT jInt* );

/**
* Reads a jLong value in a compact format.
*
* @param output parameter - the jLong value read
*/
enum DXResult readCompactLong( OUT jLong* );

/**
* Reads an array of bytes in a compact encapsulation format.
* This method defines length as a number of bytes.
*
* @param output parameter - the byte array read
*/
enum DXResult readByteArray( OUT jByte** );

// ========== UTF API ==========

/**
* Reads Unicode code point in a UTF-8 format.
* Overlong UTF-8 and CESU-8-encoded surrogates are accepted and read without errors.
*
* @param output parameter - the Unicode code point read
*/
enum DXResult readUTFChar( OUT jInt* );

/**
* Reads Unicode string in a UTF-8 format with compact encapsulation.
* Overlong UTF-8 and CESU-8-encoded surrogates are accepted and read without errors.
* This method defines length as a number of bytes.
*
* @param output parameter - the Unicode string read
*/
enum DXResult readUTFString( OUT dx_string* );




#endif // BUFFERED_INPUT