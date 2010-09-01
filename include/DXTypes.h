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

#ifndef API_TYPES_H
#define API_TYPES_H

//#include "EventData.h"
// dummy
typedef int ERRORCODE;
//typedef int HFEED;
//typedef int HCONNECTION;

//typedef int event_listener_t;
typedef enum dxf_event_t{
	dx_qoute,
	dx_trade
};
#ifdef _WIN32

#include <wchar.h>

typedef unsigned char      dx_bool_t;           // 8 bit
typedef char               dx_byte_t;           // 8 bit
typedef unsigned char      dx_ubyte_t;  // 8 bit
typedef wchar_t            dx_char_t;           // 16 bit
//typedef unsigned wchar_t   dx_unsigned_char_t;  // 16 bit
typedef short int          dx_short_t;          // 16 bit
typedef unsigned short int dx_ushort_t; // 16 bit
typedef int                dx_int_t;            // 32 bit
typedef unsigned int       dx_uint_t;   // 32 bit
typedef float              dx_float_t;          // 32 bit
typedef long long          dx_long_t;           // 64 bit
typedef unsigned long long dx_ulong_t;  // 64 bit
typedef double             dx_double_t;         // 64 bit

typedef dx_char_t*    dx_string_t;
typedef const dx_char_t *dx_const_string_t;

#endif /* _WIN32 */
#endif // API_TYPES_H