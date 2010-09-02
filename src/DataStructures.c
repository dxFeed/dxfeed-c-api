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
#include "EventData.h"
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
static const struct dx_field_info_ex_t dx_fields_trade[] =  
        { 
		  {dx_fid_utf_char,							 L"Last.Exchange"}, 
		  {dx_fid_compact_int,						 L"Last.Time "}, 
		  {dx_fid_compact_int| dx_fid_flag_decimal,	 L"Last.Price"},
		  {dx_fid_compact_int,						 L"Last.Size"}, 
		  {dx_fid_compact_int,						 L"Last.Tick "}, 
		  {dx_fid_compact_int| dx_fid_flag_decimal,  L"Last.Change"}, 
		  {dx_fid_compact_int| dx_fid_flag_decimal,	 L"Volume"}
		}; 

static const struct dx_field_info_ex_t dx_fields_quote[] =
        { 
		  {dx_fid_utf_char,							 L"Bid.Exchange"}, 
		  {dx_fid_compact_int | dx_fid_flag_decimal, L"Bid.Price"}, 
		  {dx_fid_compact_int,						 L"Bid.Size"},
		  {dx_fid_utf_char,							 L"Ask.Exchange"}, 
		  {dx_fid_compact_int | dx_fid_flag_decimal, L"Ask.Price"}, 
		  {dx_fid_compact_int,						 L"Ask.Size"}, 
		  {dx_fid_compact_int,						 L"Bid.Time"}, 
		  {dx_fid_compact_int,						 L"Ask.Time"}
		}; 
static const struct dx_field_info_ex_t dx_fields_fundamental[] =
        { 
		  {dx_fid_compact_int | dx_fid_flag_decimal, L"High.Price"}, 
		  {dx_fid_compact_int | dx_fid_flag_decimal, L"Low.Price"}, 
		  {dx_fid_compact_int | dx_fid_flag_decimal, L"Open.Price"},
		  {dx_fid_compact_int | dx_fid_flag_decimal, L"Close.Price"}, 
		  {dx_fid_compact_int,						 L"OpenInterest"} 
		}; 
static const struct dx_field_info_ex_t dx_fields_profile[] =
        { 
		  {dx_fid_compact_int | dx_fid_flag_decimal, L"Beta"}, 
		  {dx_fid_compact_int | dx_fid_flag_decimal, L"Eps"}, 
		  {dx_fid_compact_int,						 L"DivFreq"},
		  {dx_fid_compact_int | dx_fid_flag_decimal, L"ExdDiv.Amount"}, 
		  {dx_fid_compact_int ,						 L"ExdDiv.Date"}, 
		  {dx_fid_compact_int | dx_fid_flag_decimal, L"52High.Price"}, 
		  {dx_fid_compact_int | dx_fid_flag_decimal, L"52Low.Price"}, 
		  {dx_fid_compact_int | dx_fid_flag_decimal, L"Shares"},
		  {dx_fid_compact_int,						 L"IsIndex"}, 
		  {dx_fid_utf_char_array,					 L"Description"}
		}; 
static const struct dx_field_info_ex_t dx_fields_market_maker[] =
        { 
		  {dx_fid_utf_char,	   				         L"MMExchange"}, 
		  {dx_fid_compact_int,						 L"MMID"}, 
		  {dx_fid_compact_int| dx_fid_flag_decimal,	 L"MMBid.Price"},
		  {dx_fid_compact_int ,						 L"MMBid.Size"}, 
		  {dx_fid_compact_int | dx_fid_flag_decimal, L"MMAsk.Price"}, 
		  {dx_fid_compact_int ,						 L"MMAsk.Size"} 
		}; 
/* -------------------------------------------------------------------------- */

/*
*   data records
*/
static const struct dx_record_info_t dx_records[] = 
{
	{ 0, L"Trade", sizeof(dx_fields_trade) / sizeof(dx_fields_trade[0]), &dx_fields_trade[0] },
    { 1, L"Quote", sizeof(dx_fields_quote) / sizeof(dx_fields_quote[0]), &dx_fields_quote[0] },
    { 2, L"Fundamental", sizeof(dx_fields_fundamental) / sizeof(dx_fields_fundamental[0]), &dx_fields_fundamental[0] },
    { 3, L"Profile", sizeof(dx_fields_profile) / sizeof(dx_fields_profile[0]), &dx_fields_profile[0] },
    { 4, L"MarketMaker", sizeof(dx_fields_market_maker) / sizeof(dx_fields_market_maker[0]), &dx_fields_market_maker[0] },
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
// record ids simply hardcoded
dx_int_t get_record_id(dx_int_t event_type){
	switch (event_type){
		case DX_ET_TRADE: return 0; 
		case DX_ET_QUOTE: return 1; 
		case DX_ET_FUNDAMENTAL: return 2; 
		case DX_ET_PROFILE: return 3; 
		case DX_ET_MARKET_MAKER: return 4; 
		default: return (-1);
	}	
}

dx_int_t get_event_type_by_id(dx_int_t record_id){
	switch (record_id){
		case 0: return DX_ET_TRADE; 
		case 1 : return DX_ET_QUOTE; 
		case 2 : return DX_ET_FUNDAMENTAL; 
		case 3 : return DX_ET_PROFILE; 
		case 4 : return DX_ET_MARKET_MAKER; 
		default: return (DX_ET_UNUSED);
	}
}