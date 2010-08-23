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

#include "PrimitiveTypes.h"

/* -------------------------------------------------------------------------- */
enum dx_data_structures_id_t {
    dx_dsi_1,
    dx_dsi_2
};

/* -------------------------------------------------------------------------- */

enum dx_fields_id_t {
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
};

/* -------------------------------------------------------------------------- */

struct dx_field_info_t {
    enum dx_fields_id_t id;
    dx_string_t         type;
};

/* -------------------------------------------------------------------------- */

struct dx_record_info_t {
    //enum dx_fields_id_t    id;
    dx_string_t             name;
    dx_int_t                fields_count;
    const struct dx_field_info_t* fields;
};

/* -------------------------------------------------------------------------- */

struct dx_struct_one_t {
    dx_string_t record_name;
    dx_int_t    fields_count;
    dx_char_t   field1;
    dx_long_t   field2;
    dx_double_t field3;
    dx_int_t    field4;
    dx_float_t  field5;
};

/* -------------------------------------------------------------------------- */

struct dx_struct_two_t {
    dx_string_t record_name;
    dx_int_t    fields_count;
    dx_char_t   field1;
    dx_long_t   field2;
    dx_double_t field3;
    dx_byte_t   field4;
    dx_float_t  field5;
    dx_float_t  field6;
};

/* -------------------------------------------------------------------------- */

struct dx_struct_three_t {
    dx_string_t record_name;
    dx_int_t    fields_count;
    dx_char_t   field1;
    dx_long_t   field2;
    dx_double_t field3;
    dx_float_t  field4;
    dx_long_t   field5;
    dx_float_t  field6;
};

/* -------------------------------------------------------------------------- */

const struct dx_record_info_t* dx_get_record_by_name(const dx_string_t name);

/* -------------------------------------------------------------------------- */

bool dx_matching_fields(const struct dx_record_info_t* record, const dx_string_t* fname, dx_int_t fname_len, dx_int_t* ftype, dx_int_t ftype_len);
