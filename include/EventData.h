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
 *  Here we have the data structures passed along with symbol events
 */

/**
 * @file
 * @brief dxFeed C API event data structures declarations
 */

#ifndef EVENT_DATA_H_INCLUDED
#define EVENT_DATA_H_INCLUDED

#include "RecordData.h"
#include "DXTypes.h"
#include <limits.h>
#include <math.h>

#ifndef OUT
    #define OUT
#endif /* OUT */

/* -------------------------------------------------------------------------- */
/*
 *	Event type constants
 */
/* -------------------------------------------------------------------------- */

/// Event ID
typedef enum {
    dx_eid_begin = 0,
    dx_eid_trade = dx_eid_begin,
    dx_eid_quote,
    dx_eid_summary,
    dx_eid_profile,
    dx_eid_order,
    dx_eid_time_and_sale,
    dx_eid_candle,
    dx_eid_trade_eth,
    dx_eid_spread_order,
    dx_eid_greeks,
    dx_eid_theo_price,
    dx_eid_underlying,
    dx_eid_series,
    dx_eid_configuration,

    /* add new event id above this line */

    dx_eid_count,
    dx_eid_invalid
} dx_event_id_t;

/// Trade event
#define DXF_ET_TRADE         (1 << dx_eid_trade)
/// Quote event
#define DXF_ET_QUOTE         (1 << dx_eid_quote)
/// Summary event
#define DXF_ET_SUMMARY       (1 << dx_eid_summary)
/// Profile event
#define DXF_ET_PROFILE       (1 << dx_eid_profile)
/// Order event
#define DXF_ET_ORDER         (1 << dx_eid_order)
/// Time & sale event
#define DXF_ET_TIME_AND_SALE (1 << dx_eid_time_and_sale)
/// Candle event
#define DXF_ET_CANDLE        (1 << dx_eid_candle)
/// Trade eth event
#define DXF_ET_TRADE_ETH     (1 << dx_eid_trade_eth)
/// Spread order event
#define DXF_ET_SPREAD_ORDER  (1 << dx_eid_spread_order)
/// Greeks event
#define DXF_ET_GREEKS        (1 << dx_eid_greeks)
/// Theo price event
#define DXF_ET_THEO_PRICE    (1 << dx_eid_theo_price)
/// Underlying event
#define DXF_ET_UNDERLYING    (1 << dx_eid_underlying)
/// Series event
#define DXF_ET_SERIES        (1 << dx_eid_series)
/// Configuration event
#define DXF_ET_CONFIGURATION (1 << dx_eid_configuration)
#define DXF_ET_UNUSED        (~((1 << dx_eid_count) - 1))

/// Event bit-mask
#define DX_EVENT_BIT_MASK(event_id) (1 << event_id)

/**
 * Event Subscription flags
 */
typedef enum {
  ///Used for default subscription
  dx_esf_default = 0x0,
  ///Used for subscribing on one record only in case of snapshots
  dx_esf_single_record = 0x1,
  ///Used with \a dx_esf_single_record flag and for \a dx_eid_order (Order) event
  dx_esf_sr_market_maker_order = 0x2,
  ///Used for time series subscription
  dx_esf_time_series = 0x4,
  ///Used for regional quotes
  dx_esf_quotes_regional = 0x8,
  ///Used for wildcard ("*") subscription
  dx_esf_wildcard = 0x10,
  ///Used for forcing subscription to ticker data
  dx_esf_force_ticker = 0x20,
  ///Used for forcing subscription to stream data
  dx_esf_force_stream = 0x40,
  ///Used for forcing subscription to history data
  dx_esf_force_history = 0x80,

  dx_esf_force_enum_unsigned = UINT_MAX
} dx_event_subscr_flag;

// The length of record suffix including including the terminating null character 
#define DXF_RECORD_SUFFIX_SIZE 5

/* -------------------------------------------------------------------------- */
/*
*	Source suffix array
*/
/* -------------------------------------------------------------------------- */

/// Suffix
typedef struct {
    dxf_char_t suffix[DXF_RECORD_SUFFIX_SIZE];
} dx_suffix_t;

/// Order source
typedef struct {
    dx_suffix_t *elements;
    size_t size;
    size_t capacity;
} dx_order_source_array_t;

/// Order source array
typedef dx_order_source_array_t* dx_order_source_array_ptr_t;

/* -------------------------------------------------------------------------- */
/*
 *	Event data structures and support
 */
/* -------------------------------------------------------------------------- */

