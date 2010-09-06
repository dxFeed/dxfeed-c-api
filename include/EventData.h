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

#include "PrimitiveTypes.h"
#include "DXTypes.h"

/* -------------------------------------------------------------------------- */
/*
 *	Event type constants
 
 *  add new values here for new event types, use the next power-of-two
 *  value available and modify the exclusion mask
 */
/* -------------------------------------------------------------------------- */

typedef enum {
    dx_eid_trade,
    dx_eid_quote,
    dx_eid_fundamental,
    dx_eid_profile,
    dx_eid_market_maker,
    
    /* add new values above this line, add a new bit mask according to
       the existing template */
    
    dx_eid_count
} dx_event_id_t;

#define DX_ET_TRADE			 (1 << dx_eid_trade)
#define DX_ET_QUOTE			 (1 << dx_eid_quote)
#define DX_ET_FUNDAMENTAL	 (1 << dx_eid_fundamental)
#define DX_ET_PROFILE		 (1 << dx_eid_profile)
#define DX_ET_MARKET_MAKER	 (1 << dx_eid_market_maker)
#define DX_ET_UNUSED		 (~((1 << dx_eid_count) - 1))


/* -------------------------------------------------------------------------- */
/*
 *	Event data structures
 */
/* -------------------------------------------------------------------------- */

typedef void* dx_event_data_t;

typedef struct {
    dx_char_t last_exchange;
    dx_int_t last_time;
    dx_double_t last_price;
    dx_int_t last_size;
    dx_int_t last_tick;
    dx_double_t last_change;	
    dx_double_t	volume;	
} dxf_trade_t;

typedef struct {
    dx_char_t bid_exchange;
    dx_double_t bid_price;
    dx_int_t bid_size;
    dx_char_t ask_exchange;
    dx_double_t ask_price;
    dx_int_t ask_size;	
    dx_int_t bid_time;
    dx_int_t ask_time;   
} dxf_quote_t;

typedef struct {
    dx_double_t	high_price;
    dx_double_t	low_price;
    dx_double_t	open_price;
    dx_double_t	close_price;
    dx_int_t open_interest;
} dxf_fundamental_t;

typedef struct {
	dx_char_t mm_exchange;
	dx_int_t mm_id;
	dx_double_t mmbid_price;
	dx_int_t mmbid_size;
	dx_double_t mmask_price;
	dx_int_t mmask_size;
} dxf_market_maker;

/* -------------------------------------------------------------------------- */
/*
 *	Event listener prototype
 */
/* -------------------------------------------------------------------------- */

typedef void (*dx_event_listener_t) (int event_type, dx_const_string_t symbol_name,
                                     const dx_event_data_t* data, int data_count);

 
#endif /* EVENT_DATA_H_INCLUDED */