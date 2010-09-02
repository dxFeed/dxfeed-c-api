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

#define DX_ET_QUOTE			 (1 << 0)
#define DX_ET_TRADE			 (1 << 1)
#define DX_ET_FUNDAMENTAL	 (1 << 2)
#define DX_ET_PROFILE		 (1 << 3)
#define DX_ET_MARKET_MAKER	 (1 << 4)
#define DX_ET_LAST			 (1 << 5)
#define DX_ET_UNUSED		 (~ ((1 << 6) - 1 ))


typedef void* dxf_subscription_t;
/* -------------------------------------------------------------------------- */
/*
 *	Event data structures
 */
/* -------------------------------------------------------------------------- */

typedef void* dx_event_data_t;

struct dxf_quote_t {
  	dx_char_t			bid_exchange;
	dx_double_t			bid_price;
	dx_int_t			bid_size;
	dx_char_t			ask_exchange;
	dx_double_t			ask_price;
	dx_int_t			ask_size;	
	dx_int_t			bid_time;	
	dx_int_t			ask_time;   
};
struct dxf_market_maker{
	dx_char_t			mm_exchange; 
	dx_int_t			mm_id;
	dx_double_t			mmbid_price;
	dx_int_t 			mmbid_size;
	dx_double_t			mmask_price; 
	dx_int_t			mmask_size;
};

/* -------------------------------------------------------------------------- */
/*
 *	Event listener prototype
 */
/* -------------------------------------------------------------------------- */

typedef void (*dx_event_listener_t)(int event_type, dx_const_string_t symbol_name,
                                    const dx_event_data_t* data, int data_count);

 
#endif /* EVENT_DATA_H_INCLUDED */