/// Event data
typedef void* dxf_event_data_t;

/// Order scope
typedef enum {
	dxf_osc_composite = 0,
	dxf_osc_regional = 1,
	dxf_osc_aggregate = 2,
	dxf_osc_order = 3
} dxf_order_scope_t;

/* Trade & Trade ETH -------------------------------------------------------- */

/// Direction
typedef enum {
    dxf_dir_undefined = 0,
    dxf_dir_down = 1,
    dxf_dir_zero_down = 2,
    dxf_dir_zero = 3,
    dxf_dir_zero_up = 4,
    dxf_dir_up = 5
} dxf_direction_t;

/// Trade
typedef struct {
    dxf_long_t time;
    dxf_int_t sequence;
    dxf_int_t time_nanos;
    dxf_char_t exchange_code;
    dxf_double_t price;
    dxf_int_t size;
    /* This field is absent in TradeETH */
    dxf_int_t tick;
    /* This field is absent in TradeETH */
    dxf_double_t change;
    dxf_int_t raw_flags;
    dxf_double_t day_volume;
    dxf_double_t day_turnover;
    dxf_direction_t direction;
    dxf_bool_t is_eth;
	dxf_order_scope_t scope;
} dxf_trade_t;

/* Quote -------------------------------------------------------------------- */

/// Quote
typedef struct {
    dxf_long_t time;
    dxf_int_t sequence;
    dxf_int_t time_nanos;
    dxf_long_t bid_time;
    dxf_char_t bid_exchange_code;
    dxf_double_t bid_price;
    dxf_int_t bid_size;
    dxf_long_t ask_time;
    dxf_char_t ask_exchange_code;
    dxf_double_t ask_price;
    dxf_int_t ask_size;
    dxf_order_scope_t scope;
} dxf_quote_t;

/* Summary ------------------------------------------------------------------ */

/// Price type
typedef enum {
    dxf_pt_regular = 0,
    dxf_pt_indicative = 1,
    dxf_pt_preliminary = 2,
    dxf_pt_final = 3
} dxf_price_type_t;

/// Summary
typedef struct {
    dxf_dayid_t day_id;
    dxf_double_t day_open_price;
    dxf_double_t day_high_price;
    dxf_double_t day_low_price;
    dxf_double_t day_close_price;
    dxf_dayid_t prev_day_id;
    dxf_double_t prev_day_close_price;
    dxf_double_t prev_day_volume;
    dxf_int_t open_interest;
    dxf_int_t raw_flags;
    dxf_char_t exchange_code;
    dxf_price_type_t day_close_price_type;
    dxf_price_type_t prev_day_close_price_type;
    dxf_order_scope_t scope;
} dxf_summary_t;

/* Profile ------------------------------------------------------------------ */

/// Trading status
typedef enum {
  dxf_ts_undefined = 0,
  dxf_ts_halted = 1,
  dxf_ts_active = 2
} dxf_trading_status_t;

/// Short sale restriction
typedef enum {
  dxf_ssr_undefined = 0,
  dxf_ssr_active = 1,
  dxf_ssr_inactive = 2
} dxf_short_sale_restriction_t;

/// Profile
typedef struct {
    dxf_double_t beta;
    dxf_double_t eps;
    dxf_int_t div_freq;
    dxf_double_t exd_div_amount;
    dxf_dayid_t exd_div_date;
    dxf_double_t _52_high_price;
    dxf_double_t _52_low_price;
    dxf_double_t shares;
    dxf_double_t free_float;
    dxf_double_t high_limit_price;
    dxf_double_t low_limit_price;
    dxf_long_t halt_start_time;
    dxf_long_t halt_end_time;
    dxf_int_t raw_flags;
    dxf_const_string_t description;
    dxf_const_string_t status_reason;
    dxf_trading_status_t trading_status;
    dxf_short_sale_restriction_t ssr;
} dxf_profile_t;

/* Order & Spread Order ----------------------------------------------------- */

/// Order side
typedef enum {
    dxf_osd_undefined = 0,
    dxf_osd_buy = 1,
    dxf_osd_sell = 2
} dxf_order_side_t;

/// Order
typedef struct {
    dxf_event_flags_t event_flags;
    dxf_long_t index;
    dxf_long_t time;
    dxf_int_t time_nanos;
    dxf_int_t sequence;
    dxf_double_t price;
    dxf_int_t size;
    dxf_int_t count;
    dxf_order_scope_t scope;
    dxf_order_side_t side;
    dxf_char_t exchange_code;
    dxf_char_t source[DXF_RECORD_SUFFIX_SIZE];
    union {
        dxf_const_string_t market_maker;
        dxf_const_string_t spread_symbol;
    };
} dxf_order_t;

