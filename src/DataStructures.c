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


#include "DXFeed.h"

#include "DataStructures.h"
#include "BufferedIOCommon.h"
#include "DXAlgorithms.h"
#include "ConnectionContextData.h"
#include "DXErrorHandling.h"
#include "Logger.h"
#include "ServerMessageProcessor.h"

/* -------------------------------------------------------------------------- */
/*
 *	Trade data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_trade[] = { 
    { dx_fid_compact_int, L"Last.Time", DX_RECORD_FIELD_SETTER_NAME(dx_trade_t, time), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_trade_t, time), DX_RECORD_FIELD_GETTER_NAME(dx_trade_t, time), 
    dx_ft_common_field },

    { dx_fid_utf_char, L"Last.Exchange", DX_RECORD_FIELD_SETTER_NAME(dx_trade_t, exchange_code), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_trade_t, exchange_code), DX_RECORD_FIELD_GETTER_NAME(dx_trade_t, exchange_code),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Last.Price", DX_RECORD_FIELD_SETTER_NAME(dx_trade_t, price), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_trade_t, price), DX_RECORD_FIELD_GETTER_NAME(dx_trade_t, price),
    dx_ft_common_field },

    { dx_fid_compact_int, L"Last.Size", DX_RECORD_FIELD_SETTER_NAME(dx_trade_t, size), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_trade_t, size), DX_RECORD_FIELD_GETTER_NAME(dx_trade_t, size),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Volume", DX_RECORD_FIELD_SETTER_NAME(dx_trade_t, day_volume), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_trade_t, day_volume), DX_RECORD_FIELD_GETTER_NAME(dx_trade_t, day_volume),
    dx_ft_common_field }
}; 

/* -------------------------------------------------------------------------- */
/*
 *	Quote data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_quote[] = {
    { dx_fid_compact_int, L"Bid.Time", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, bid_time), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, bid_time), DX_RECORD_FIELD_GETTER_NAME(dx_quote_t, bid_time),
    dx_ft_common_field },

    { dx_fid_utf_char, L"Bid.Exchange", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, bid_exchange_code), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, bid_exchange_code), DX_RECORD_FIELD_GETTER_NAME(dx_quote_t, bid_exchange_code),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Bid.Price", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, bid_price), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, bid_price), DX_RECORD_FIELD_GETTER_NAME(dx_quote_t, bid_price),
    dx_ft_common_field },

    { dx_fid_compact_int, L"Bid.Size", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, bid_size), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, bid_size), DX_RECORD_FIELD_GETTER_NAME(dx_quote_t, bid_size),
    dx_ft_common_field },

    { dx_fid_compact_int, L"Ask.Time", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, ask_time), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, ask_time), DX_RECORD_FIELD_GETTER_NAME(dx_quote_t, ask_time),
    dx_ft_common_field },

    { dx_fid_utf_char, L"Ask.Exchange", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, ask_exchange_code), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, ask_exchange_code), DX_RECORD_FIELD_GETTER_NAME(dx_quote_t, ask_exchange_code),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Ask.Price", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, ask_price), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, ask_price), DX_RECORD_FIELD_GETTER_NAME(dx_quote_t, ask_price),
    dx_ft_common_field },

    { dx_fid_compact_int, L"Ask.Size", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, ask_size), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, ask_size), DX_RECORD_FIELD_GETTER_NAME(dx_quote_t, ask_size),
    dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Fundamental data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_fundamental[] = { 
    { dx_fid_compact_int | dx_fid_flag_decimal, L"High.Price", DX_RECORD_FIELD_SETTER_NAME(dx_fundamental_t, day_high_price), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_fundamental_t, day_high_price), DX_RECORD_FIELD_GETTER_NAME(dx_fundamental_t, day_high_price),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Low.Price", DX_RECORD_FIELD_SETTER_NAME(dx_fundamental_t, day_low_price), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_fundamental_t, day_low_price), DX_RECORD_FIELD_GETTER_NAME(dx_fundamental_t, day_low_price),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Open.Price", DX_RECORD_FIELD_SETTER_NAME(dx_fundamental_t, day_open_price), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_fundamental_t, day_open_price), DX_RECORD_FIELD_GETTER_NAME(dx_fundamental_t, day_open_price),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Close.Price", DX_RECORD_FIELD_SETTER_NAME(dx_fundamental_t, prev_day_close_price), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_fundamental_t, prev_day_close_price), DX_RECORD_FIELD_GETTER_NAME(dx_fundamental_t, prev_day_close_price),
    dx_ft_common_field },

    { dx_fid_compact_int, L"OpenInterest", DX_RECORD_FIELD_SETTER_NAME(dx_fundamental_t, open_interest), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_fundamental_t, open_interest), DX_RECORD_FIELD_GETTER_NAME(dx_fundamental_t, open_interest),
    dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Profile data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_profile[] = { 
	{ dx_fid_utf_char_array, L"Description", DX_RECORD_FIELD_SETTER_NAME(dx_profile_t, description), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_profile_t, description), DX_RECORD_FIELD_GETTER_NAME(dx_profile_t, description),
    dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Market maker data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_market_maker[] = { 
	{ dx_fid_utf_char, L"MMExchange", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mm_exchange), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mm_exchange), DX_RECORD_FIELD_GETTER_NAME(dx_market_maker_t, mm_exchange), 
    dx_ft_first_time_int_field },

	{ dx_fid_compact_int, L"MMID", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mm_id), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mm_id), DX_RECORD_FIELD_GETTER_NAME(dx_market_maker_t, mm_id),
    dx_ft_second_time_int_field },

	{ dx_fid_compact_int, L"MMBid.Time", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmbid_time), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmbid_time), DX_RECORD_FIELD_GETTER_NAME(dx_market_maker_t, mmbid_time),
    dx_ft_common_field },

	{ dx_fid_compact_int | dx_fid_flag_decimal, L"MMBid.Price", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmbid_price), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmbid_price), DX_RECORD_FIELD_GETTER_NAME(dx_market_maker_t, mmbid_price),
    dx_ft_common_field },

	{ dx_fid_compact_int, L"MMBid.Size", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmbid_size), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmbid_size), DX_RECORD_FIELD_GETTER_NAME(dx_market_maker_t, mmbid_size),
    dx_ft_common_field },

	{ dx_fid_compact_int, L"MMAsk.Time", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmask_time), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmask_time), DX_RECORD_FIELD_GETTER_NAME(dx_market_maker_t, mmask_time),
    dx_ft_common_field },

	{ dx_fid_compact_int | dx_fid_flag_decimal, L"MMAsk.Price", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmask_price), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmask_price), DX_RECORD_FIELD_GETTER_NAME(dx_market_maker_t, mmask_price),
    dx_ft_common_field },

	{ dx_fid_compact_int, L"MMAsk.Size", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmask_size), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmask_size), DX_RECORD_FIELD_GETTER_NAME(dx_market_maker_t, mmask_size),
    dx_ft_common_field },

	{ dx_fid_compact_int, L"MMBid.Count", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmbid_count), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmbid_count), DX_RECORD_FIELD_GETTER_NAME(dx_market_maker_t, mmbid_count),
    dx_ft_common_field },

	{ dx_fid_compact_int, L"MMAsk.Count", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmask_count), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmask_count), DX_RECORD_FIELD_GETTER_NAME(dx_market_maker_t, mmask_count),
    dx_ft_common_field }
}; 

/* -------------------------------------------------------------------------- */
/*
 *	Order data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_order[] = { 
	{ dx_fid_compact_int, L"Index", DX_RECORD_FIELD_SETTER_NAME(dx_order_t, index), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_order_t, index), DX_RECORD_FIELD_GETTER_NAME(dx_order_t, index),
    dx_ft_second_time_int_field },

	{ dx_fid_compact_int, L"Time", DX_RECORD_FIELD_SETTER_NAME(dx_order_t, time), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_order_t, time), DX_RECORD_FIELD_GETTER_NAME(dx_order_t, time),
    dx_ft_common_field },

	{ dx_fid_compact_int, L"Sequence", DX_RECORD_FIELD_SETTER_NAME(dx_order_t, sequence), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_order_t, sequence), DX_RECORD_FIELD_GETTER_NAME(dx_order_t, sequence),
    dx_ft_common_field },

	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Price", DX_RECORD_FIELD_SETTER_NAME(dx_order_t, price), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_order_t, price), DX_RECORD_FIELD_GETTER_NAME(dx_order_t, price),
    dx_ft_common_field },

	{ dx_fid_compact_int, L"Size", DX_RECORD_FIELD_SETTER_NAME(dx_order_t, size), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_order_t, size), DX_RECORD_FIELD_GETTER_NAME(dx_order_t, size),
    dx_ft_common_field },

	{ dx_fid_compact_int, L"Flags", DX_RECORD_FIELD_SETTER_NAME(dx_order_t, flags), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_order_t, flags), DX_RECORD_FIELD_GETTER_NAME(dx_order_t, flags),
    dx_ft_common_field },

	{ dx_fid_compact_int, L"MMID", DX_RECORD_FIELD_SETTER_NAME(dx_order_t, mmid), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_order_t, mmid), DX_RECORD_FIELD_GETTER_NAME(dx_order_t, mmid),
    dx_ft_common_field },

	{ dx_fid_compact_int, L"Count", DX_RECORD_FIELD_SETTER_NAME(dx_order_t, count), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_order_t, count), DX_RECORD_FIELD_GETTER_NAME(dx_order_t, count),
    dx_ft_common_field }
}; 

/* -------------------------------------------------------------------------- */
/*
 *	Time and Sale data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_time_and_sale[] = {
    { dx_fid_compact_int, L"Time", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, time), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, time), DX_RECORD_FIELD_GETTER_NAME(dx_time_and_sale_t, time),
    dx_ft_first_time_int_field },

    { dx_fid_compact_int, L"Sequence", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, sequence), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, sequence), DX_RECORD_FIELD_GETTER_NAME(dx_time_and_sale_t, sequence),
    dx_ft_second_time_int_field },

    { dx_fid_utf_char, L"Exchange", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, exchange_code), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, exchange_code), DX_RECORD_FIELD_GETTER_NAME(dx_time_and_sale_t, exchange_code),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Price", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, price), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, price), DX_RECORD_FIELD_GETTER_NAME(dx_time_and_sale_t, price),
    dx_ft_common_field },

    { dx_fid_compact_int, L"Size", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, size), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, size), DX_RECORD_FIELD_GETTER_NAME(dx_time_and_sale_t, size),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Bid.Price", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, bid_price), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, bid_price), DX_RECORD_FIELD_GETTER_NAME(dx_time_and_sale_t, bid_price),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Ask.Price", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, ask_price), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, ask_price), DX_RECORD_FIELD_GETTER_NAME(dx_time_and_sale_t, ask_price),
    dx_ft_common_field },

    { dx_fid_compact_int, L"ExchangeSaleConditions", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, exch_sale_conds), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, exch_sale_conds), DX_RECORD_FIELD_GETTER_NAME(dx_time_and_sale_t, exch_sale_conds),
    dx_ft_common_field },

    { dx_fid_compact_int, L"Flags", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, type), 
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, type), DX_RECORD_FIELD_GETTER_NAME(dx_time_and_sale_t, type),
    dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
*	Candle data fields
*/
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_candle[] = {
    { dx_fid_compact_int | dx_fid_flag_time, L"Time", DX_RECORD_FIELD_SETTER_NAME(dx_candle_t, time),
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_candle_t, time), DX_RECORD_FIELD_GETTER_NAME(dx_candle_t, time),
    dx_ft_first_time_int_field },

    { dx_fid_compact_int | dx_fid_flag_sequence, L"Sequence", DX_RECORD_FIELD_SETTER_NAME(dx_candle_t, sequence),
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_candle_t, sequence), DX_RECORD_FIELD_GETTER_NAME(dx_candle_t, sequence),
    dx_ft_second_time_int_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Count", DX_RECORD_FIELD_SETTER_NAME(dx_candle_t, count),
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_candle_t, count), DX_RECORD_FIELD_GETTER_NAME(dx_candle_t, count),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Open", DX_RECORD_FIELD_SETTER_NAME(dx_candle_t, open),
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_candle_t, open), DX_RECORD_FIELD_GETTER_NAME(dx_candle_t, open),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"High", DX_RECORD_FIELD_SETTER_NAME(dx_candle_t, high),
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_candle_t, high), DX_RECORD_FIELD_GETTER_NAME(dx_candle_t, high),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Low", DX_RECORD_FIELD_SETTER_NAME(dx_candle_t, low),
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_candle_t, low), DX_RECORD_FIELD_GETTER_NAME(dx_candle_t, low),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Close", DX_RECORD_FIELD_SETTER_NAME(dx_candle_t, close),
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_candle_t, close), DX_RECORD_FIELD_GETTER_NAME(dx_candle_t, close),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Volume", DX_RECORD_FIELD_SETTER_NAME(dx_candle_t, volume),
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_candle_t, volume), DX_RECORD_FIELD_GETTER_NAME(dx_candle_t, volume),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"VWAP", DX_RECORD_FIELD_SETTER_NAME(dx_candle_t, vwap),
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_candle_t, vwap), DX_RECORD_FIELD_GETTER_NAME(dx_candle_t, vwap),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Bid.Volume", DX_RECORD_FIELD_SETTER_NAME(dx_candle_t, bid_volume),
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_candle_t, bid_volume), DX_RECORD_FIELD_GETTER_NAME(dx_candle_t, bid_volume),
    dx_ft_common_field },

    { dx_fid_compact_int | dx_fid_flag_decimal, L"Ask.Volume", DX_RECORD_FIELD_SETTER_NAME(dx_candle_t, ask_volume),
    DX_RECORD_FIELD_DEF_VAL_NAME(dx_candle_t, ask_volume), DX_RECORD_FIELD_GETTER_NAME(dx_candle_t, ask_volume),
    dx_ft_common_field }
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
    sizeof(dx_fields_order) / sizeof(dx_fields_order[0]),
    sizeof(dx_fields_time_and_sale) / sizeof(dx_fields_time_and_sale[0]),
    sizeof(dx_fields_candle) / sizeof(dx_fields_candle[0])
};

