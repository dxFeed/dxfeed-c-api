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
#include "ParserCommon.h"
/* -------------------------------------------------------------------------- */

//static const struct dx_field_info_t dx_fields[] = 
//{
//    {dx_fid_void,           L"VOID"},
//    {dx_fid_byte,           L"BYTE"},
//    {dx_fid_utf_char,       L"UTF_CHAR"},
//    {dx_fid_short,          L"SHORT"},
//    {dx_fid_int,            L"INT"},
//    {dx_fid_compact_int,    L"COMPACT_INT"},
//    {dx_fid_byte_array,     L"BYTE_ARRAY"},
//    {dx_fid_utf_char_array, L"UTF_CHAR_ARRAY"},
//
//    {dx_fid_compact_int | dx_fid_flag_decimal,      L"DECIMAL"},
//    {dx_fid_compact_int | dx_fid_flag_short_string, L"SHORT_STRING"},
//    {dx_fid_byte_array | dx_fid_flag_string,        L"STRING"},
//    {dx_fid_byte_array | dx_fid_flag_custom_object, L"CUSTOM_OBJECT"},
//    {dx_fid_byte_array | dx_fid_flag_serial_object, L"SERIAL_OBJECT"}
//};

/* -------------------------------------------------------------------------- */

//static const dx_int_t dx_fields_types_count = sizeof(dx_fields) / sizeof(dx_fields[0]);

/* -------------------------------------------------------------------------- */

/*
*   fields of data records
*/
static const struct dx_field_info_ex_t dx_fields_stub[] = { {dx_fid_void , L"Stub"} }; 

static const struct dx_field_info_ex_t dx_fields_qoute[] =
        { {dx_fid_compact_int,						 L"Bid.Time"}, 
		  {dx_fid_utf_char,							 L"Bid.Exchange"}, 
		  {dx_fid_compact_int & dx_fid_flag_decimal, L"Bid.Price"}, 
		  {dx_fid_compact_int,						 L"Bid.Size"},
		  {dx_fid_compact_int,						 L"Ask.Time"}, 
		  {dx_fid_utf_char,							 L"Ask.Exchange"}, 
		  {dx_fid_compact_int & dx_fid_flag_decimal, L"Ask.Price"}, 
		  {dx_fid_compact_int,						 L"Ask.Size"} 
		}; 

//static const struct dx_field_info_ex_t dx_fields_2[] =
//        { {dx_fid_byte, L"BYTE", L"Field1"}, {dx_fid_int, L"INT", L"Field2"}, {dx_fid_byte, L"BYTE", L"Field3"} };
//
//static const struct dx_field_info_ex_t dx_fields_3[] =
//        { {dx_fid_int, L"INT", L"Field1"}, {dx_fid_compact_int, L"COMPACT_INT", L"Field2"}, {dx_fid_utf_char_array, L"UTF_CHAR_ARRAY", L"Field3"} };
//
//static const struct dx_field_info_ex_t dx_fields_4[] =
//        { {dx_fid_short, L"SHORT", L"Field1"}, {dx_fid_utf_char, L"UTF_CHAR", L"Field2"}, {dx_fid_int, L"INT", L"Field3"}, {dx_fid_byte, L"BYTE", L"Field4"} };

/* -------------------------------------------------------------------------- */

/*
*   data records
*/
static const struct dx_record_info_t dx_records[] = 
{
	{ 0, L"Stub", sizeof(dx_fields_stub) / sizeof(dx_fields_stub[0]), &dx_fields_stub[0] },
    { 1, L"Qoute", sizeof(dx_fields_qoute) / sizeof(dx_fields_qoute[0]), &dx_fields_qoute[0] },
   // { 2, L"Record2", sizeof(dx_fields_2) / sizeof(dx_fields_2[0]), &dx_fields_2[0] },
   // { 3, L"Record3", sizeof(dx_fields_3) / sizeof(dx_fields_3[0]), &dx_fields_3[0] },
   // { 4, L"Record4", sizeof(dx_fields_4) / sizeof(dx_fields_4[0]), &dx_fields_4[0] },
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

    if (fname_len != n || ftype_len != n) {
        return false; // different number of fields
    }

    for (i = 0; i < n; i++) {
        const struct dx_field_info_ex_t* fld = &record->fields[i];
        if (wcscmp(fld->local_name, fname[i]) != 0 || fld->id != ftype[i]) {
            return false;
        }
    }

    return true;
}


enum dx_result_t dx_get_record_by_id(dx_int_t id, struct dx_record_info_t* record){
	if (id < 0 || id > dx_records_count)
		return setParseError(dx_pr_wrong_record_id);

	*record = dx_records[id];
	return parseSuccessful();
}

