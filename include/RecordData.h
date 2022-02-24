/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 2.0 (the "License"); you may not use this file except in compliance with
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

/**
 * @file
 * @brief dxFeed C API domain types declarations
 */

/* -------------------------------------------------------------------------- */
/*
 *	Record type constants
 */
/* -------------------------------------------------------------------------- */

/// Record info ID
typedef enum {
    dx_rid_begin = 0,
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
    dx_rid_configuration,

    /* add new values above this line */

    dx_rid_count,
    dx_rid_invalid
} dx_record_info_id_t;

/// Record ID
typedef dxf_int_t dx_record_id_t;

/* Invalid or empty record id */
static const dx_record_id_t DX_RECORD_ID_INVALID = -1;

/* -------------------------------------------------------------------------- */
/*
 *	Record structures
 */
/* -------------------------------------------------------------------------- */

/// Trade
typedef struct dx_trade {
    dxf_int_t time;
    dxf_int_t sequence;
    dxf_int_t time_nanos;
    dxf_char_t exchange_code;
    dxf_double_t price;
	dxf_double_t size;
    dxf_int_t tick;
    dxf_double_t change;
	dxf_dayid_t day_id;
    dxf_double_t day_volume;
    dxf_double_t day_turnover;
	dxf_int_t flags;
} dx_trade_t;

/// TradeETH
typedef dx_trade_t dx_trade_eth_t;

/// Quote
typedef struct dx_quote {
    dxf_int_t sequence;
    dxf_int_t time_nanos;
    dxf_int_t bid_time;
    dxf_char_t bid_exchange_code;
    dxf_double_t bid_price;
	dxf_double_t bid_size;
    dxf_int_t ask_time;
    dxf_char_t ask_exchange_code;
    dxf_double_t ask_price;
	dxf_double_t ask_size;
} dx_quote_t;

/// Summary
typedef struct dx_summary {
    dxf_dayid_t day_id;	
    dxf_double_t day_open_price;
    dxf_double_t day_high_price;
    dxf_double_t day_low_price;
    dxf_double_t day_close_price;
    dxf_dayid_t prev_day_id;
    dxf_double_t prev_day_close_price;
    dxf_double_t prev_day_volume;
	dxf_double_t open_interest;
    dxf_int_t flags;
} dx_summary_t;

/// Profile
typedef struct dx_profile {
    dxf_double_t beta;
    dxf_double_t eps;
	dxf_double_t div_freq;
    dxf_double_t exd_div_amount;
    dxf_dayid_t exd_div_date;
    dxf_double_t high_price_52;
    dxf_double_t low_price_52;
    dxf_double_t shares;
    dxf_double_t free_float;
    dxf_double_t high_limit_price;
    dxf_double_t low_limit_price;
    dxf_int_t halt_start_time;
    dxf_int_t halt_end_time;
    dxf_int_t flags;
    dxf_const_string_t description;
    dxf_const_string_t status_reason;
} dx_profile_t;

/// Market maker
typedef struct dx_market_maker {
    dxf_char_t mm_exchange;
    dxf_int_t mm_id;
    dxf_int_t mmbid_time;
    dxf_double_t mmbid_price;
	dxf_double_t mmbid_size;
	dxf_double_t mmbid_count;
    dxf_int_t mmask_time;
    dxf_double_t mmask_price;
	dxf_double_t mmask_size;
	dxf_double_t mmask_count;
} dx_market_maker_t;

/// Order
typedef struct dx_order {
    dxf_int_t index;
    dxf_int_t time;
    dxf_int_t time_nanos;
    dxf_int_t sequence;
	dxf_long_t action_time;
	dxf_long_t order_id;
	dxf_long_t aux_order_id;
    dxf_double_t price;
	dxf_double_t size;
	dxf_double_t executed_size;
	dxf_double_t count;
    dxf_int_t flags;
	dxf_long_t trade_id;
	dxf_double_t trade_price;
	dxf_double_t trade_size;
    dxf_int_t mmid;
} dx_order_t;

/// Spread order
typedef struct dx_spread_order {
    dxf_int_t index;
    dxf_int_t time;
    dxf_int_t time_nanos;
    dxf_int_t sequence;
	dxf_long_t action_time;
	dxf_long_t order_id;
	dxf_long_t aux_order_id;
    dxf_double_t price;
	dxf_double_t size;
	dxf_double_t executed_size;
	dxf_double_t count;
    dxf_int_t flags;
	dxf_long_t trade_id;
	dxf_double_t trade_price;
	dxf_double_t trade_size;
    dxf_const_string_t spread_symbol;
} dx_spread_order_t;

/// Time & sale
typedef struct dx_time_and_sale {
    dxf_int_t time;
    dxf_int_t sequence;
	dxf_int_t time_nanos;
    dxf_char_t exchange_code;
    dxf_double_t price;
	dxf_double_t size;
    dxf_double_t bid_price;
    dxf_double_t ask_price;
    dxf_int_t exchange_sale_conditions;
    dxf_int_t flags;
    dxf_const_string_t buyer;
    dxf_const_string_t seller;
} dx_time_and_sale_t;

/// Candle
typedef struct dx_candle {
    dxf_int_t time;
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
	dxf_double_t open_interest;
    dxf_double_t imp_volatility;
} dx_candle_t;

/// Greeks
typedef struct dx_greeks {
    dxf_int_t time;
    dxf_int_t sequence;
    dxf_double_t price;
    dxf_double_t volatility;
    dxf_double_t delta;
    dxf_double_t gamma;
    dxf_double_t theta;
    dxf_double_t rho;
    dxf_double_t vega;
} dx_greeks_t;

/**
 * @addtogroup event-data-structures-theo-price
 * @{
 */

/**
 * @brief Theo price
 *
 * @details Theo price is a snapshot of the theoretical option price computation that is periodically performed by
 * dxPrice model-free computation. It represents the most recent information that is available about the corresponding
 * values at any given moment of time. The values include first and second order derivative of the price curve by
 * price, so that the real-time theoretical option price can be estimated on real-time changes of the underlying price
 * in the vicinity.
 */
typedef struct dx_theo_price {
    // To have same record for record and event
	/// Timestamp of this event in milliseconds
    dxf_long_t time;
	/// Theoretical option price
    dxf_double_t price;
	/// Underlying price at the time of theo price computation
    dxf_double_t underlying_price;
	/// Delta of the theoretical price
    dxf_double_t delta;
	/// Gamma of the theoretical price
    dxf_double_t gamma;
	/// Implied simple dividend return of the corresponding option series
    dxf_double_t dividend;
	/// Implied simple interest return of the corresponding option series
    dxf_double_t interest;
} dx_theo_price_t;

///@}

/// Underlying
typedef struct dx_underlying {
    dxf_double_t volatility;
    dxf_double_t front_volatility;
    dxf_double_t back_volatility;
	dxf_double_t call_volume;
	dxf_double_t put_volume;
    dxf_double_t put_call_ratio;
} dx_underlying_t;

/// Series
typedef struct dx_series {
    dxf_int_t index;
    dxf_int_t time;
    dxf_int_t sequence;
    dxf_dayid_t expiration;
    dxf_double_t volatility;
	dxf_double_t call_volume;
	dxf_double_t put_volume;
    dxf_double_t put_call_ratio;
    dxf_double_t forward_price;
    dxf_double_t dividend;
    dxf_double_t interest;
} dx_series_t;

/// Configuration
typedef struct dx_configuration {
    dxf_int_t version;
    dxf_byte_array_t object;
} dx_configuration_t;

#endif /* RECORD_DATA_H_INCLUDED */
