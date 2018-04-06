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
 
#ifndef EVENT_DATA_H_INCLUDED
#define EVENT_DATA_H_INCLUDED

#include "RecordData.h"
#include "DXTypes.h"

#ifndef OUT
    #define OUT
#endif /* OUT */

/* -------------------------------------------------------------------------- */
/*
 *	Event type constants
 */
/* -------------------------------------------------------------------------- */

typedef enum {
    dx_eid_begin,
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

#define DXF_ET_TRADE         (1 << dx_eid_trade)
#define DXF_ET_QUOTE         (1 << dx_eid_quote)
#define DXF_ET_SUMMARY       (1 << dx_eid_summary)
#define DXF_ET_PROFILE       (1 << dx_eid_profile)
#define DXF_ET_ORDER         (1 << dx_eid_order)
#define DXF_ET_TIME_AND_SALE (1 << dx_eid_time_and_sale)
#define DXF_ET_CANDLE        (1 << dx_eid_candle)
#define DXF_ET_TRADE_ETH     (1 << dx_eid_trade_eth)
#define DXF_ET_SPREAD_ORDER  (1 << dx_eid_spread_order)
#define DXF_ET_GREEKS        (1 << dx_eid_greeks)
#define DXF_ET_THEO_PRICE    (1 << dx_eid_theo_price)
#define DXF_ET_UNDERLYING    (1 << dx_eid_underlying)
#define DXF_ET_SERIES        (1 << dx_eid_series)
#define DXF_ET_CONFIGURATION (1 << dx_eid_configuration)
#define DXF_ET_UNUSED        (~((1 << dx_eid_count) - 1))

#define DX_EVENT_BIT_MASK(event_id) (1 << event_id)

// The length of record suffix including including the terminating null character 
#define DXF_RECORD_SUFFIX_SIZE 5

/* -------------------------------------------------------------------------- */
/*
*	Source suffix array
*/
/* -------------------------------------------------------------------------- */

typedef struct {
    dxf_char_t suffix[DXF_RECORD_SUFFIX_SIZE];
} dx_suffix_t;

typedef struct {
    dx_suffix_t *elements;
    size_t size;
    size_t capacity;
} dx_order_source_array_t;

typedef dx_order_source_array_t* dx_order_source_array_ptr_t;

/* -------------------------------------------------------------------------- */
/*
 *	Event data structures and support
 */
/* -------------------------------------------------------------------------- */

typedef void* dxf_event_data_t;

/* Trade -------------------------------------------------------------------- */

/* Event and record are the same */
typedef dx_trade_t dxf_trade_t;

typedef enum {
    dxf_dir_undefined = 0,
    dxf_dir_down = 1,
    dxf_dir_zero_down = 2,
    dxf_dir_zero = 3,
    dxf_dir_zero_up = 4,
    dxf_dir_up = 5
} dxf_direction_t;

static const dxf_int_t  DXF_TRADE_FLAGS_ETH       = 1;
static const dxf_byte_t DXF_TRADE_FLAGS_DIR_SHIFT = 1;
static const dxf_byte_t DXF_TRADE_FLAGS_DIR_MASK  = 0x7;

#define DXF_TRADE_IS_ETH(trade)   (((trade)->flags & DXF_TRADE_FLAGS_ETH) == DXF_TRADE_FLAGS_ETH)
#define DXF_TRADE_GET_DIR(trade)  ((dxf_direction_t)(((trade)->flags >> DXF_TRADE_FLAGS_DIR_SHIFT) & DXF_TRADE_FLAGS_DIR_MASK))

/* Quote -------------------------------------------------------------------- */

/* Event and record are the same */
typedef dx_quote_t dxf_quote_t;

/* Summary ------------------------------------------------------------------ */

/* Event and record are the same */

typedef dx_summary_t dxf_summary_t;

typedef enum {
    dxf_pt_regular = 0,
    dxf_pt_indicative = 1,
    dxf_pt_preliminary = 2,
    dxf_pt_final = 3
} dxf_price_type_t;

static const dxf_byte_t DXF_SUMMARY_FLAGS_DCPT_SHIFT  = 2;
static const dxf_byte_t DXF_SUMMARY_FLAGS_PDCPT_SHIFT = 0;
static const dxf_byte_t DXF_SUMMARY_FLAGS_CPT_MASK    = 0x3;

#define DX_SUMMARY_GET_DAY_CLOSE_PRICE_TYPE(summary)      ((dxf_price_type_t)(((summary)->flags >> DXF_SUMMARY_FLAGS_DCPT_SHIFT)  & DXF_SUMMARY_FLAGS_CPT_MASK))
#define DX_SUMMARY_GET_PREV_DAY_CLOSE_PRICE_TYPE(summary) ((dxf_price_type_t)(((summary)->flags >> DXF_SUMMARY_FLAGS_PDCPT_SHIFT) & DXF_SUMMARY_FLAGS_CPT_MASK))

/* Profile ------------------------------------------------------------------ */
typedef dx_profile_t dxf_profile_t;
typedef dx_candle_t dxf_candle_t;
typedef dx_trade_eth_t dxf_trade_eth_t;
typedef dx_greeks_t dxf_greeks_t;
typedef dx_theo_price_t dxf_theo_price_t;
typedef dx_underlying_t dxf_underlying_t;
typedef dx_series_t dxf_series_t;

typedef struct {
    dxf_event_flags_t event_flags;
    dxf_long_t time;
    dxf_int_t sequence;
    dxf_char_t exchange_code;
    dxf_double_t price;
    dxf_long_t size;
    dxf_double_t bid_price;
    dxf_double_t ask_price;
    dxf_const_string_t exchange_sale_conditions;
    dxf_int_t flags;
    dxf_const_string_t buyer;
    dxf_const_string_t seller;

    dxf_long_t index;
    dxf_int_t side;
    dxf_bool_t is_cancel;
    dxf_bool_t is_correction;
    dxf_bool_t is_eth_trade;
    dxf_bool_t is_new;
    dxf_bool_t is_spread_leg;
    dxf_bool_t is_valid_tick;
    dxf_int_t type;
} dxf_time_and_sale_t;

typedef struct {
    dxf_int_t count;
    dxf_event_flags_t event_flags;
    dxf_char_t exchange_code;
    dxf_long_t index;
    dxf_int_t level;
    dxf_int_t side;
    dxf_double_t price;
    dxf_int_t scope;
    dxf_int_t sequence;
    dxf_long_t size;
    dxf_char_t source[DXF_RECORD_SUFFIX_SIZE];
    dxf_long_t time;
    dxf_long_t time_sequence;
    dxf_const_string_t market_maker;
} dxf_order_t;

typedef struct {
    dxf_int_t count;
    dxf_event_flags_t event_flags;
    dxf_char_t exchange_code;
    dxf_long_t index;
    dxf_int_t level;
    dxf_int_t side;
    dxf_double_t price;
    dxf_int_t scope;
    dxf_int_t sequence;
    dxf_long_t size;
    dxf_char_t source[DXF_RECORD_SUFFIX_SIZE];
    dxf_long_t time;
    dxf_long_t time_sequence;
    dxf_const_string_t spread_symbol;
} dxf_spread_order_t;

typedef struct {
    dxf_string_t object;
} dxf_configuration_t;

/* -------------------------------------------------------------------------- */
/*
 *	Event data constants
 */
/* -------------------------------------------------------------------------- */

static const dxf_byte_t DXF_SUMMARY_PRICE_TYPE_REGULAR = 0;
static const dxf_byte_t DXF_SUMMARY_PRICE_TYPE_INDICATIVE = 1;
static const dxf_byte_t DXF_SUMMARY_PRICE_TYPE_PRELIMINARY = 2;
static const dxf_byte_t DXF_SUMMARY_PRICE_TYPE_FINAL = 3;

static const dxf_int_t DXF_ORDER_SIDE_BUY = 0;
static const dxf_int_t DXF_ORDER_SIDE_SELL = 1;

static const dxf_int_t DXF_ORDER_LEVEL_COMPOSITE = 0;
static const dxf_int_t DXF_ORDER_LEVEL_REGIONAL = 1;
static const dxf_int_t DXF_ORDER_LEVEL_AGGREGATE = 2;
static const dxf_int_t DXF_ORDER_LEVEL_ORDER = 3;

static const dxf_int_t DXF_TIME_AND_SALE_TYPE_NEW = 0;
static const dxf_int_t DXF_TIME_AND_SALE_TYPE_CORRECTION = 1;
static const dxf_int_t DXF_TIME_AND_SALE_TYPE_CANCEL = 2;

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

typedef enum {
    dxf_cpa_last,
    dxf_cpa_bid,
    dxf_cpa_ask,
    dxf_cpa_mark,
    dxf_cpa_settlement,

    dxf_cpa_count,

    dxf_cpa_default = dxf_cpa_last
} dxf_candle_price_attribute_t;

typedef enum {
    dxf_csa_any,
    dxf_csa_regular,

    dxf_csa_count,

    dxf_csa_default = dxf_csa_any
} dxf_candle_session_attribute_t;

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


typedef void (*dxf_event_listener_t) (int event_type, dxf_const_string_t symbol_name,
                                      const dxf_event_data_t* data, int data_count,
                                      void* user_data);

typedef void (*dxf_event_listener_v2_t) (int event_type, dxf_const_string_t symbol_name,
                                      const dxf_event_data_t* data, int data_count, 
                                      const dxf_event_params_t* event_params, void* user_data);

/* -------------------------------------------------------------------------- */
/*
 *	Various event functions
 */
/* -------------------------------------------------------------------------- */

dxf_const_string_t dx_event_type_to_string (int event_type);
int dx_get_event_data_struct_size (int event_id);
dx_event_id_t dx_get_event_id_by_bitmask (int event_bitmask);

/* -------------------------------------------------------------------------- */
/*
 *	Event subscription stuff
 */
/* -------------------------------------------------------------------------- */

typedef enum {
    dx_st_ticker,
    dx_st_stream,
    dx_st_history,

    /* add new subscription types above this line */

    dx_st_count
} dx_subscription_type_t;

typedef struct {
    dx_record_id_t record_id;
    dx_subscription_type_t subscription_type;
} dx_event_subscription_param_t;

typedef struct {
    dx_event_subscription_param_t* elements;
    size_t size;
    size_t capacity;
} dx_event_subscription_param_list_t;

/*
 * Returns the list of subscription params. Fills records list according to event_id.
 *
 * You need to call dx_free(params.elements) to free resources.
 */
size_t dx_get_event_subscription_params(dxf_connection_t connection, dx_order_source_array_ptr_t order_source, dx_event_id_t event_id,
                                     dxf_uint_t subscr_flags, OUT dx_event_subscription_param_list_t* params);

/* -------------------------------------------------------------------------- */
/*
*  Snapshot data structs
*/
/* -------------------------------------------------------------------------- */

typedef struct {
    int event_type;
    dxf_string_t symbol;

    size_t records_count;
    const dxf_event_data_t* records;
} dxf_snapshot_data_t, *dxf_snapshot_data_ptr_t;

/* -------------------------------------------------------------------------- */
/*
*  Snapshot listener prototype

*  snapshot_data - pointer to the received snapshot data
*  user_data     - pointer to user struct, use NULL by default
*/
/* -------------------------------------------------------------------------- */

typedef void(*dxf_snapshot_listener_t) (const dxf_snapshot_data_ptr_t snapshot_data, void* user_data);

/* -------------------------------------------------------------------------- */
/*
*  Incremental Snapshot listener prototype

*  snapshot_data - pointer to the received snapshot data
*  new_snapshot  - flag, is this call with new snapshot or incremental update.
*  user_data     - pointer to user struct, use NULL by default
*/
/* -------------------------------------------------------------------------- */
#define DXF_IS_CANDLE_REMOVAL(c) (((c)->event_flags & dxf_ef_remove_event) != 0)
#define DXF_IS_ORDER_REMOVAL(o) ((o)->size == 0)
#define DXF_IS_SPREAD_ORDER_REMOVAL(o) ((o)->size == 0)
#define DXF_IS_TIME_AND_SALE_REMOVAL(t) (((t)->event_flags & dxf_ef_remove_event) != 0)
#define DXF_IS_GREEKS_REMOVAL(g) (((g)->event_flags & dxf_ef_remove_event) != 0)
#define DXF_IS_SERIES_REMOVAL(s) (((s)->event_flags & dxf_ef_remove_event) != 0)
typedef void(*dxf_snapshot_inc_listener_t) (const dxf_snapshot_data_ptr_t snapshot_data, int new_snapshot, void* user_data);

/* -------------------------------------------------------------------------- */
/*
*  Price Level data structs
*/
/* -------------------------------------------------------------------------- */
typedef struct {
    dxf_double_t price;
    dxf_long_t size;
    dxf_long_t time;
} dxf_price_level_element_t;

typedef struct {
    dxf_const_string_t symbol;

    size_t bids_count;
    const dxf_price_level_element_t *bids;

    size_t asks_count;
    const dxf_price_level_element_t *asks;
} dxf_price_level_book_data_t, *dxf_price_level_book_data_ptr_t;

/* -------------------------------------------------------------------------- */
/*
*  Price Level listener prototype

*  book          - pointer to the received price book data.
*                  bids and asks are sorted by price,
*                  best bid (with largest price) and best ask
*                  (with smallest price) are first elements
*                  of corresponding arrays.
*  user_data     - pointer to user struct, use NULL by default
*/
/* -------------------------------------------------------------------------- */

typedef void(*dxf_price_level_book_listener_t) (const dxf_price_level_book_data_ptr_t book, void* user_data);

/* -------------------------------------------------------------------------- */
/*
 *	Event data navigation functions
 */
/* -------------------------------------------------------------------------- */

#ifdef __cplusplus
    extern "C"
#endif
const dxf_event_data_t dx_get_event_data_item (int event_mask, const dxf_event_data_t data, size_t index);
 
#endif /* EVENT_DATA_H_INCLUDED */