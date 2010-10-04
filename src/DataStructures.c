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
#include "DXAlgorithms.h"
#include "ConnectionContextData.h"

/* -------------------------------------------------------------------------- */
/*
 *	Trade data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_trade[] = { 
	{ dx_fid_compact_int, L"Last.Time", DX_RECORD_FIELD_SETTER_NAME(dx_trade_t, time), DX_RECORD_FIELD_DEF_VAL_NAME(dx_trade_t, time) },
	{ dx_fid_utf_char, L"Last.Exchange", DX_RECORD_FIELD_SETTER_NAME(dx_trade_t, exchange_code), DX_RECORD_FIELD_DEF_VAL_NAME(dx_trade_t, exchange_code) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Last.Price", DX_RECORD_FIELD_SETTER_NAME(dx_trade_t, price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_trade_t, price) },
	{ dx_fid_compact_int, L"Last.Size", DX_RECORD_FIELD_SETTER_NAME(dx_trade_t, size), DX_RECORD_FIELD_DEF_VAL_NAME(dx_trade_t, size) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Volume", DX_RECORD_FIELD_SETTER_NAME(dx_trade_t, day_volume), DX_RECORD_FIELD_DEF_VAL_NAME(dx_trade_t, day_volume) }
}; 

/* -------------------------------------------------------------------------- */
/*
 *	Quote data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_quote[] = {
    { dx_fid_compact_int, L"Bid.Time", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, bid_time), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, bid_time) },
    { dx_fid_utf_char, L"Bid.Exchange", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, bid_exchange_code), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, bid_exchange_code) },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Bid.Price", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, bid_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, bid_price) },
    { dx_fid_compact_int, L"Bid.Size", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, bid_size), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, bid_size) },
    { dx_fid_compact_int, L"Ask.Time", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, ask_time), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, ask_time) },
    { dx_fid_utf_char, L"Ask.Exchange", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, ask_exchange_code), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, ask_exchange_code) },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Ask.Price", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, ask_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, ask_price) },
    { dx_fid_compact_int, L"Ask.Size", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, ask_size), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, ask_size) }
};

/* -------------------------------------------------------------------------- */
/*
 *	Fundamental data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_fundamental[] = { 
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"High.Price", DX_RECORD_FIELD_SETTER_NAME(dx_fundamental_t, day_high_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_fundamental_t, day_high_price) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Low.Price", DX_RECORD_FIELD_SETTER_NAME(dx_fundamental_t, day_low_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_fundamental_t, day_low_price) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Open.Price", DX_RECORD_FIELD_SETTER_NAME(dx_fundamental_t, day_open_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_fundamental_t, day_open_price) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Close.Price", DX_RECORD_FIELD_SETTER_NAME(dx_fundamental_t, prev_day_close_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_fundamental_t, prev_day_close_price) },
	{ dx_fid_compact_int, L"OpenInterest", DX_RECORD_FIELD_SETTER_NAME(dx_fundamental_t, open_interest), DX_RECORD_FIELD_DEF_VAL_NAME(dx_fundamental_t, open_interest) }
};

/* -------------------------------------------------------------------------- */
/*
 *	Profile data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_profile[] = { 
	{ dx_fid_utf_char_array, L"Description", DX_RECORD_FIELD_SETTER_NAME(dx_profile_t, description), DX_RECORD_FIELD_DEF_VAL_NAME(dx_profile_t, description) }
};

/* -------------------------------------------------------------------------- */
/*
 *	Market maker data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_market_maker[] = { 
	{ dx_fid_utf_char, L"MMExchange", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mm_exchange), DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mm_exchange) },
	{ dx_fid_compact_int, L"MMID", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mm_id), DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mm_id) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"MMBid.Price", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmbid_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmbid_price) },
	{ dx_fid_compact_int, L"MMBid.Size", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmbid_size), DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmbid_size) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"MMAsk.Price", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmask_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmask_price) },
	{ dx_fid_compact_int, L"MMAsk.Size", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmask_size), DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmask_size) }
}; 

/* -------------------------------------------------------------------------- */
/*
 *	Time and Sale data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_time_and_sale[] = {
    { dx_fid_compact_int, L"Time", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, time), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, time) },
    { dx_fid_compact_int, L"Sequence", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, sequence), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, sequence) },
    { dx_fid_utf_char, L"Exchange", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, exchange_code), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, exchange_code) },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Price", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, price) },
    { dx_fid_compact_int, L"Size", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, size), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, size) },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Bid.Price", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, bid_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, bid_price) },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Ask.Price", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, ask_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, ask_price) },
    { dx_fid_compact_int, L"ExchangeSaleConditions", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, exch_sale_conds), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, exch_sale_conds) },
    { dx_fid_compact_int, L"Flags", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, type), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, type) }
};

/* -------------------------------------------------------------------------- */
/*
 *	Records
 */
/* -------------------------------------------------------------------------- */

static const int g_record_field_counts[dx_rid_count] = {
    sizeof(dx_fields_trade) / sizeof(dx_fields_trade[0]),
    sizeof(dx_fields_quote) / sizeof(dx_fields_quote[0]),
    sizeof(dx_fields_fundamental) / sizeof(dx_fields_fundamental[0]),
    sizeof(dx_fields_profile) / sizeof(dx_fields_profile[0]),
    sizeof(dx_fields_market_maker) / sizeof(dx_fields_market_maker[0]),
    sizeof(dx_fields_time_and_sale) / sizeof(dx_fields_time_and_sale[0])
};

