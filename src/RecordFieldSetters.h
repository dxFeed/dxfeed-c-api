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
 *	Contains the setter prototypes for the fields of record structures we pass
 *  to the client side
 */
 
#ifndef RECORD_FIELD_SETTERS_H_INCLUDED
#define RECORD_FIELD_SETTERS_H_INCLUDED

/* -------------------------------------------------------------------------- */
/*
 *	Generic setter prototype
 */
/* -------------------------------------------------------------------------- */

typedef void (*dx_record_field_setter_t)(void* object, const void* field);

/* -------------------------------------------------------------------------- */
/*
 *	Setter macros
 */
/* -------------------------------------------------------------------------- */

#define DX_RECORD_FIELD_SETTER_NAME(struct_name, field_name) \
    struct_name##_##field_name##_##setter
    
#define DX_RECORD_FIELD_SETTER_PROTOTYPE(struct_name, field_name) \
    void DX_RECORD_FIELD_SETTER_NAME(struct_name, field_name) (void* object, const void* field)
    
/* -------------------------------------------------------------------------- */
/*
 *	Trade field setters
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_trade_t, time);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_trade_t, exchange_code);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_trade_t, price);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_trade_t, size);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_trade_t, day_volume);

/* -------------------------------------------------------------------------- */
/*
 *	Quote field setters
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_quote_t, bid_time);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_quote_t, bid_exchange_code);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_quote_t, bid_price);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_quote_t, bid_size);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_quote_t, ask_time);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_quote_t, ask_exchange_code);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_quote_t, ask_price);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_quote_t, ask_size);

/* -------------------------------------------------------------------------- */
/*
 *	Fundamental field setters
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_fundamental_t, day_high_price);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_fundamental_t, day_low_price);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_fundamental_t, day_open_price);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_fundamental_t, prev_day_close_price);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_fundamental_t, open_interest);

/* -------------------------------------------------------------------------- */
/*
 *	Profile field setters
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_profile_t, description);

/* -------------------------------------------------------------------------- */
/*
 *	Market maker field setters
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_market_maker_t, mm_exchange);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_market_maker_t, mm_id);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_market_maker_t, mmbid_time);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_market_maker_t, mmbid_price);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_market_maker_t, mmbid_size);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_market_maker_t, mmask_time);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_market_maker_t, mmask_price);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_market_maker_t, mmask_size);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_market_maker_t, mmbid_count);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_market_maker_t, mmask_count);

/* -------------------------------------------------------------------------- */
/*
 *	Order field setters
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_order_t, index);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_order_t, time);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_order_t, sequence);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_order_t, price);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_order_t, size);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_order_t, flags);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_order_t, mmid);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_order_t, count);

/* -------------------------------------------------------------------------- */
/*
 *	Time and Sale field setters
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_time_and_sale_t, time);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_time_and_sale_t, sequence);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_time_and_sale_t, exchange_code);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_time_and_sale_t, price);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_time_and_sale_t, size);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_time_and_sale_t, bid_price);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_time_and_sale_t, ask_price);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_time_and_sale_t, exch_sale_conds);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_time_and_sale_t, type);

/* -------------------------------------------------------------------------- */
/*
*	Candle field setters
*/
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_candle_t, time);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_candle_t, sequence);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_candle_t, count);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_candle_t, open);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_candle_t, high);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_candle_t, low);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_candle_t, close);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_candle_t, volume);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_candle_t, vwap);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_candle_t, bid_volume);
DX_RECORD_FIELD_SETTER_PROTOTYPE(dx_candle_t, ask_volume);

/* -------------------------------------------------------------------------- */
/*
 *	Default field value functions
 */
/* -------------------------------------------------------------------------- */
/*
 *	Generic default value getter prototype
 */
/* -------------------------------------------------------------------------- */

typedef const void* (*dx_record_field_def_val_getter_t) (void);

/* -------------------------------------------------------------------------- */
/*
 *	Default value function macros
 */
/* -------------------------------------------------------------------------- */

#define DX_RECORD_FIELD_DEF_VAL_NAME(struct_name, field_name) \
    struct_name##_##field_name##_##default_value

#define DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(struct_name, field_name) \
    const void* DX_RECORD_FIELD_DEF_VAL_NAME(struct_name, field_name) (void)
    
