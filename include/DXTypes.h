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

/**
 * @file
 * @brief dxFeed C API types declarations
 */

#ifndef DX_TYPES_H_INCLUDED
#define DX_TYPES_H_INCLUDED

/// Error code
typedef int ERRORCODE;

/// Subscription
typedef void* dxf_subscription_t;

/// Connection
typedef void* dxf_connection_t;

/// Candle attributes
typedef void* dxf_candle_attributes_t;

/// Snapshot
typedef void* dxf_snapshot_t;

/// Price level book
typedef void* dxf_price_level_book_t;

/// Regional book
typedef void* dxf_regional_book_t;

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
typedef int                dxf_dayid_t;

/// String
typedef dxf_char_t*        dxf_string_t;

/// Const String
typedef const dxf_char_t*  dxf_const_string_t;

#else /* POSIX? */

#include <stdint.h>
#include <wchar.h>

/// Boolean
typedef unsigned char    dxf_bool_t;           // 8 bit

/// Byte
typedef int8_t           dxf_byte_t;           // 8 bit

/// Unsigned byte
typedef uint8_t          dxf_ubyte_t;  // 8 bit

/// Char
typedef wchar_t          dxf_char_t;           // 16 bit
//typedef unsigned wchar_t   dx_unsigned_char_t;  // 16 bit

/// Short
typedef int16_t          dxf_short_t;          // 16 bit

/// Unsigned short
typedef uint16_t         dxf_ushort_t; // 16 bit

/// Int
typedef int32_t          dxf_int_t;            // 32 bit

/// Unsigned int
typedef uint32_t         dxf_uint_t;   // 32 bit

/// Float
typedef float            dxf_float_t;          // 32 bit

/// Long
typedef int64_t          dxf_long_t;           // 64 bit

/// Unsigned long
typedef uint64_t         dxf_ulong_t;  // 64 bit

/// Double
typedef double           dxf_double_t;         // 64 bit

/// DayId
typedef int32_t          dxf_dayid_t;

/// String
typedef dxf_char_t*        dxf_string_t;

/// Const String
typedef const dxf_char_t*  dxf_const_string_t;

#endif /* _WIN32/POSIX */

/// Event flags
typedef dxf_uint_t dxf_event_flags_t;

/// Byte array
typedef struct {
    dxf_byte_t* elements;
    int size;
    int capacity;
} dxf_byte_array_t;

/// Property item
typedef struct {
	dxf_string_t key;
	dxf_string_t value;
} dxf_property_item_t;

/// Connection status
typedef enum {
	dxf_cs_not_connected = 0,
	dxf_cs_connected,
	dxf_cs_login_required,
	dxf_cs_authorized
} dxf_connection_status_t;

#endif /* DX_TYPES_H_INCLUDED */