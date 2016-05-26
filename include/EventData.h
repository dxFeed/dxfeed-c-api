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
    
    /* add new event id above this line */
    
    dx_eid_count,
    dx_eid_invalid
} dx_event_id_t;

#define DXF_ET_TRADE		 (1 << dx_eid_trade)
#define DXF_ET_QUOTE		 (1 << dx_eid_quote)
#define DXF_ET_SUMMARY	     (1 << dx_eid_summary)
#define DXF_ET_PROFILE		 (1 << dx_eid_profile)
#define DXF_ET_ORDER	     (1 << dx_eid_order)
#define DXF_ET_TIME_AND_SALE (1 << dx_eid_time_and_sale)
#define DXF_ET_CANDLE        (1 << dx_eid_candle)
#define DXF_ET_UNUSED		 (~((1 << dx_eid_count) - 1))

#define DX_EVENT_BIT_MASK(event_id) (1 << event_id)

#define RECORD_SUFFIX_SIZE 5

/* -------------------------------------------------------------------------- */
/*
*	Source suffix array
*/
/* -------------------------------------------------------------------------- */

typedef struct {
    dxf_char_t suffix[RECORD_SUFFIX_SIZE];
} dx_suffix_t;

typedef struct {
    dx_suffix_t *elements;
    int size;
    int capacity;
} dx_order_source_array_t;

typedef dx_order_source_array_t* dx_order_source_array_ptr_t;

/* -------------------------------------------------------------------------- */
/*
 *	Event data structures
 */
/* -------------------------------------------------------------------------- */

typedef void* dxf_event_data_t;

typedef dx_trade_t dxf_trade_t;
typedef dx_quote_t dxf_quote_t;
typedef dx_fundamental_t dxf_summary_t;
typedef dx_profile_t dxf_profile_t;
typedef dx_time_and_sale_t dxf_time_and_sale_t;
typedef dx_candle_t dxf_candle_t;

typedef struct {
    dxf_long_t index;
    dxf_int_t side;
    dxf_int_t level;
    dxf_long_t time;
    dxf_char_t exchange_code;
    dxf_const_string_t market_maker;
    dxf_double_t price;
    dxf_long_t size;
    dxf_char_t source[RECORD_SUFFIX_SIZE];
    dxf_int_t count;
} dxf_order_t;

/* -------------------------------------------------------------------------- */
/*
 *	Event data constants
 */
/* -------------------------------------------------------------------------- */

static const dxf_int_t DXF_ORDER_SIDE_BUY = 0;
static const dxf_int_t DXF_ORDER_SIDE_SELL = 1;

static const dxf_int_t DXF_ORDER_LEVEL_COMPOSITE = 0;
static const dxf_int_t DXF_ORDER_LEVEL_REGIONAL = 1;
static const dxf_int_t DXF_ORDER_LEVEL_AGGREGATE = 2;
static const dxf_int_t DXF_ORDER_LEVEL_ORDER = 3;

static const dxf_int_t DXF_TIME_AND_SALE_TYPE_NEW = 0;
static const dxf_int_t DXF_TIME_AND_SALE_TYPE_CORRECTION = 1;
static const dxf_int_t DXF_TIME_AND_SALE_TYPE_CANCEL = 2;

/* -------------------------------------------------------------------------- */
/*
 *	Event candle attributes
 */
/* -------------------------------------------------------------------------- */

#define DXF_CANDLE_EXCHANGE_CODE_COMPOSITE_ATTRIBUTE '\0';
static const dxf_char_t DXF_CANDLE_EXCHANGE_CODE_DEFAULT_ATTRIBUTE = DXF_CANDLE_EXCHANGE_CODE_COMPOSITE_ATTRIBUTE;

typedef enum {
    dxf_cpa_last,
    dxf_cpa_bid,
    dxf_cpa_ask,
    dxf_cpa_mark,
    dxf_cpa_settlement,

    dxf_cpa_count
} dxf_candle_price_attribute_t;

static const dxf_candle_price_attribute_t DXF_CANDLE_PRICE_DEFAULT_ATTRIBUTE = dxf_cpa_last;

typedef enum {
    dxf_csa_any,
    dxf_csa_regular,

    dxf_csa_count
} dxf_candle_session_attribute_t;

static const dxf_candle_session_attribute_t DXF_CANDLE_SESSION_DEFAULT_ATTRIBUTE = dxf_csa_any;

typedef enum {
    dxf_ct_tick,
    dxf_ct_second,
    dxf_ct_minute,
    dxf_ct_hour,
    dxf_ct_day,
    dxf_ct_week,
    dxf_ct_month,
    dxf_ct_optexp,
    dxf_ct_year,
    dxf_ct_volume,
    dxf_ct_price,
    dxf_ct_price_momentum,
    dxf_ct_price_renko,

    dxf_ct_count
} dxf_candle_type_t;

typedef struct {
    dxf_double_t value;
    dxf_candle_type_t type;
} dxf_candle_period_attribute_t;

static const dxf_candle_period_attribute_t DXF_CANDLE_PERIOD_DEFAULT_ATTRIBUTE = { 1, dxf_ct_tick };

typedef enum {
    dxf_caa_midnight,
    dxf_caa_session,

    dxf_caa_count
} dxf_candle_alignment_attribute_t;

static const dxf_candle_alignment_attribute_t DXF_CANDLE_ALIGNMENT_DEFAULT_ATTRIBUTE = dxf_caa_midnight;

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
 *	Event listener prototype
 
 *  event type here is a one-bit mask, not an integer
 *  from dx_eid_begin to dx_eid_count
 */
/* -------------------------------------------------------------------------- */

typedef void (*dxf_event_listener_t) (int event_type, dxf_const_string_t symbol_name,
                                      const dxf_event_data_t* data, dxf_event_flags_t flags,
                                      int data_count, void* user_data);
                                     
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
    int size;
    int capacity;
} dx_event_subscription_param_list_t;

/*
 * Returns the list of subscription params. Fills records list according to event_id.
 *
 * You need to call dx_free(params.elements) to free resources.
 */
int dx_get_event_subscription_params(dxf_connection_t connection, dx_order_source_array_ptr_t order_source, dx_event_id_t event_id,
                                      OUT dx_event_subscription_param_list_t* params);

/* -------------------------------------------------------------------------- */
/*
 *	Event data navigation functions
 */
/* -------------------------------------------------------------------------- */

#ifdef __cplusplus
    extern "C"
#endif
const dxf_event_data_t dx_get_event_data_item (int event_mask, const dxf_event_data_t data, int index);
 
#endif /* EVENT_DATA_H_INCLUDED */