/* -------------------------------------------------------------------------- */
/*
 *	Trade field default value getters
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_trade_t, time);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_trade_t, exchange_code);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_trade_t, price);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_trade_t, size);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_trade_t, day_volume);

/* -------------------------------------------------------------------------- */
/*
 *	Quote field default value getters
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_quote_t, bid_time);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_quote_t, bid_exchange_code);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_quote_t, bid_price);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_quote_t, bid_size);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_quote_t, ask_time);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_quote_t, ask_exchange_code);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_quote_t, ask_price);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_quote_t, ask_size);

/* -------------------------------------------------------------------------- */
/*
 *	Fundamental field default value getters
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_fundamental_t, day_high_price);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_fundamental_t, day_low_price);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_fundamental_t, day_open_price);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_fundamental_t, prev_day_close_price);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_fundamental_t, open_interest);

/* -------------------------------------------------------------------------- */
/*
 *	Profile field default value getters
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_profile_t, description);

/* -------------------------------------------------------------------------- */
/*
 *	Market maker field default value getters
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_market_maker_t, mm_exchange);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_market_maker_t, mm_id);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_market_maker_t, mmbid_time);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_market_maker_t, mmbid_price);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_market_maker_t, mmbid_size);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_market_maker_t, mmask_time);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_market_maker_t, mmask_price);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_market_maker_t, mmask_size);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_market_maker_t, mmbid_count);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_market_maker_t, mmask_count);

/* -------------------------------------------------------------------------- */
/*
 *	Order field default value getters
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_order_t, index);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_order_t, time);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_order_t, sequence);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_order_t, price);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_order_t, size);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_order_t, flags);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_order_t, mmid);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_order_t, count);

/* -------------------------------------------------------------------------- */
/*
 *	Time and Sale field default value getters
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_time_and_sale_t, time);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_time_and_sale_t, sequence);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_time_and_sale_t, exchange_code);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_time_and_sale_t, price);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_time_and_sale_t, size);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_time_and_sale_t, bid_price);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_time_and_sale_t, ask_price);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_time_and_sale_t, exch_sale_conds);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_time_and_sale_t, type);

/* -------------------------------------------------------------------------- */
/*
*	Candle field default value getters
*/
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_candle_t, time);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_candle_t, sequence);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_candle_t, count);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_candle_t, open);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_candle_t, high);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_candle_t, low);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_candle_t, close);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_candle_t, volume);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_candle_t, vwap);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_candle_t, bid_volume);
DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(dx_candle_t, ask_volume);

/* -------------------------------------------------------------------------- */
/*
*	Generic setter prototype
*/
/* -------------------------------------------------------------------------- */

typedef void(*dx_record_field_getter_t)(void* object, OUT void* field);

/* -------------------------------------------------------------------------- */
/*
*	Setter macros
*/
/* -------------------------------------------------------------------------- */

#define DX_RECORD_FIELD_GETTER_NAME(struct_name, field_name) \
    struct_name##_##field_name##_##getter

#define DX_RECORD_FIELD_GETTER_PROTOTYPE(struct_name, field_name) \
    void DX_RECORD_FIELD_GETTER_NAME(struct_name, field_name) (void* object, OUT void* field)

/* -------------------------------------------------------------------------- */
/*
*	Trade field getters
*/
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_trade_t, time);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_trade_t, exchange_code);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_trade_t, price);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_trade_t, size);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_trade_t, day_volume);

/* -------------------------------------------------------------------------- */
/*
*	Quote field getters
*/
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_quote_t, bid_time);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_quote_t, bid_exchange_code);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_quote_t, bid_price);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_quote_t, bid_size);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_quote_t, ask_time);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_quote_t, ask_exchange_code);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_quote_t, ask_price);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_quote_t, ask_size);

/* -------------------------------------------------------------------------- */
/*
*	Fundamental field getters
*/
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_fundamental_t, day_high_price);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_fundamental_t, day_low_price);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_fundamental_t, day_open_price);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_fundamental_t, prev_day_close_price);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_fundamental_t, open_interest);

/* -------------------------------------------------------------------------- */
/*
*	Profile field getters
*/
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_profile_t, description);

/* -------------------------------------------------------------------------- */
/*
*	Market maker field getters
*/
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_market_maker_t, mm_exchange);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_market_maker_t, mm_id);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_market_maker_t, mmbid_time);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_market_maker_t, mmbid_price);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_market_maker_t, mmbid_size);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_market_maker_t, mmask_time);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_market_maker_t, mmask_price);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_market_maker_t, mmask_size);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_market_maker_t, mmbid_count);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_market_maker_t, mmask_count);

/* -------------------------------------------------------------------------- */
/*
*	Order field getters
*/
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_order_t, index);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_order_t, time);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_order_t, sequence);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_order_t, price);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_order_t, size);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_order_t, flags);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_order_t, mmid);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_order_t, count);

/* -------------------------------------------------------------------------- */
/*
*	Time and Sale field getters
*/
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_time_and_sale_t, time);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_time_and_sale_t, sequence);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_time_and_sale_t, exchange_code);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_time_and_sale_t, price);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_time_and_sale_t, size);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_time_and_sale_t, bid_price);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_time_and_sale_t, ask_price);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_time_and_sale_t, exch_sale_conds);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_time_and_sale_t, type);

/* -------------------------------------------------------------------------- */
/*
*	Candle field getters
*/
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_candle_t, time);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_candle_t, sequence);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_candle_t, count);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_candle_t, open);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_candle_t, high);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_candle_t, low);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_candle_t, close);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_candle_t, volume);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_candle_t, vwap);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_candle_t, bid_volume);
DX_RECORD_FIELD_GETTER_PROTOTYPE(dx_candle_t, ask_volume);

#endif /* RECORD_FIELD_SETTERS_H_INCLUDED */