/* Time And Sale ------------------------------------------------------------ */

/// Time & sale type
typedef enum {
    dxf_tnst_new = 0,
    dxf_tnst_correction = 1,
    dxf_tnst_cancel = 2
} dxf_tns_type_t;

/// Time & sale
typedef struct {
    dxf_event_flags_t event_flags;
    dxf_long_t index;
    dxf_long_t time;
    dxf_char_t exchange_code;
    dxf_double_t price;
    dxf_int_t size;
    dxf_double_t bid_price;
    dxf_double_t ask_price;
    dxf_const_string_t exchange_sale_conditions;
    dxf_int_t raw_flags;
    dxf_const_string_t buyer;
    dxf_const_string_t seller;
    dxf_order_side_t side;
    dxf_tns_type_t type;
    dxf_bool_t is_valid_tick;
    dxf_bool_t is_eth_trade;
    dxf_char_t trade_through_exempt;
    dxf_bool_t is_spread_leg;
    dxf_order_scope_t scope;
} dxf_time_and_sale_t;

/* Candle ------------------------------------------------------------------- */
/// Candle
typedef struct {
    dxf_event_flags_t event_flags;
    dxf_long_t index;
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
    dxf_int_t open_interest;
    dxf_double_t imp_volatility;
} dxf_candle_t;

/* Greeks ------------------------------------------------------------------- */
/// Greeks
typedef struct {
    dxf_event_flags_t event_flags;
    dxf_long_t index;
    dxf_long_t time;
    dxf_double_t price;
    dxf_double_t volatility;
    dxf_double_t delta;
    dxf_double_t gamma;
    dxf_double_t theta;
    dxf_double_t rho;
    dxf_double_t vega;
} dxf_greeks_t;

/* TheoPrice ---------------------------------------------------------------- */
/* Event and record are the same */
/// Theo price
typedef dx_theo_price_t dxf_theo_price_t;

/* Underlying --------------------------------------------------------------- */
/* Event and record are the same */
/// Underlying
typedef dx_underlying_t dxf_underlying_t;

/* Series ------------------------------------------------------------------- */
/// Series
typedef struct {
    dxf_event_flags_t event_flags;
    dxf_long_t index;
    dxf_long_t time;
    dxf_int_t sequence;
    dxf_dayid_t expiration;
    dxf_double_t volatility;
    dxf_double_t put_call_ratio;
    dxf_double_t forward_price;
    dxf_double_t dividend;
    dxf_double_t interest;
} dxf_series_t;

/// Configuration
typedef struct {
    dxf_int_t version;
    dxf_string_t object;
} dxf_configuration_t;

/* -------------------------------------------------------------------------- */
/*
 *	Event data constants
 */
/* -------------------------------------------------------------------------- */

static dxf_const_string_t DXF_ORDER_COMPOSITE_BID_STR = L"COMPOSITE_BID";
static dxf_const_string_t DXF_ORDER_COMPOSITE_ASK_STR = L"COMPOSITE_ASK";

/* -------------------------------------------------------------------------- */
/*
 *	Event candle attributes
 */
/* -------------------------------------------------------------------------- */

#define DXF_CANDLE_EXCHANGE_CODE_COMPOSITE_ATTRIBUTE L'\0'
#define DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT DXF_CANDLE_EXCHANGE_CODE_COMPOSITE_ATTRIBUTE
#define DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT 1.0
#define DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT (NAN)

/// Candle price attribute
typedef enum {
    dxf_cpa_last,
    dxf_cpa_bid,
    dxf_cpa_ask,
    dxf_cpa_mark,
    dxf_cpa_settlement,

    dxf_cpa_count,

    dxf_cpa_default = dxf_cpa_last
} dxf_candle_price_attribute_t;

/// Candle session attribute
typedef enum {
    dxf_csa_any,
    dxf_csa_regular,

    dxf_csa_count,

    dxf_csa_default = dxf_csa_any
} dxf_candle_session_attribute_t;