static const dx_record_info_t g_record_info[dx_rid_count] = {
    { L"Trade", sizeof(dx_fields_trade) / sizeof(dx_fields_trade[0]), dx_fields_trade },
    { L"Quote", sizeof(dx_fields_quote) / sizeof(dx_fields_quote[0]), dx_fields_quote },
    { L"Fundamental", sizeof(dx_fields_fundamental) / sizeof(dx_fields_fundamental[0]), dx_fields_fundamental },
    { L"Profile", sizeof(dx_fields_profile) / sizeof(dx_fields_profile[0]), dx_fields_profile },
    { L"MarketMaker", sizeof(dx_fields_market_maker) / sizeof(dx_fields_market_maker[0]), dx_fields_market_maker },
    { L"Order", sizeof(dx_fields_order) / sizeof(dx_fields_order[0]), dx_fields_order },
    { L"TimeAndSale", sizeof(dx_fields_time_and_sale) / sizeof(dx_fields_time_and_sale[0]), dx_fields_time_and_sale },
    { L"Candle", sizeof(dx_fields_candle) / sizeof(dx_fields_candle[0]), dx_fields_candle }
};

/* List stores records. The list is not cleared until at least one connection is opened. */
static dx_record_list_t g_records_list = { NULL, 0, 0, 0 };
static dx_mutex_t guard;
static bool guard_is_initialized = false;