static const dx_record_info_t g_records[dx_rid_count] = {
	{ L"Trade", sizeof(dx_fields_trade) / sizeof(dx_fields_trade[0]), dx_fields_trade },
    { L"Quote", sizeof(dx_fields_quote) / sizeof(dx_fields_quote[0]), dx_fields_quote },
    { L"Fundamental", sizeof(dx_fields_fundamental) / sizeof(dx_fields_fundamental[0]), dx_fields_fundamental },
    { L"Profile", sizeof(dx_fields_profile) / sizeof(dx_fields_profile[0]), dx_fields_profile },
    { L"MarketMaker", sizeof(dx_fields_market_maker) / sizeof(dx_fields_market_maker[0]), dx_fields_market_maker },
    { L"TimeAndSale", sizeof(dx_fields_time_and_sale) / sizeof(dx_fields_time_and_sale[0]), dx_fields_time_and_sale }
};

/* In the Java code, the exchange code is determined by the record name: it's the last symbol of
   the record name if the second last symbol is '&', otherwise it's zero.
   Here we don't have any exchange code semantics in the record names (and neither the Java code does),
   so all the record exchange codes are set to zero */
static dx_char_t const g_record_exchange_codes[dx_rid_count] = { 0 };

/* -------------------------------------------------------------------------- */
/*
 *	Data structures connection context
 */
/* -------------------------------------------------------------------------- */

typedef struct {
    dx_record_id_t protocol_to_record_id_map[dx_rid_count];
    dx_record_server_support_info_t record_server_support_states[dx_rid_count];
} dx_data_structures_connection_context_t;

#define CONTEXT_FIELD(field) \
    (((dx_data_structures_connection_context_t*)dx_get_subsystem_data(connection, dx_ccs_data_structures))->field)

/* -------------------------------------------------------------------------- */

void dx_free_server_support_states_data (dx_record_server_support_info_t* data);

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_data_structures) {
    dx_data_structures_connection_context_t* context = dx_calloc(1, sizeof(dx_data_structures_connection_context_t));
    dx_record_id_t record_id = dx_rid_begin;
    
    if (context == NULL) {
        return false;
    }
    
    for (; record_id < dx_rid_count; ++record_id) {
        context->protocol_to_record_id_map[record_id] = record_id;
        
        if ((context->record_server_support_states[record_id].fields = 
             dx_calloc(g_record_field_counts[record_id], sizeof(bool))) == NULL) {
            dx_free_server_support_states_data(context->record_server_support_states);
            dx_free(context);
            
            return false;
        }
        
        context->record_server_support_states[record_id].field_count = g_record_field_counts[record_id];
    }
    
    if (!dx_set_subsystem_data(connection, dx_ccs_data_structures, context)) {
        dx_free_server_support_states_data(context->record_server_support_states);
        dx_free(context);

        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_data_structures) {
    dx_data_structures_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_data_structures);
    
    if (context == NULL) {
        return true;
    }
    
    dx_free_server_support_states_data(context->record_server_support_states);
    dx_free(context);
    
    return true;
}

/* -------------------------------------------------------------------------- */

void dx_free_server_support_states_data (dx_record_server_support_info_t* data) {
    dx_record_id_t record_id = dx_rid_begin;
    
    for (; record_id < dx_rid_count; ++record_id) {
        if (data[record_id].fields != NULL) {
            dx_free(data[record_id].fields);
        }
    }
}

/* -------------------------------------------------------------------------- */

dx_record_server_support_info_t* dx_get_record_server_support_states (dxf_connection_t connection) {
    return CONTEXT_FIELD(record_server_support_states);
}

/* -------------------------------------------------------------------------- */
/*
 *	Event record functions implementation
 */
/* -------------------------------------------------------------------------- */

dx_record_id_t dx_get_record_id (dxf_connection_t connection, dx_int_t protocol_id) {
    return CONTEXT_FIELD(protocol_to_record_id_map)[protocol_id];
}

/* -------------------------------------------------------------------------- */

void dx_assign_protocol_id (dxf_connection_t connection, dx_record_id_t record_id, dx_int_t protocol_id) {
    CONTEXT_FIELD(protocol_to_record_id_map)[protocol_id] = record_id;
}

/* -------------------------------------------------------------------------- */

const dx_record_info_t* dx_get_record_by_id (dx_record_id_t record_id) {
	return &g_records[record_id];
}

/* -------------------------------------------------------------------------- */

dx_record_id_t dx_get_record_id_by_name (dx_const_string_t record_name) {
    dx_record_id_t record_id = dx_rid_begin;

    for (; record_id < dx_rid_count; ++record_id) {
        if (dx_compare_strings(g_records[record_id].name, record_name) == 0) {
            return record_id;
        }
    }
    
    return dx_rid_invalid;
}

/* -------------------------------------------------------------------------- */

const int g_invalid_index = (int)-1;

int dx_find_record_field (const dx_record_info_t* record_info, dx_const_string_t field_name,
                          dx_int_t field_type) {
    int cur_field_index = 0;
    dx_field_info_t* fields = (dx_field_info_t*)record_info->fields;
    
    for (; cur_field_index < record_info->field_count; ++cur_field_index) {
        if (dx_compare_strings(fields[cur_field_index].name, field_name) == 0 &&
            fields[cur_field_index].type == field_type) {
            
            return cur_field_index;
        }
    }
    
    return g_invalid_index;
}

/* -------------------------------------------------------------------------- */

dx_char_t dx_get_record_exchange_code (dx_record_id_t record_id) {
    return g_record_exchange_codes[record_id];
}