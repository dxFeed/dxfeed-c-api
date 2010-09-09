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
 *	Contains the setter prototypes for the fields of event data structures we pass
 *  to the client side
 */
 
#ifndef EVENT_DATA_FIELD_SETTERS_H_INCLUDED
#define EVENT_DATA_FIELD_SETTERS_H_INCLUDED

/* -------------------------------------------------------------------------- */
/*
 *	Generic setter prototype
 */
/* -------------------------------------------------------------------------- */

typedef void (*dx_event_data_field_setter_t)(void* object, const void* field);

/* -------------------------------------------------------------------------- */
/*
 *	Setter macros
 */
/* -------------------------------------------------------------------------- */

#define DX_EVENT_DATA_FIELD_SETTER_NAME(struct_name, field_name) \
    dx_##struct_name##_##field_name##_##setter
    
#define DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(struct_name, field_name) \
    void DX_EVENT_DATA_FIELD_SETTER_NAME(struct_name, field_name) (void* object, const void* field)

/* -------------------------------------------------------------------------- */
/*
 *	Trade field setters
 */
/* -------------------------------------------------------------------------- */

DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_trade_t, last_exchange);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_trade_t, last_time);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_trade_t, last_price);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_trade_t, last_size);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_trade_t, last_tick);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_trade_t, last_change);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_trade_t, volume);

/* -------------------------------------------------------------------------- */
/*
 *	Quote field setters
 */
/* -------------------------------------------------------------------------- */

DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_quote_t, bid_exchange);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_quote_t, bid_price);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_quote_t, bid_size);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_quote_t, ask_exchange);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_quote_t, ask_price);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_quote_t, ask_size);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_quote_t, bid_time);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_quote_t, ask_time);

/* -------------------------------------------------------------------------- */
/*
 *	Fundamental field setters
 */
/* -------------------------------------------------------------------------- */

DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_fundamental_t, high_price);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_fundamental_t, low_price);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_fundamental_t, open_price);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_fundamental_t, close_price);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_fundamental_t, open_interest);

/* -------------------------------------------------------------------------- */
/*
 *	Profile field setters
 */
/* -------------------------------------------------------------------------- */

DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_profile_t, beta);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_profile_t, eps);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_profile_t, div_freq);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_profile_t, exd_div_amount);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_profile_t, exd_div_date);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_profile_t, price_52_high);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_profile_t, price_52_low);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_profile_t, shares);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_profile_t, is_index);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_profile_t, description);

/* -------------------------------------------------------------------------- */
/*
 *	Market maker field setters
 */
/* -------------------------------------------------------------------------- */

DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_market_maker, mm_exchange);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_market_maker, mm_id);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_market_maker, mmbid_price);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_market_maker, mmbid_size);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_market_maker, mmask_price);
DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(dxf_market_maker, mmask_size);



#endif /* EVENT_DATA_FIELD_SETTERS_H_INCLUDED */