/// Candle type period attribute
typedef enum {
    dxf_ctpa_tick,
    dxf_ctpa_second,
    dxf_ctpa_minute,
    dxf_ctpa_hour,
    dxf_ctpa_day,
    dxf_ctpa_week,
    dxf_ctpa_month,
    dxf_ctpa_optexp,
    dxf_ctpa_year,
    dxf_ctpa_volume,
    dxf_ctpa_price,
    dxf_ctpa_price_momentum,
    dxf_ctpa_price_renko,

    dxf_ctpa_count,

    dxf_ctpa_default = dxf_ctpa_tick
} dxf_candle_type_period_attribute_t;

/// Candle alignment attribute
typedef enum {
    dxf_caa_midnight,
    dxf_caa_session,

    dxf_caa_count,

    dxf_caa_default = dxf_caa_midnight
} dxf_candle_alignment_attribute_t;

/* -------------------------------------------------------------------------- */
/*
 *	Events flag constants
 */
/* -------------------------------------------------------------------------- */

/// Event flag
typedef enum {
    dxf_ef_tx_pending = 0x01,
    dxf_ef_remove_event = 0x02,
    dxf_ef_snapshot_begin = 0x04,
    dxf_ef_snapshot_end = 0x08,
    dxf_ef_snapshot_snip = 0x10,
    dxf_ef_remove_symbol = 0x20
} dxf_event_flag;

/* -------------------------------------------------------------------------- */
/*
*   Additional event params struct
*/
/* -------------------------------------------------------------------------- */

typedef dxf_ulong_t dxf_time_int_field_t;

/// Event params
typedef struct {
    dxf_event_flags_t flags;
    dxf_time_int_field_t time_int_field;
    dxf_ulong_t snapshot_key;
} dxf_event_params_t;

/* -------------------------------------------------------------------------- */
/*
 *	Event listener prototype
 
 *  event type here is a one-bit mask, not an integer
 *  from dx_eid_begin to dx_eid_count
 */
/* -------------------------------------------------------------------------- */


/// Event listener prototype
typedef void (*dxf_event_listener_t) (int event_type, dxf_const_string_t symbol_name,
                                      const dxf_event_data_t* data, int data_count,
                                      void* user_data);

/// Event listener prototype v2
typedef void (*dxf_event_listener_v2_t) (int event_type, dxf_const_string_t symbol_name,
                                      const dxf_event_data_t* data, int data_count, 
                                      const dxf_event_params_t* event_params, void* user_data);

/* -------------------------------------------------------------------------- */
/*
 *	Various event functions
 */
/* -------------------------------------------------------------------------- */

/**
 * @ingroup c-api-event-listener-functions
 *
 * @brief Converts event type code to its string representation
 *
 * @param event_type Event type code
 *
 * @return String representation of event type
 */
dxf_const_string_t dx_event_type_to_string (int event_type);

/**
 * @ingroup c-api-event-listener-functions
 *
 * @brief Get event data structure size for given event id
 *
 * @param event_id Event id
 *
 * @return Data structure size
 */
int dx_get_event_data_struct_size (int event_id);

/**
 * @ingroup c-api-event-listener-functions
 *
 * @brief Get event id by event bitmask
 *
 * @param event_bitmask Event bitmask
 *
 * @return Event id
 */
dx_event_id_t dx_get_event_id_by_bitmask (int event_bitmask);

/* -------------------------------------------------------------------------- */
/*
 *	Event subscription stuff
 */
/* -------------------------------------------------------------------------- */

/// Subscription type
typedef enum {
	dx_st_begin = 0,

    dx_st_ticker = dx_st_begin,
    dx_st_stream,
    dx_st_history,

    /* add new subscription types above this line */

    dx_st_count
} dx_subscription_type_t;

/// Event subscription param
typedef struct {
    dx_record_id_t record_id;
    dx_subscription_type_t subscription_type;
} dx_event_subscription_param_t;

/// Event subscription param list
typedef struct {
    dx_event_subscription_param_t* elements;
    size_t size;
    size_t capacity;
} dx_event_subscription_param_list_t;

/**
 * @ingroup c-api-basic-subscription-functions
 *
 * @brief Returns the list of subscription params. Fills records list according to event_id.
 *
 * @param[in]  connection    Connection handle
 * @param[in]  order_source  Order source
 * @param[in]  event_id      Event id
 * @param[in]  subscr_flags  Subscription flags
 * @param[out] params        Subscription params
 *
 * @warning You need to call dx_free(params.elements) to free resources.
 */
size_t dx_get_event_subscription_params(dxf_connection_t connection, dx_order_source_array_ptr_t order_source, dx_event_id_t event_id,
										dx_event_subscr_flag subscr_flags, OUT dx_event_subscription_param_list_t* params);

