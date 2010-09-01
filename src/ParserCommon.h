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

#ifndef COMMON_H
#define COMMON_H

#include <wctype.h>
#include <limits.h>
#include "PrimitiveTypes.h"
#include "DXErrorCodes.h"
#include "DXTypes.h"
////////////////////////////////////////////////////////////////////////////////
// prefix of function's parameters
#define OUT

enum dx_result_t {
    R_SUCCESSFUL = 0,
    R_FAILED
};

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t setParseError(int err);
enum dx_result_t parseSuccessful();

enum parser_result_t dx_get_parser_last_error();

dx_string_t dx_create_string(dx_int_t size);

#define CHECKED_CALL(func, param) \
    if (func(param) != R_SUCCESSFUL) {\
    return R_FAILED;\
    }

#define CHECKED_CALL_0(func) \
    if (func() != R_SUCCESSFUL) {\
    return R_FAILED;\
    }

#define CHECKED_CALL_2(func, param1, param2) \
    if (func( param1, param2 ) != R_SUCCESSFUL) {\
    return R_FAILED;\
    }

#define CHECKED_CALL_3(func, param1, param2, param3) \
    if (func( param1, param2, param3 ) != R_SUCCESSFUL) {\
    return R_FAILED;\
    }

////////////////////////////////////////////////////////////////////////////////
// Unicode helpers
////////////////////////////////////////////////////////////////////////////////

/**
* Determines if the given dx_char_t value is a
* high-surrogate code unit (also known as leading-surrogate
* code unit). Such values do not represent characters by
* themselves, but are used in the representation of 
* supplementary characters in the UTF-16 encoding.
*
* This method returns nonzero if and only if
* ch >= '\uD800' && ch <= '\uDBFF' is true.
*
* @param   ch   the dx_char_t value to be tested.
* @return  nonzero if the dx_char_t value
*          is between '\uD800' and '\uDBFF' inclusive;
*          zero otherwise.
*/
int isHighSurrogate(dx_char_t);


/**
* Determines if the given dx_char_t value is a
* low-surrogate code unit (also known as trailing-surrogate code
* unit). Such values do not represent characters by themselves,
* but are used in the representation of supplementary characters
* in the UTF-16 encoding.
*
* This method returns nonzero if and only if
* ch >= '\uDC00' && ch <= '\uDFFF' is true.
*
* @param   ch   the dx_char_t value to be tested.
* @return  nonzero if the dx_char_t value
*          is between '\uDC00' and '\uDFFF' inclusive;
*          zero otherwise.
*/
int isLowSurrogate(dx_char_t);

/**
* Converts the specified surrogate pair to its supplementary code
* point value. This method does not validate the specified
* surrogate pair. The caller must validate it using 
* isSurrogatePair(char, char) if necessary.
*
* @param  high the high-surrogate code unit
* @param  low the low-surrogate code unit
* @return the supplementary code point composed from the
*         specified surrogate pair.
*/
dx_int_t toCodePoint(dx_char_t high, dx_char_t low);

/**
* Converts the specified character (Unicode code point) to its
* UTF-16 representation. If the specified code point is a BMP
* (Basic Multilingual Plane or Plane 0) value, the same value is
* stored in dst[dstIndex], and 1 is returned. If the
* specified code point is a supplementary character, its
* surrogate values are stored in dst[dstIndex]
* (high-surrogate) and dst[dstIndex+1]
* (low-surrogate), and 2 is returned.
*
* @param  codePoint the character (Unicode code point) to be converted.
* @param dstIndex the start index into the dst array where the converted value is stored.
* @param dstLen the length of the dst
* @param  dst pointer to an array of dx_char_t in which the codePoint's UTF-16 value is stored.
* @param  res pointer to dx_int_t in which stored 1 if the code point is a BMP code point
* or 2 if the code point is a supplementary code point
*
* set error DS_IllegalArgument if the specified codePoint is not a valid Unicode code point
* or the specified <code>dst</code> is null.
* set error DS_IndexOutOfBounds if dstIndex is not less than dstLen,
* or if dst at dstIndex doesn't have enough array element(s) to store the resulting char
* value(s). (If dstIndex is equal to dstLen-1 and the specified codePoint is a supplementary
* character, the high-surrogate value is not stored in dst[dstIndex].)
*/
enum dx_result_t toChars(dx_int_t codePoint, dx_int_t dstIndex, dx_int_t dstLen, OUT dx_string_t* dst, OUT dx_int_t* res);

void toSurrogates(dx_int_t codePoint, dx_int_t index, OUT dx_string_t* dst);


/* -------------------------------------------------------------------------- */
/*
/* compact API 
/*
/* -------------------------------------------------------------------------- */

/**
* Returns number of bytes that are needed to write specified number in a compact format.
*
* @param n the number those compact length is returned
* @return number of bytes that are needed to write specified number in a compact format
*/
dx_int_t dx_get_compact_length(dx_long_t n);

//#define NULL 0
//#define false 0
//#define true 1
//typedef int bool;

#endif // COMMON_H