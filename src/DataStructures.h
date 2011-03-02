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

#ifndef DATA_STRUCTURES_H_INCLUDED
#define DATA_STRUCTURES_H_INCLUDED

#include "PrimitiveTypes.h"
#include "DXTypes.h"
#include "RecordFieldSetters.h"
#include "BufferedIOCommon.h"
#include "EventData.h"

/* -------------------------------------------------------------------------- */
/*
 *	Connection context functions
 */
/* -------------------------------------------------------------------------- */

void* dx_get_data_structures_connection_context (dxf_connection_t connection);

/* -------------------------------------------------------------------------- */
/*
 *	Event record types and structures
 */
/* -------------------------------------------------------------------------- */

typedef enum {
    dx_fid_void = 0,
    dx_fid_byte = 1,
    dx_fid_utf_char = 2,
    dx_fid_short = 3,
    dx_fid_int = 4,
    // ids 5,6,7 are reserved for future use

    dx_fid_compact_int = 8,
    dx_fid_byte_array = 9,
    dx_fid_utf_char_array = 10,
    // ids 11-15 are reserved for future use as array of short, array of int, etc

    dx_fid_flag_decimal = 0x10,       // decimal representation as int field
    dx_fid_flag_short_string = 0x20,  // short (up to 4-character) string representation as int field
    dx_fid_flag_string = 0x80,        // String representation as byte array
    dx_fid_flag_custom_object = 0xe0, // customly serialized object as byte array
    dx_fid_flag_serial_object = 0xf0  // serialized object as byte array
} dx_field_id_t;

/* -------------------------------------------------------------------------- */

typedef struct {
    int type;
    dxf_const_string_t name;
    dx_record_field_setter_t setter;
    dx_record_field_def_val_getter_t def_val_getter;
} dx_field_info_t;

/* -------------------------------------------------------------------------- */

typedef struct {
    dxf_const_string_t name;
    int field_count;
    const dx_field_info_t* fields;
} dx_record_info_t;

/* -------------------------------------------------------------------------- */
/*
 *	Record functions
 */
/* -------------------------------------------------------------------------- */

dx_record_id_t dx_get_record_id (void* context, dxf_int_t server_record_id);
bool dx_assign_server_record_id (void* context, dx_record_id_t record_id, dxf_int_t server_record_id);

const dx_record_info_t* dx_get_record_by_id (dx_record_id_t record_id);
dx_record_id_t dx_get_record_id_by_name (dxf_const_string_t record_name);

int dx_find_record_field (const dx_record_info_t* record_info, dxf_const_string_t field_name,
                          dxf_int_t field_type);
dxf_char_t dx_get_record_exchange_code (dx_record_id_t record_id);

int* dx_get_record_server_support_states (void* context);

#endif /* DATA_STRUCTURES_H_INCLUDED */