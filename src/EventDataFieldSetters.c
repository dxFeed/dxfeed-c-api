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
 *	Implementation of the event data field setters
 */
 
#include "EventDataFieldSetters.h"
#include "EventData.h"

/* -------------------------------------------------------------------------- */
/*
 *	Setter body macro
 */
/* -------------------------------------------------------------------------- */

#define FIELD_SETTER_BODY(struct_name, field_name, field_type) \
    DX_EVENT_DATA_FIELD_SETTER_PROTOTYPE(struct_name, field_name) { \
        ((struct_name*)object)->field_name = *(field_type*)field; \
    }
    
/* -------------------------------------------------------------------------- */
/*
 *	Trade field setters implementation
 */
/* -------------------------------------------------------------------------- */

FIELD_SETTER_BODY(dxf_trade_t, last_exchange, dx_char_t)
FIELD_SETTER_BODY(dxf_trade_t, last_time, dx_int_t)
FIELD_SETTER_BODY(dxf_trade_t, last_price, dx_double_t)
FIELD_SETTER_BODY(dxf_trade_t, last_size, dx_int_t)
FIELD_SETTER_BODY(dxf_trade_t, last_tick, dx_int_t)
FIELD_SETTER_BODY(dxf_trade_t, last_change, dx_double_t)
FIELD_SETTER_BODY(dxf_trade_t, volume, dx_double_t)

/* -------------------------------------------------------------------------- */
/*
 *	Quote field setters implementation
 */
/* -------------------------------------------------------------------------- */

FIELD_SETTER_BODY(dxf_quote_t, bid_exchange, dx_char_t)
FIELD_SETTER_BODY(dxf_quote_t, bid_price, dx_double_t)
FIELD_SETTER_BODY(dxf_quote_t, bid_size, dx_int_t)
FIELD_SETTER_BODY(dxf_quote_t, ask_exchange, dx_char_t)
FIELD_SETTER_BODY(dxf_quote_t, ask_price, dx_double_t)
FIELD_SETTER_BODY(dxf_quote_t, ask_size, dx_int_t)
FIELD_SETTER_BODY(dxf_quote_t, bid_time, dx_int_t)
FIELD_SETTER_BODY(dxf_quote_t, ask_time, dx_int_t)

/* -------------------------------------------------------------------------- */
/*
 *	Fundamental field setters implementation
 */
/* -------------------------------------------------------------------------- */

FIELD_SETTER_BODY(dxf_fundamental_t, high_price, dx_double_t)
FIELD_SETTER_BODY(dxf_fundamental_t, low_price, dx_double_t)
FIELD_SETTER_BODY(dxf_fundamental_t, open_price, dx_double_t)
FIELD_SETTER_BODY(dxf_fundamental_t, close_price, dx_double_t)
FIELD_SETTER_BODY(dxf_fundamental_t, open_interest, dx_int_t)

/* -------------------------------------------------------------------------- */
/*
 *	Market maker field setters implementation
 */
/* -------------------------------------------------------------------------- */

FIELD_SETTER_BODY(dxf_market_maker, mm_exchange, dx_char_t)
FIELD_SETTER_BODY(dxf_market_maker, mm_id, dx_int_t)
FIELD_SETTER_BODY(dxf_market_maker, mmbid_price, dx_double_t)
FIELD_SETTER_BODY(dxf_market_maker, mmbid_size, dx_int_t)
FIELD_SETTER_BODY(dxf_market_maker, mmask_price, dx_double_t)
FIELD_SETTER_BODY(dxf_market_maker, mmask_size, dx_int_t)