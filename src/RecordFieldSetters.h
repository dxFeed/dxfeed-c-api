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
 *	Generic default value getter prototype
 */
/* -------------------------------------------------------------------------- */

typedef const void* (*dx_record_field_def_val_getter_t) (void);

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

#define DX_RECORD_FIELD_SETTER_NAME(struct_name, field_name) \
	struct_name##_##field_name##_##setter

#define DX_RECORD_FIELD_SETTER_PROTOTYPE(struct_name, field_name) \
	void DX_RECORD_FIELD_SETTER_NAME(struct_name, field_name) (void* object, const void* field)

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
 *	Setter macros
 */
/* -------------------------------------------------------------------------- */

#define DX_RECORD_FIELD_GETTER_NAME(struct_name, field_name) \
	struct_name##_##field_name##_##getter

#define DX_RECORD_FIELD_GETTER_PROTOTYPE(struct_name, field_name) \
	void DX_RECORD_FIELD_GETTER_NAME(struct_name, field_name) (void* object, OUT void* field)

/* -------------------------------------------------------------------------- */
/*
 *	Standard operations macros
 */
/* -------------------------------------------------------------------------- */
#define DX_RECORD_FIELD_STDOPS_PROTOTYPES(struct_name, field_name) \
    DX_RECORD_FIELD_SETTER_PROTOTYPE(struct_name, field_name);     \
    DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(struct_name, field_name);    \
    DX_RECORD_FIELD_GETTER_PROTOTYPE(struct_name, field_name);

/* -------------------------------------------------------------------------- */
/*
 *	Trade field operations
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_trade_t, time)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_trade_t, sequence)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_trade_t, time_nanos)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_trade_t, exchange_code)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_trade_t, price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_trade_t, size)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_trade_t, tick)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_trade_t, change)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_trade_t, flags)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_trade_t, day_volume)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_trade_t, day_turnover)

/* -------------------------------------------------------------------------- */
/*
 *	Quote field operations
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_quote_t, sequence)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_quote_t, time_nanos)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_quote_t, bid_time)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_quote_t, bid_exchange_code)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_quote_t, bid_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_quote_t, bid_size)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_quote_t, ask_time)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_quote_t, ask_exchange_code)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_quote_t, ask_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_quote_t, ask_size)

/* -------------------------------------------------------------------------- */
/*
 *	Summary field operations
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_summary_t, day_id)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_summary_t, day_open_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_summary_t, day_high_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_summary_t, day_low_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_summary_t, day_close_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_summary_t, prev_day_id)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_summary_t, prev_day_close_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_summary_t, prev_day_volume)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_summary_t, open_interest)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_summary_t, flags)

/* -------------------------------------------------------------------------- */
/*
 *	Profile field operations
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, beta)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, eps)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, div_freq)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, exd_div_amount)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, exd_div_date)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, _52_high_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, _52_low_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, shares)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, free_float)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, high_limit_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, low_limit_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, halt_start_time)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, halt_end_time)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, flags)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, description)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_profile_t, status_reason)

/* -------------------------------------------------------------------------- */
/*
 *	Market maker field operations
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_market_maker_t, mm_exchange)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_market_maker_t, mm_id)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_market_maker_t, mmbid_time)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_market_maker_t, mmbid_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_market_maker_t, mmbid_size)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_market_maker_t, mmbid_count)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_market_maker_t, mmask_time)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_market_maker_t, mmask_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_market_maker_t, mmask_size)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_market_maker_t, mmask_count)

/* -------------------------------------------------------------------------- */
/*
 *	Order field operations
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_order_t, index)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_order_t, time)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_order_t, time_nanos)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_order_t, sequence)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_order_t, price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_order_t, size)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_order_t, flags)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_order_t, mmid)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_order_t, count)

/* -------------------------------------------------------------------------- */
/*
 *	Time and Sale field operations
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_time_and_sale_t, time)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_time_and_sale_t, sequence)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_time_and_sale_t, exchange_code)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_time_and_sale_t, price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_time_and_sale_t, size)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_time_and_sale_t, bid_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_time_and_sale_t, ask_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_time_and_sale_t, exchange_sale_conditions)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_time_and_sale_t, flags)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_time_and_sale_t, buyer)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_time_and_sale_t, seller)

/* -------------------------------------------------------------------------- */
/*
 *	Candle field operations
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_candle_t, time)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_candle_t, sequence)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_candle_t, count)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_candle_t, open)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_candle_t, high)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_candle_t, low)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_candle_t, close)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_candle_t, volume)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_candle_t, vwap)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_candle_t, bid_volume)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_candle_t, ask_volume)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_candle_t, open_interest)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_candle_t, imp_volatility)

/* -------------------------------------------------------------------------- */
/*
 *	SpreadOrder field operations
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_spread_order_t, index)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_spread_order_t, time)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_spread_order_t, sequence)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_spread_order_t, price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_spread_order_t, size)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_spread_order_t, count)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_spread_order_t, flags)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_spread_order_t, spread_symbol)

/* -------------------------------------------------------------------------- */
/*
 *	Greeks field operations
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_greeks_t, time)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_greeks_t, sequence)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_greeks_t, price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_greeks_t, volatility)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_greeks_t, delta)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_greeks_t, gamma)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_greeks_t, theta)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_greeks_t, rho)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_greeks_t, vega)

/* -------------------------------------------------------------------------- */
/*
 *	TheoPrice field operations
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_theo_price_t, time)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_theo_price_t, price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_theo_price_t, underlying_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_theo_price_t, delta)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_theo_price_t, gamma)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_theo_price_t, dividend)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_theo_price_t, interest)

/* -------------------------------------------------------------------------- */
/*
 *	Underlying field operations
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_underlying_t, volatility)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_underlying_t, front_volatility)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_underlying_t, back_volatility)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_underlying_t, put_call_ratio)

/* -------------------------------------------------------------------------- */
/*
 *	Series field operations
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_series_t, index)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_series_t, time)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_series_t, sequence)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_series_t, expiration)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_series_t, volatility)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_series_t, put_call_ratio)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_series_t, forward_price)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_series_t, dividend)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_series_t, interest)

/* -------------------------------------------------------------------------- */
/*
 *	Configuration field operations
 */
/* -------------------------------------------------------------------------- */

DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_configuration_t, version)
DX_RECORD_FIELD_STDOPS_PROTOTYPES(dx_configuration_t, object)

#endif /* RECORD_FIELD_SETTERS_H_INCLUDED */