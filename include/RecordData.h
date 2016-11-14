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
 
/*
 *	Here we have what's related to the records we receive from the server.
 */
 
#ifndef RECORD_DATA_H_INCLUDED
#define RECORD_DATA_H_INCLUDED

#include "DXTypes.h"

/* -------------------------------------------------------------------------- */
/*
 *	Record type constants
 */
/* -------------------------------------------------------------------------- */

typedef enum {
    dx_rid_begin,
    dx_rid_trade = dx_rid_begin,
    dx_rid_quote,
    dx_rid_summary,
    dx_rid_profile,
    dx_rid_market_maker,
    dx_rid_order,
    dx_rid_time_and_sale,
    dx_rid_candle,
    dx_rid_trade_eth,
    dx_rid_spread_order,
    dx_rid_greeks,
    dx_rid_theo_price,
    dx_rid_underlying,
    dx_rid_series,

    /* add new values above this line */

    dx_rid_count,
    dx_rid_invalid
} dx_record_info_id_t;

typedef int dx_record_id_t;

static const dxf_int_t DX_RECORD_SUFFIX_MASK = 0x7C00;
static const dxf_int_t DX_RECORD_SUFFIX_IN_FLAG_SHIFT = 10;
static const dxf_int_t DX_RECORD_SUFFIX_HIGH_BITS = 0x40;

/* Invalid or empty record id */
static const dx_record_id_t DX_RECORD_ID_INVALID = -1;

/* -------------------------------------------------------------------------- */
/*
 *	Record structures
 */
/* -------------------------------------------------------------------------- */

typedef struct {
    dxf_long_t time;
    dxf_char_t exchange_code;
    dxf_double_t price;
    dxf_long_t size;
    dxf_long_t tick;
    dxf_double_t change;
    dxf_double_t day_volume;	
} dx_trade_t;

typedef struct {
    dxf_long_t bid_time;
    dxf_char_t bid_exchange_code;
    dxf_double_t bid_price;
    dxf_long_t bid_size;
    dxf_long_t ask_time;
    dxf_char_t ask_exchange_code;
    dxf_double_t ask_price;
    dxf_long_t ask_size;
} dx_quote_t;

typedef struct {
    dxf_dayid_t day_id;
    dxf_double_t day_open_price;
    dxf_double_t day_high_price;
    dxf_double_t day_low_price;
    dxf_double_t day_close_price;
    dxf_dayid_t prev_day_id;
    dxf_double_t prev_day_close_price;
    dxf_long_t open_interest;
    dxf_long_t flags;
    dxf_char_t exchange_code;
    /* Note: calculated fields */
    dxf_byte_t day_close_price_type;
    dxf_byte_t prev_day_close_price_type;
} dx_summary_t;

typedef struct {
    dxf_double_t beta;
    dxf_double_t eps;
    dxf_long_t div_freq;
    dxf_double_t exd_div_amount;
    dxf_dayid_t exd_div_date;
    dxf_double_t _52_high_price;
    dxf_double_t _52_low_price;
    dxf_double_t shares;
    dxf_const_string_t description;
    dxf_long_t flags;
    dxf_const_string_t status_reason;
    dxf_long_t halt_start_time;
    dxf_long_t halt_end_time;
    dxf_double_t high_limit_price;
    dxf_double_t low_limit_price;
} dx_profile_t;

typedef struct {
    dxf_char_t mm_exchange;
    dxf_int_t mm_id;
    dxf_int_t mmbid_time;
    dxf_double_t mmbid_price;
    dxf_int_t mmbid_size;
    dxf_int_t mmask_time;
    dxf_double_t mmask_price;
    dxf_int_t mmask_size;
    dxf_int_t mmbid_count;
    dxf_int_t mmask_count;
} dx_market_maker_t;

typedef struct {
    dxf_int_t index;
    dxf_int_t time;
    dxf_int_t sequence;
    dxf_double_t price;
    dxf_int_t size;
    dxf_int_t flags;
    dxf_int_t mmid;
    dxf_int_t count;
} dx_order_t;

typedef struct {
    dxf_long_t event_id;
    dxf_long_t time;
    dxf_int_t sequence;
    dxf_char_t exchange_code;
    dxf_double_t price;
    dxf_long_t size;
    dxf_double_t bid_price;
    dxf_double_t ask_price;
    dxf_const_string_t exchange_sale_conditions;
    dxf_int_t flags;

    dxf_int_t event_flags;
    dxf_long_t index;
    dxf_int_t side;
    dxf_bool_t is_spread_leg;
    dxf_bool_t is_trade;
    dxf_bool_t is_valid_tick;
    dxf_int_t type;
} dx_time_and_sale_t;

typedef struct {
    dxf_long_t time;
    dxf_int_t sequence;
    dxf_double_t count;
    dxf_double_t open;
    dxf_double_t high;
    dxf_double_t low;
    dxf_double_t close;
    dxf_double_t volume;
    dxf_double_t vwap;
    dxf_double_t bid_volume;
    dxf_double_t ask_volume;
    /* Note: next two fields open_interest and imp_volatility introduced for 
       daily candle */
    dxf_double_t open_interest;
    dxf_double_t imp_volatility;
    /* Note: calculated fields */
    dxf_long_t index;
} dx_candle_t;

typedef struct {
    dxf_long_t time;
    dxf_int_t flags;
    dxf_char_t exchange_code;
    dxf_double_t price;
    dxf_long_t size;
    dxf_double_t eth_volume;
} dx_trade_eth_t;

typedef struct {
    dxf_int_t index;
    dxf_int_t time;
    dxf_int_t sequence;
    dxf_double_t price;
    dxf_int_t size;
    dxf_int_t count;
    dxf_int_t flags;
    dxf_const_string_t spread_symbol;
} dx_spread_order_t;

typedef struct {
    dxf_long_t time;
    dxf_int_t sequence;
    dxf_double_t greeks_price;
    dxf_double_t volatility;
    dxf_double_t delta;
    dxf_double_t gamma;
    dxf_double_t theta;
    dxf_double_t rho;
    dxf_double_t vega;
    /* Note: calculated fields */
    dxf_long_t index;
} dx_greeks_t;

typedef struct {
    dxf_long_t theo_time;
    dxf_double_t theo_price;
    dxf_double_t theo_underlying_price;
    dxf_double_t theo_delta;
    dxf_double_t theo_gamma;
    dxf_double_t theo_dividend;
    dxf_double_t theo_interest;
} dx_theo_price_t;

typedef struct {
    dxf_double_t volatility;
    dxf_double_t front_volatility;
    dxf_double_t back_volatility;
    dxf_double_t put_call_ratio;
} dx_underlying_t;

typedef struct {
    dxf_dayid_t expiration;
    dxf_int_t sequence;
    dxf_double_t volatility;
    dxf_double_t put_call_ratio;
    dxf_double_t forward_price;
    dxf_double_t dividend;
    dxf_double_t interest;
    /* Note: calculated fields */
    dxf_long_t index;
} dx_series_t;

#endif /* RECORD_DATA_H_INCLUDED */