/* -------------------------------------------------------------------------- */
/*
*  Snapshot data structs
*/
/* -------------------------------------------------------------------------- */

/// Snapshot
typedef struct {
    int event_type;
    dxf_string_t symbol;

    size_t records_count;
    const dxf_event_data_t* records;
} dxf_snapshot_data_t, *dxf_snapshot_data_ptr_t;

/* -------------------------------------------------------------------------- */
/**
 * @ingroup c-api-snapshots
 *
 * @brief Snapshot listener prototype
 *
 * @param[in] snapshot_data Pointer to the received snapshot data
 * @param[in] user_data     Pointer to user struct, use NULL by default
 */
/* -------------------------------------------------------------------------- */
typedef void(*dxf_snapshot_listener_t) (const dxf_snapshot_data_ptr_t snapshot_data, void* user_data);

/* -------------------------------------------------------------------------- */
#define DXF_IS_CANDLE_REMOVAL(c) (((c)->event_flags & dxf_ef_remove_event) != 0)
#define DXF_IS_ORDER_REMOVAL(o) ((((o)->event_flags & dxf_ef_remove_event) != 0) || ((o)->size == 0))
#define DXF_IS_SPREAD_ORDER_REMOVAL(o) ((((o)->event_flags & dxf_ef_remove_event) != 0) || ((o)->size == 0))
#define DXF_IS_TIME_AND_SALE_REMOVAL(t) (((t)->event_flags & dxf_ef_remove_event) != 0)
#define DXF_IS_GREEKS_REMOVAL(g) (((g)->event_flags & dxf_ef_remove_event) != 0)
#define DXF_IS_SERIES_REMOVAL(s) (((s)->event_flags & dxf_ef_remove_event) != 0)
/**
 *  @brief Incremental Snapshot listener prototype

 *  @param[in] snapshot_data Pointer to the received snapshot data
 *  @param[in] new_snapshot  Flag, is this call with new snapshot or incremental update.
 *  @param[in] user_data     Pointer to user struct, use NULL by default
 */
typedef void(*dxf_snapshot_inc_listener_t) (const dxf_snapshot_data_ptr_t snapshot_data, int new_snapshot, void* user_data);

/* -------------------------------------------------------------------------- */
/*
*  Price Level data structs
*/
/* -------------------------------------------------------------------------- */
/// Price level element
typedef struct {
    dxf_double_t price;
    dxf_long_t size;
    dxf_long_t time;
} dxf_price_level_element_t;

/// Price level book data
typedef struct {
    dxf_const_string_t symbol;

    size_t bids_count;
    const dxf_price_level_element_t *bids;

    size_t asks_count;
    const dxf_price_level_element_t *asks;
} dxf_price_level_book_data_t, *dxf_price_level_book_data_ptr_t;

/* -------------------------------------------------------------------------- */
/**
 * @ingroup c-api-price-level-book
 *
 * @brief Price Level listener prototype

 *  @param[in] book      Pointer to the received price book data.
 *                       bids and asks are sorted by price,
 *                       best bid (with largest price) and best ask
 *                       (with smallest price) are first elements
 *                       of corresponding arrays.
 *  @param[in] user_data Pointer to user struct, use NULL by default
 */
/* -------------------------------------------------------------------------- */
typedef void(*dxf_price_level_book_listener_t) (const dxf_price_level_book_data_ptr_t book, void* user_data);

/* -------------------------------------------------------------------------- */
/**
 * @ingroup c-api-regional-book
 *
 * @brief Regional quote listener prototype
 *
 * @param[in] quote     Pointer to the received regional quote
 * @param[in] user_data Pointer to user struct, use NULL by default
 */
/* -------------------------------------------------------------------------- */

typedef void(*dxf_regional_quote_listener_t) (dxf_const_string_t symbol, const dxf_quote_t* quotes, int count, void* user_data);

/* -------------------------------------------------------------------------- */
/*
 *	Event data navigation functions
 */
/* -------------------------------------------------------------------------- */

#ifdef __cplusplus
    extern "C"
#endif
/**
 * @ingroup c-api-event-listener-functions
 *
 * @brief  Get event data item from event data
 *
 * @param event_mask Event mask
 * @param data       Event data
 * @param index      Event data item index
 * @return Event data item
 */
const dxf_event_data_t dx_get_event_data_item (int event_mask, const dxf_event_data_t data, size_t index);
 
#endif /* EVENT_DATA_H_INCLUDED */