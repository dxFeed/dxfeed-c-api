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

#include "DataStructures.h"

/* -------------------------------------------------------------------------- */

static const struct dx_field_info_t dx_fields[] = 
{
    {dx_fid_void,           L"VOID"},
    {dx_fid_byte,           L"BYTE"},
    {dx_fid_utf_char,       L"UTF_CHAR"},
    {dx_fid_short,          L"SHORT"},
    {dx_fid_int,            L"INT"},
    {dx_fid_compact_int,    L"COMPACT_INT"},
    {dx_fid_byte_array,     L"BYTE_ARRAY"},
    {dx_fid_utf_char_array, L"UTF_CHAR_ARRAY"},

    {dx_fid_compact_int | dx_fid_flag_decimal,      L"DECIMAL"},
    {dx_fid_compact_int | dx_fid_flag_short_string, L"SHORT_STRING"},
    {dx_fid_byte_array | dx_fid_flag_string,        L"STRING"},
    {dx_fid_byte_array | dx_fid_flag_custom_object, L"CUSTOM_OBJECT"},
    {dx_fid_byte_array | dx_fid_flag_serial_object, L"SERIAL_OBJECT"}
};

/* -------------------------------------------------------------------------- */

static const dx_int_t dx_fields_types_count = sizeof(dx_fields) / sizeof(dx_fields[0]);

/* -------------------------------------------------------------------------- */

/*
*   fields of data records
*/
static const struct dx_field_info_t dx_fields_1[] =
        { {dx_fid_byte, L"BYTE"}, {dx_fid_int, L"INT"}, {dx_fid_int, L"INT"}, {dx_fid_utf_char_array, L"UTF_CHAR_ARRAY"} };

static const struct dx_field_info_t dx_fields_2[] =
        { {dx_fid_byte, L"BYTE"}, {dx_fid_int, L"INT"}, {dx_fid_byte, L"BYTE"} };

static const struct dx_field_info_t dx_fields_3[] =
        { {dx_fid_int, L"INT"}, {dx_fid_compact_int, L"COMPACT_INT"}, {dx_fid_utf_char_array, L"UTF_CHAR_ARRAY"} };

static const struct dx_field_info_t dx_fields_4[] =
        { {dx_fid_short, L"SHORT"}, {dx_fid_utf_char, L"UTF_CHAR"}, {dx_fid_int, L"INT"}, {dx_fid_byte, L"BYTE"} };

/* -------------------------------------------------------------------------- */

/*
*   data records
*/
static const struct dx_record_info_t dx_records[] = 
{
    { L"Record1", sizeof(dx_fields_1) / sizeof(dx_fields_1[0]), &dx_fields_1[0] },
    { L"Record2", sizeof(dx_fields_2) / sizeof(dx_fields_2[0]), &dx_fields_2[0] },
    { L"Record3", sizeof(dx_fields_3) / sizeof(dx_fields_3[0]), &dx_fields_3[0] },
    { L"Record4", sizeof(dx_fields_4) / sizeof(dx_fields_4[0]), &dx_fields_4[0] },
};

static const dx_int_t dx_records_count = sizeof(dx_records) / sizeof(dx_records[0]);

/* -------------------------------------------------------------------------- */

const struct dx_record_info_t* dx_get_record_by_name(const dx_string_t name) {
    int i = 0;
    for (; i < dx_records_count; ++i) {
        if (wcscmp(dx_records[i].name, name) == 0) {
            return &dx_records[i];
        }
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

bool dx_matching_fields(const struct dx_record_info_t* record, const dx_string_t* fname, dx_int_t fname_len, dx_int_t* ftype, dx_int_t ftype_len) {
    dx_int_t n = record->fields_count;
    dx_int_t i;

    if (fname_len != n || ftype_len != n)
        return false; // different number of fields

    for (i = 0; i < n; i++) {
        dx_field_info_t* fld = &record->fields[i];
        if (!fld->getLocalName().equals(fname[i]) || fld->id != ftype[i])
            return false;
    }

    return true;
    //dx_int_t nint = record.getIntFieldCount();
    //dx_int_t nobj = record.getObjFieldCount();
    //dx_int_t n = nint + nobj;
    //dx_int_t i;
    //if (fname.length != n || ftype.length != n)
    //    return false; // different number of fields
    //for (i = 0; i < nint; i++) {
    //    DataIntField fld = record.getIntField(i);
    //    if (!fld.getLocalName().equals(fname[i]) || fld.getSerialType().getId() != ftype[i])
    //        return false;
    //}
    //for (i = 0; i < nobj; i++) {
    //    DataObjField fld = record.getObjField(i);
    //    if (!fld.getLocalName().equals(fname[i + nint]) || fld.getSerialType().getId() != ftype[i + nint])
    //        return false;
    //}
    //return true;
}
