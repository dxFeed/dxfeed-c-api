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
#include "EventDataFieldSetters.h"

/* -------------------------------------------------------------------------- */
/*
 *	Trade data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_trade[] = { 
	{ dx_fid_utf_char, L"Last.Exchange", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_trade_t, last_exchange) },
	{ dx_fid_compact_int, L"Last.Time", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_trade_t, last_time) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Last.Price", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_trade_t, last_price) },
	{ dx_fid_compact_int, L"Last.Size", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_trade_t, last_size) },
	{ dx_fid_compact_int, L"Last.Tick", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_trade_t, last_tick) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Last.Change", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_trade_t, last_change) }, 
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Volume", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_trade_t, volume) }
}; 

/* -------------------------------------------------------------------------- */
/*
 *	Quote data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_quote[] = {
    { dx_fid_utf_char, L"Bid.Exchange", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_quote_t, bid_exchange) },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Bid.Price", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_quote_t, bid_price) },
    { dx_fid_compact_int, L"Bid.Size", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_quote_t, bid_size) },
    { dx_fid_utf_char, L"Ask.Exchange", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_quote_t, ask_exchange) },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Ask.Price", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_quote_t, ask_price) },
    { dx_fid_compact_int, L"Ask.Size", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_quote_t, ask_size) },
    { dx_fid_compact_int, L"Bid.Time", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_quote_t, bid_time) },
    { dx_fid_compact_int, L"Ask.Time", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_quote_t, ask_time) }
};

/* -------------------------------------------------------------------------- */
/*
 *	Fundamental data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_fundamental[] = { 
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"High.Price", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_fundamental_t, high_price) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Low.Price", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_fundamental_t, low_price) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Open.Price", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_fundamental_t, open_price) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Close.Price", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_fundamental_t, close_price) },
	{ dx_fid_compact_int, L"OpenInterest", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_fundamental_t, open_interest) }
};
		
/* -------------------------------------------------------------------------- */
/*
 *	Profile data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_profile[] = { 
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Beta", NULL },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Eps", NULL },
	{ dx_fid_compact_int, L"DivFreq", NULL },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"ExdDiv.Amount", NULL },
	{ dx_fid_compact_int , L"ExdDiv.Date", NULL },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"52High.Price", NULL },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"52Low.Price", NULL },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Shares", NULL },
	{ dx_fid_compact_int, L"IsIndex", NULL },
	{ dx_fid_utf_char_array, L"Description", NULL }
}; 

/* -------------------------------------------------------------------------- */
/*
 *	Market maker data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_market_maker[] = { 
	{ dx_fid_utf_char, L"MMExchange", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_market_maker, mm_exchange) },
	{ dx_fid_compact_int, L"MMID", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_market_maker, mm_id) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"MMBid.Price", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_market_maker, mmbid_price) },
	{ dx_fid_compact_int, L"MMBid.Size", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_market_maker, mmbid_size) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"MMAsk.Price", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_market_maker, mmask_price) },
	{ dx_fid_compact_int, L"MMAsk.Size", DX_EVENT_DATA_FIELD_SETTER_NAME(dxf_market_maker, mmask_size) }
}; 

/* -------------------------------------------------------------------------- */
/*
 *	Event data records
 */
/* -------------------------------------------------------------------------- */

static const dx_record_info_t dx_records[] = {
	{ 0, L"Trade", sizeof(dx_fields_trade) / sizeof(dx_fields_trade[0]), &dx_fields_trade[0], dx_eid_trade },
    { 1, L"Quote", sizeof(dx_fields_quote) / sizeof(dx_fields_quote[0]), &dx_fields_quote[0], dx_eid_quote },
    { 2, L"Fundamental", sizeof(dx_fields_fundamental) / sizeof(dx_fields_fundamental[0]), &dx_fields_fundamental[0], dx_eid_fundamental },
    { 3, L"Profile", sizeof(dx_fields_profile) / sizeof(dx_fields_profile[0]), &dx_fields_profile[0], dx_eid_profile },
    { 4, L"MarketMaker", sizeof(dx_fields_market_maker) / sizeof(dx_fields_market_maker[0]), &dx_fields_market_maker[0], dx_eid_market_maker },
};

static const dx_int_t g_records_count = sizeof(dx_records) / sizeof(dx_records[0]);

/* -------------------------------------------------------------------------- */

const dx_record_info_t* dx_get_record_by_name (dx_const_string_t name) {
    int i = 0;
    
    for (; i < g_records_count; ++i) {
        if (wcscmp(dx_records[i].name, name) == 0) {
            return &dx_records[i];
        }
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

bool dx_matching_fields (const dx_record_info_t* record,
                         const dx_const_string_t* field_names, size_t field_name_count,
                         const dx_int_t* field_types, size_t field_type_count) {
    size_t n = record->field_count;
    size_t i = 0;

    if (field_name_count != record->field_count || field_type_count != record->field_count) {
        return false;
    }

    for (; i < record->field_count; ++i) {
        const dx_field_info_t* field = &(record->fields[i]);
        
        if (wcscmp(field->local_name, field_names[i]) != 0 || field->id != field_types[i]) {
            return false;
        }
    }

    return true;
}

/* -------------------------------------------------------------------------- */

dx_result_t dx_get_record_by_id (dx_int_t id, dx_record_info_t** record) {
	if (id < 0 || id > g_records_count) {
		return setParseError(dx_pr_wrong_record_id);
    }

	*record = &dx_records[id];
	
	return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

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

dx_const_string_t dx_event_type_to_string(dx_int_t event_type){
	switch (event_type){
		case DX_ET_TRADE: return L"Trade"; 
		case DX_ET_QUOTE: return L"Quote"; 
		case DX_ET_FUNDAMENTAL: return L"Fundamental"; 
		case DX_ET_PROFILE: return L"Profile"; 
		case DX_ET_MARKET_MAKER: return L"MarketMaker"; 
		default: return L"";
	}	
}