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
#include "EventDataFieldSetters.h"
#include "ParserCommon.h"

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
} dx_fields_id_t;

/* -------------------------------------------------------------------------- */

typedef struct {
    dx_fields_id_t id;
    dx_string_t local_name;
    dx_event_data_field_setter_t setter;
} dx_field_info_t;

/* -------------------------------------------------------------------------- */

typedef struct {
    dx_int_t id;
    dx_string_t name;
    size_t field_count;
    const dx_field_info_t* fields;
    int event_id;
} dx_record_info_t;

//const struct dx_record_info_t* dx_get_record_by_name(const dx_string_t name);
dx_int_t get_record_id(dx_int_t event_type);
dx_int_t get_event_type_by_id(dx_int_t record_id);
dx_const_string_t dx_event_type_to_string(dx_int_t event_type);

/* -------------------------------------------------------------------------- */

bool dx_matching_fields (const dx_record_info_t* record,
                         const dx_const_string_t* field_names, size_t field_name_count,
                         const dx_int_t* field_types, size_t field_type_count);

dx_result_t dx_get_record_by_id (dx_int_t id, dx_record_info_t** record);

#endif /* DATA_STRUCTURES_H_INCLUDED */