/* -------------------------------------------------------------------------- */
/*
 *	Data structures connection context
 */
/* -------------------------------------------------------------------------- */

#define RECORD_ID_VECTOR_SIZE   1000

typedef struct {
    dxf_int_t server_record_id;
    dx_record_id_t local_record_id;
} dx_record_id_pair_t;

typedef struct {
    dx_record_id_pair_t* elements;
    int size;
    int capacity;
} dx_record_id_map_t;

typedef struct {
    dx_record_id_t frequent_ids[RECORD_ID_VECTOR_SIZE];
    dx_record_id_map_t id_map;
} dx_server_local_record_id_map_t;

typedef struct {
    dxf_connection_t connection;
    
    dx_server_local_record_id_map_t record_id_map;
    dx_record_server_support_state_list_t record_server_support_states;
} dx_data_structures_connection_context_t;

#define CTX(context) \
    ((dx_data_structures_connection_context_t*)context)

/* -------------------------------------------------------------------------- */

void dx_clear_record_server_support_states(dx_data_structures_connection_context_t* context) {
    dx_record_server_support_state_list_t *states = NULL;
    if (context == NULL)
        return;
    states = &(context->record_server_support_states);
    if (states->elements == NULL)
        return;

    dx_free(states->elements);
    states->elements = NULL;
    states->size = 0;
    states->capacity = 0;
}

