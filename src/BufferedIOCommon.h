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

#ifndef BUFFERED_IO_COMMON_H_INCLUDED
#define BUFFERED_IO_COMMON_H_INCLUDED

#include "PrimitiveTypes.h"
#include "DXTypes.h"

/* -------------------------------------------------------------------------- */
/*
 *	Common Unicode functions
 */
/* -------------------------------------------------------------------------- */

/*
 * Determines if the given dx_char_t value is a
 * high-surrogate code unit (also known as leading-surrogate
 * code unit). Such values do not represent characters by
 * themselves, but are used in the representation of
 * supplementary characters in the UTF-16 encoding.
 *
 * This method returns nonzero if and only if
 * value >= '\uD800' && value <= '\uDBFF' is true.
 *
 * @param   value   the dx_char_t value to be tested.
 * @return  nonzero if the dx_char_t value
 *          is between '\uD800' and '\uDBFF' inclusive;
 *          zero otherwise.
 */
int dx_is_high_surrogate (dxf_char_t value);

/*
 * Determines if the given dx_char_t value is a
 * low-surrogate code unit (also known as trailing-surrogate code
 * unit). Such values do not represent characters by themselves,
 * but are used in the representation of supplementary characters
 * in the UTF-16 encoding.
 *
 * This method returns nonzero if and only if
 * value >= '\uDC00' && value <= '\uDFFF' is true.
 *
 * @param   value   the dx_char_t value to be tested.
 * @return  nonzero if the dx_char_t value
 *          is between '\uDC00' and '\uDFFF' inclusive;
 *          zero otherwise.
 */
int dx_is_low_surrogate (dxf_char_t value);

/*
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
dxf_int_t dx_surrogates_to_code_point (dxf_char_t high, dxf_char_t low);

/*
 * Converts the specified character (Unicode code point) to its
 * UTF-16 representation. If the specified code point is a BMP
 * (Basic Multilingual Plane or Plane 0) value, the same value is
 * stored in dest_buffer[dest_buffer_pos], and 1 is returned. If the
 * specified code point is a supplementary character, its
 * surrogate values are stored in dest_buffer[dest_buffer_pos]
 * (high-surrogate) and dest_buffer[dest_buffer_pos+1]
 * (low-surrogate), and 2 is returned.
 *
 * @param  code_point the character (Unicode code point) to be converted.
 * @param  dest_buffer an array of dx_char_t in which the code_point's UTF-16 value is stored.
 * @param dest_buffer_pos the start index into the dest_buffer array where the converted value is stored.
 * @param dest_buffer_length the length of the dest_buffer
 * @param  result pointer to dx_int_t in which stored 1 if the code point is a BMP code point
 * or 2 if the code point is a supplementary code point
 *
 * set error DS_IllegalArgument if the specified code_point is not a valid Unicode code point
 * or the specified <code>dest_buffer</code> is null.
 * set error DS_IndexOutOfBounds if dest_buffer_pos is not less than dest_buffer_length,
 * or if dest_buffer at dest_buffer_pos doesn't have enough array element(s) to store the resulting char
 * value(s). (If dest_buffer_pos is equal to dest_buffer_length-1 and the specified code_point is a supplementary
 * character, the high-surrogate value is not stored in dest_buffer[dest_buffer_pos].)
 */
bool dx_code_point_to_utf16_chars (dxf_int_t code_point, dxf_string_t dest_buffer, int dest_buffer_pos, int dest_buffer_length, OUT int* result);

/* -------------------------------------------------------------------------- */
/*
 *	Common compact API functions
 */
/* -------------------------------------------------------------------------- */

/*
 * Returns number of bytes that are needed to write specified number in a compact format.
 *
 * @param value the number those compact length is returned
 * @return number of bytes that are needed to write specified number in a compact format
 */
int dx_get_compact_size (dxf_long_t value);

#endif /* BUFFERED_IO_COMMON_H_INCLUDED */