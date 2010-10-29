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

#ifndef DX_TYPES_H_INCLUDED
#define DX_TYPES_H_INCLUDED

typedef int ERRORCODE;
typedef void* dxf_subscription_t;

#ifdef _WIN32

#include <wchar.h>

typedef unsigned char      dxf_bool_t;           // 8 bit
typedef char               dxf_byte_t;           // 8 bit
typedef unsigned char      dxf_ubyte_t;  // 8 bit
typedef wchar_t            dxf_char_t;           // 16 bit
//typedef unsigned wchar_t   dx_unsigned_char_t;  // 16 bit
typedef short int          dxf_short_t;          // 16 bit
typedef unsigned short int dxf_ushort_t; // 16 bit
typedef int                dxf_int_t;            // 32 bit
typedef unsigned int       dxf_uint_t;   // 32 bit
typedef float              dxf_float_t;          // 32 bit
typedef long long          dxf_long_t;           // 64 bit
typedef unsigned long long dxf_ulong_t;  // 64 bit
typedef double             dxf_double_t;         // 64 bit

typedef dxf_char_t*        dxf_string_t;
typedef const dxf_char_t*  dxf_const_string_t;

#endif /* _WIN32 */
#endif /* DX_TYPES_H_INCLUDED */