void dx_free_data_structures_context(dx_data_structures_connection_context_t** context) {
    if (context == NULL)
        return;
    dx_clear_record_server_support_states(*context);
    dx_free(*context);
    context = NULL;
}

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_data_structures) {
    dx_data_structures_connection_context_t* context = dx_calloc(1, sizeof(dx_data_structures_connection_context_t));
    int i = 0;
    
    if (context == NULL) {
        return false;
    }
    
    context->connection = connection;
    context->record_server_support_states.elements = NULL;
    context->record_server_support_states.size = 0;
    context->record_server_support_states.capacity = 0;
    
    for (; i < RECORD_ID_VECTOR_SIZE; ++i) {
        context->record_id_map.frequent_ids[i] = DX_RECORD_ID_INVALID;
    }
    
    if (!dx_set_subsystem_data(connection, dx_ccs_data_structures, context)) {
        dx_free_data_structures_context(&context);

        return false;
    }
    
    return true;
}

void dx_init_records_list_guard() {
    if (!guard_is_initialized) {
        dx_mutex_create(&guard);
        guard_is_initialized = true;
    }
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_data_structures) {
    bool res = true;
    dx_data_structures_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_data_structures, &res);
    
    if (context == NULL) {
        return res;
    }
    
    CHECKED_FREE(context->record_id_map.id_map.elements);
    dx_free_data_structures_context(&context);
    
    return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_data_structures) {
    return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Connection context functions implementation
 */
/* -------------------------------------------------------------------------- */

void* dx_get_data_structures_connection_context (dxf_connection_t connection) {
    return dx_get_subsystem_data(connection, dx_ccs_data_structures, NULL);
}

/* -------------------------------------------------------------------------- */
/*
 *	Record functions implementation
 */
/* -------------------------------------------------------------------------- */

#define RECORD_ID_PAIR_COMPARATOR(left, right) \
    DX_NUMERIC_COMPARATOR((left).server_record_id, (right).server_record_id)

dx_record_id_t dx_get_record_id(void* context, dxf_int_t server_record_id) {
    dx_server_local_record_id_map_t* record_id_map = &(CTX(context)->record_id_map);
        
    if (server_record_id >= 0 && server_record_id < RECORD_ID_VECTOR_SIZE) {
        return record_id_map->frequent_ids[server_record_id];
    } else {
        dx_record_id_t idx;
        bool found = false;
        dx_record_id_pair_t dummy;
        
        dummy.server_record_id = server_record_id;
        
        DX_ARRAY_SEARCH(record_id_map->id_map.elements, 0, record_id_map->id_map.size, dummy, RECORD_ID_PAIR_COMPARATOR, true, found, idx);
        
        if (!found) {
            return DX_RECORD_ID_INVALID;
        }
        
        return record_id_map->id_map.elements[idx].local_record_id;
    }
}

/* -------------------------------------------------------------------------- */

bool dx_assign_server_record_id(void* context, dx_record_id_t record_id, dxf_int_t server_record_id) {
    dx_server_local_record_id_map_t* record_id_map = &(CTX(context)->record_id_map);
        
    if (server_record_id >= 0 && server_record_id < RECORD_ID_VECTOR_SIZE) {
        record_id_map->frequent_ids[server_record_id] = record_id;
    } else {
        int idx;
        bool found = false;
        bool failed = false;
        dx_record_id_pair_t rip;
        
        rip.server_record_id = server_record_id;
        rip.local_record_id = record_id;
        
        DX_ARRAY_SEARCH(record_id_map->id_map.elements, 0, record_id_map->id_map.size, rip, RECORD_ID_PAIR_COMPARATOR, true, found, idx);
        
        if (found) {
            record_id_map->id_map.elements[idx].local_record_id = record_id;
            
            return true;
        }
        
        DX_ARRAY_INSERT(record_id_map->id_map, dx_record_id_pair_t, rip, idx, dx_capacity_manager_halfer, failed);
        
        if (failed) {
            return false;
        }
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

/* Don't try to change any field of struct. You shouldn't free this resources */
const dx_record_item_t* dx_get_record_by_id(dx_record_id_t record_id) {
    dx_record_item_t* value = NULL;
    dx_init_records_list_guard();
    dx_mutex_lock(&guard);
    value = &g_records_list.elements[record_id];
    dx_mutex_unlock(&guard);
    return value;
}

/* -------------------------------------------------------------------------- */

dx_record_id_t dx_get_record_id_by_name(dxf_const_string_t record_name) {
    dx_record_id_t record_id = 0;

    dx_init_records_list_guard();
    dx_mutex_lock(&guard);

    for (; record_id < g_records_list.size; ++record_id) {
        if (dx_compare_strings(g_records_list.elements[record_id].name, record_name) == 0) {
            dx_mutex_unlock(&guard);
            return record_id;
        }
    }

    dx_mutex_unlock(&guard);

    return DX_RECORD_ID_INVALID;
}

/* -------------------------------------------------------------------------- */

dx_record_id_t dx_get_next_unsubscribed_record_id(bool isUpdate) {
    dx_record_id_t record_id = DX_RECORD_ID_INVALID;

    dx_init_records_list_guard();
    dx_mutex_lock(&guard);

    if (g_records_list.new_record_id < g_records_list.size && isUpdate) {
        g_records_list.new_record_id += 1;
    }

    record_id = g_records_list.new_record_id;

    dx_mutex_unlock(&guard);

    return record_id;
}

/* -------------------------------------------------------------------------- */

void dx_drop_unsubscribe_counter() {
    dx_init_records_list_guard();
    dx_mutex_lock(&guard);

    g_records_list.new_record_id = 0;

    dx_mutex_unlock(&guard);
}

/* -------------------------------------------------------------------------- */

int dx_find_record_field(const dx_record_item_t* record_info, dxf_const_string_t field_name,
                          dxf_int_t field_type) {
    int cur_field_index = 0;
    dx_field_info_t* fields = (dx_field_info_t*)record_info->fields;
    
    for (; cur_field_index < record_info->field_count; ++cur_field_index) {
        if (dx_compare_strings(fields[cur_field_index].name, field_name) == 0 &&
            (fields[cur_field_index].type & dx_fid_mask_serialization)== (field_type & dx_fid_mask_serialization)) {
            return cur_field_index;
        }
    }
    
    return INVALID_INDEX;
}

/* -------------------------------------------------------------------------- */

dxf_char_t dx_get_record_exchange_code(dx_record_id_t record_id) {
    dxf_char_t exchange_code = 0;

    dx_init_records_list_guard();
    dx_mutex_lock(&guard);

    if (record_id >= 0 && record_id < g_records_list.size)
        exchange_code = g_records_list.elements[record_id].exchange_code;

    dx_mutex_unlock(&guard);

    return exchange_code;
}

bool dx_set_record_exchange_code(dx_record_id_t record_id, dxf_char_t exchange_code) {
    if (record_id < 0 || record_id > g_records_list.size)
        return false;

    dx_init_records_list_guard();
    dx_mutex_lock(&guard);

    g_records_list.elements[record_id].exchange_code = exchange_code;

    dx_mutex_unlock(&guard);

    return true;
}

/* -------------------------------------------------------------------------- */

dx_record_server_support_state_list_t* dx_get_record_server_support_states(void* context) {
    return &(CTX(context)->record_server_support_states);
}

bool dx_get_record_server_support_state_value(dx_record_server_support_state_list_t* states, 
    dx_record_id_t record_id,
                                              OUT dx_record_server_support_state_t **value) {
    if (record_id < 0 || record_id >= states->size)
        return false;
    *value = &(states->elements[record_id]);
    return true;
}

/* -------------------------------------------------------------------------- */

/* Functions for working with records list */

bool dx_add_record_to_list(dxf_connection_t connection, dx_record_item_t record, dx_record_id_t index) {
    bool failed = false;
    dx_record_item_t new_record;
    dx_record_server_support_state_t new_state;
    dx_data_structures_connection_context_t* dscc = NULL;

    /* Update records list */
    new_record.name = dx_create_string_src(record.name);
    new_record.field_count = record.field_count;
    new_record.fields = record.fields;
    new_record.info_id = record.info_id;
    dx_memcpy(new_record.suffix, record.suffix, sizeof(record.suffix));
    new_record.exchange_code = record.exchange_code;

    DX_ARRAY_INSERT(g_records_list, dx_record_item_t, new_record, index, dx_capacity_manager_halfer, failed);

    if (failed) {
        return dx_set_error_code(dx_sec_not_enough_memory);
    }

    /* Update record server support states */
    dscc = dx_get_data_structures_connection_context(connection);
    if (dscc == NULL) {
        DX_ARRAY_DELETE(g_records_list, dx_record_item_t, index, dx_capacity_manager_halfer, failed);
        return dx_set_error_code(dx_cec_connection_context_not_initialized);
    }

    new_state = 0;
    DX_ARRAY_INSERT(dscc->record_server_support_states, dx_record_server_support_state_t, new_state, index, dx_capacity_manager_halfer, failed);

    if (failed) {
        DX_ARRAY_DELETE(g_records_list, dx_record_item_t, index, dx_capacity_manager_halfer, failed);
        return dx_set_error_code(dx_sec_not_enough_memory);
    }

    /* Update record digests */
    if (!dx_add_record_digest_to_list(connection, index)) {
        DX_ARRAY_DELETE(g_records_list, dx_record_item_t, index, dx_capacity_manager_halfer, failed);
        DX_ARRAY_DELETE(dscc->record_server_support_states, dx_record_server_support_state_t, index, dx_capacity_manager_halfer, failed);
        return false;
    }

    return true;
}

dx_record_info_id_t dx_string_to_record_info(dxf_const_string_t name)
{
    if (dx_compare_strings_num(name, g_record_info[dx_rid_trade].default_name, 
                               dx_string_length(g_record_info[dx_rid_trade].default_name)) == 0)
        return dx_rid_trade;
    else if (dx_compare_strings_num(name, g_record_info[dx_rid_quote].default_name, 
                                    dx_string_length(g_record_info[dx_rid_quote].default_name)) == 0)
        return dx_rid_quote;
    else if (dx_compare_strings_num(name, g_record_info[dx_rid_fundamental].default_name, 
                                    dx_string_length(g_record_info[dx_rid_fundamental].default_name)) == 0)
        return dx_rid_fundamental;
    else if (dx_compare_strings_num(name, g_record_info[dx_rid_profile].default_name, 
                                    dx_string_length(g_record_info[dx_rid_profile].default_name)) == 0)
        return dx_rid_profile;
    else if (dx_compare_strings_num(name, g_record_info[dx_rid_market_maker].default_name, 
                                    dx_string_length(g_record_info[dx_rid_market_maker].default_name)) == 0)
        return dx_rid_market_maker;
    else if (dx_compare_strings_num(name, g_record_info[dx_rid_order].default_name, 
                                    dx_string_length(g_record_info[dx_rid_order].default_name)) == 0)
        return dx_rid_order;
    else if (dx_compare_strings_num(name, g_record_info[dx_rid_time_and_sale].default_name, 
                                    dx_string_length(g_record_info[dx_rid_time_and_sale].default_name)) == 0)
        return dx_rid_time_and_sale;
    else if (dx_compare_strings_num(name, g_record_info[dx_rid_candle].default_name,
                                    dx_string_length(g_record_info[dx_rid_candle].default_name)) == 0)
        return dx_rid_candle;
    else
        return dx_rid_invalid;
}

bool init_record_info(dx_record_item_t *record, dxf_const_string_t name) {
    dx_record_info_id_t record_info_id;
    dx_record_info_t record_info;
    int suffix_index;
    int name_length = dx_string_length(name);

    record_info_id = dx_string_to_record_info(name);
    if (record_info_id == dx_rid_invalid) {
        dx_set_last_error(dx_ec_invalid_func_param_internal);
        return false;
    }
    record_info = g_record_info[record_info_id];

    record->name = dx_create_string_src(name);
    record->field_count = g_record_info[record_info_id].field_count;
    record->fields = g_record_info[record_info_id].fields;
    record->info_id = record_info_id;
    dx_memset(record->suffix, 0, sizeof(record->suffix));
    record->exchange_code = 0;

    suffix_index = dx_string_length(record_info.default_name);
    if (name_length < suffix_index + 1)
        return true;
    if (record_info_id == dx_rid_order) {
        if (record->name[suffix_index] != L'#')
            return true;
        dx_copy_string_len(record->suffix, &record->name[suffix_index + 1], name_length - suffix_index);
    } else if (record_info_id == dx_rid_trade || record_info_id == dx_rid_quote) {
        if (record->name[suffix_index] != L'&')
            return true;
        dx_copy_string_len(record->suffix, &record->name[suffix_index + 1], 1);
        record->exchange_code = record->suffix[0];
    }

    return true;
}

void clear_record_info(dx_record_item_t *record) {
    dx_free(record->name);
    record->name = NULL;
}

#define DX_RECORDS_COMPARATOR(l, r) (dx_compare_strings(l.name, r.name))

dx_record_id_t dx_add_or_get_record_id(dxf_connection_t connection, dxf_const_string_t name) {
    bool result = true;
    bool found = false;
    dx_record_id_t index = DX_RECORD_ID_INVALID;
    dx_record_item_t record;

    dx_init_records_list_guard();
    dx_mutex_lock(&guard);

    if (!init_record_info(&record, name)) {
        dx_mutex_unlock(&guard);
        return DX_RECORD_ID_INVALID;
    }
    
    if (g_records_list.elements == NULL) {
        index = 0;
        result = dx_add_record_to_list(connection, record, index);
    } else {
        DX_ARRAY_SEARCH(g_records_list.elements, 0, g_records_list.size, record, DX_RECORDS_COMPARATOR, false, found, index);
        if (!found) {
            result = dx_add_record_to_list(connection, record, index);
        }
    }

    clear_record_info(&record);

    dx_mutex_unlock(&guard);

    if (!result) {
        dx_logging_last_error();
        return DX_RECORD_ID_INVALID;
    }
        
    return index;
}

void dx_clear_records_list() {
    dx_record_id_t i = 0;
    dx_record_item_t *record = g_records_list.elements;

    dx_init_records_list_guard();
    dx_mutex_lock(&guard);

    for (; i < g_records_list.size; i++) {
        clear_record_info(record);
        record++;
    }
    dx_free(g_records_list.elements);
    g_records_list.elements = NULL;
    g_records_list.capacity = 0;
    g_records_list.size = 0;
    g_records_list.new_record_id = 0;

    dx_mutex_unlock(&guard);

    dx_mutex_destroy(&guard);
    guard_is_initialized = false;
}

int dx_get_records_list_count() {
    int size = 0;

    dx_init_records_list_guard();
    dx_mutex_lock(&guard);

    size = g_records_list.size;

    dx_mutex_unlock(&guard);

    return size;
}