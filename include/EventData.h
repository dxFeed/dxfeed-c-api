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
#define DXF_ET_UNUSED		 (~((1 << dx_eid_count) - 1))

#define DX_EVENT_BIT_MASK(event_id) (1 << event_id)

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

typedef struct {
    dxf_long_t index;
    dxf_int_t side;
    dxf_int_t level;
    dxf_long_t time;
    dxf_char_t exchange_code;
    dxf_const_string_t market_maker;
    dxf_double_t price;
    dxf_long_t size;
} dxf_order_t;

/* -------------------------------------------------------------------------- */
/*
 *	Event data constants
 */
/* -------------------------------------------------------------------------- */

extern const dxf_int_t DXF_ORDER_SIDE_BUY;
extern const dxf_int_t DXF_ORDER_SIDE_SELL;

extern const dxf_int_t DXF_ORDER_LEVEL_COMPOSITE;
extern const dxf_int_t DXF_ORDER_LEVEL_REGIONAL;
extern const dxf_int_t DXF_ORDER_LEVEL_AGGREGATE;
extern const dxf_int_t DXF_ORDER_LEVEL_ORDER;

extern const dxf_int_t DXF_TIME_AND_SALE_TYPE_NEW;
extern const dxf_int_t DXF_TIME_AND_SALE_TYPE_CORRECTION;
extern const dxf_int_t DXF_TIME_AND_SALE_TYPE_CANCEL;

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

int dx_get_event_subscription_params (dx_event_id_t event_id, OUT const dx_event_subscription_param_t** params);

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