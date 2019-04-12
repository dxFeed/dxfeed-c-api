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
 *	Implementation of the record field setters
 */

#include "DXFeed.h"

#include "RecordFieldSetters.h"
#include "EventData.h"
#include "DataStructures.h"

/* -------------------------------------------------------------------------- */
/*
 *	Setter body macro
 */
/* -------------------------------------------------------------------------- */

#define FIELD_SETTER_BODY(struct_name, field_name, field_type) \
	DX_RECORD_FIELD_SETTER_PROTOTYPE(struct_name, field_name) { \
		((struct_name*)object)->field_name = *(field_type*)field; \
	}

/* -------------------------------------------------------------------------- */
/*
 *	Getter body macro
 */
/* -------------------------------------------------------------------------- */

#define FIELD_GETTER_BODY(struct_name, field_name, field_type) \
	DX_RECORD_FIELD_GETTER_PROTOTYPE(struct_name, field_name) { \
		*(field_type*)field = ((struct_name*)object)->field_name; \
	}

/* -------------------------------------------------------------------------- */
/*
 *	Default value getter functions
 */
/* -------------------------------------------------------------------------- */

#define GENERIC_VALUE_GETTER_NAME(field_type) \
	generic_##field_type##_value_getter

#define GENERIC_VALUE_GETTER_NAME_PROTO(field_type) \
	const void* GENERIC_VALUE_GETTER_NAME(field_type) (void)

#define FIELD_DEF_VAL_BODY(struct_name, field_name, field_type) \
	DX_RECORD_FIELD_DEF_VAL_PROTOTYPE(struct_name, field_name) { \
		/* so far all the default values are generic and are \
		the same for all the fields of the same type */ \
		\
		return GENERIC_VALUE_GETTER_NAME(field_type)(); \
	}

/* -------------------------------------------------------------------------- */
/*
 *	Some less-than-generic value getters macros
 */
/* -------------------------------------------------------------------------- */

#define RECORD_EXCHANGE_CODE_GETTER_NAME(record_id) \
	record_id##_exchange_code_getter

#define RECORD_EXCHANGE_CODE_GETTER_BODY(record_id) \
	const void* RECORD_EXCHANGE_CODE_GETTER_NAME(record_id) (void* dscc) { \
		bool is_initialized = false; \
		static dxf_char_t exchange_code = 0; \
		\
		if (!is_initialized) { \
			exchange_code = dx_get_record_exchange_code(dscc, record_id); \
			is_initialized = true; \
		} \
		\
		return &exchange_code; \
	}

/* -------------------------------------------------------------------------- */
/*
 *	Generic value getters implementation
 */
/* -------------------------------------------------------------------------- */

GENERIC_VALUE_GETTER_NAME_PROTO(dxf_char_t) {
	static dxf_char_t c = 0;

	return &c;
}

/* -------------------------------------------------------------------------- */

GENERIC_VALUE_GETTER_NAME_PROTO(dxf_int_t) {
	static dxf_int_t i = 0;

	return &i;
}

/* -------------------------------------------------------------------------- */

GENERIC_VALUE_GETTER_NAME_PROTO(dxf_long_t) {
	static dxf_long_t l = 0;

	return &l;
}

/* -------------------------------------------------------------------------- */

GENERIC_VALUE_GETTER_NAME_PROTO(dxf_double_t) {
	static dxf_double_t d = 0;

	return &d;
}

/* -------------------------------------------------------------------------- */

GENERIC_VALUE_GETTER_NAME_PROTO(dxf_dayid_t) {
	static dxf_dayid_t d_id = 0;

	return &d_id;
}

/* -------------------------------------------------------------------------- */

GENERIC_VALUE_GETTER_NAME_PROTO(dxf_const_string_t) {
	static dxf_const_string_t s = L"<Default>";

	return &s;
}

/* -------------------------------------------------------------------------- */

GENERIC_VALUE_GETTER_NAME_PROTO(dxf_byte_array_t) {
	static dxf_byte_array_t cba = { NULL, 0, 0 };

	return &cba;
}

/* -------------------------------------------------------------------------- */
/*
 *	Standard operations macros
 */
/* -------------------------------------------------------------------------- */
#define FIELD_STDOPS_BODIES(struct_name, field_name, field_type) \
    FIELD_SETTER_BODY(struct_name, field_name, field_type)       \
    FIELD_DEF_VAL_BODY(struct_name, field_name, field_type)      \
    FIELD_GETTER_BODY(struct_name, field_name, field_type)


/* -------------------------------------------------------------------------- */
/*
 *	Trade field setters/getters implementation
 */
/* -------------------------------------------------------------------------- */

FIELD_STDOPS_BODIES(dx_trade_t, time, dxf_int_t)
FIELD_STDOPS_BODIES(dx_trade_t, sequence, dxf_int_t)
FIELD_STDOPS_BODIES(dx_trade_t, time_nanos, dxf_int_t)
FIELD_STDOPS_BODIES(dx_trade_t, exchange_code, dxf_char_t)
FIELD_STDOPS_BODIES(dx_trade_t, price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_trade_t, size, dxf_int_t)
FIELD_STDOPS_BODIES(dx_trade_t, tick, dxf_int_t)
FIELD_STDOPS_BODIES(dx_trade_t, change, dxf_double_t)
FIELD_STDOPS_BODIES(dx_trade_t, flags, dxf_int_t)
FIELD_STDOPS_BODIES(dx_trade_t, day_volume, dxf_double_t)
FIELD_STDOPS_BODIES(dx_trade_t, day_turnover, dxf_double_t)

/* -------------------------------------------------------------------------- */
/*
 *	Quote field setters/getters implementation
 */
/* -------------------------------------------------------------------------- */

FIELD_STDOPS_BODIES(dx_quote_t, sequence, dxf_int_t)
FIELD_STDOPS_BODIES(dx_quote_t, time_nanos, dxf_int_t)
FIELD_STDOPS_BODIES(dx_quote_t, bid_time, dxf_int_t)
FIELD_STDOPS_BODIES(dx_quote_t, bid_exchange_code, dxf_char_t)
FIELD_STDOPS_BODIES(dx_quote_t, bid_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_quote_t, bid_size, dxf_int_t)
FIELD_STDOPS_BODIES(dx_quote_t, ask_time, dxf_int_t)
FIELD_STDOPS_BODIES(dx_quote_t, ask_exchange_code, dxf_char_t)
FIELD_STDOPS_BODIES(dx_quote_t, ask_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_quote_t, ask_size, dxf_int_t)

/* -------------------------------------------------------------------------- */
/*
 *	Summary field setters/getters implementation
 */
/* -------------------------------------------------------------------------- */

FIELD_STDOPS_BODIES(dx_summary_t, day_id, dxf_dayid_t)
FIELD_STDOPS_BODIES(dx_summary_t, day_open_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_summary_t, day_high_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_summary_t, day_low_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_summary_t, day_close_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_summary_t, prev_day_id, dxf_dayid_t)
FIELD_STDOPS_BODIES(dx_summary_t, prev_day_close_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_summary_t, prev_day_volume, dxf_double_t)
FIELD_STDOPS_BODIES(dx_summary_t, open_interest, dxf_int_t)
FIELD_STDOPS_BODIES(dx_summary_t, flags, dxf_int_t)

/* -------------------------------------------------------------------------- */
/*
 *	Profile field setter implementation
 */
/* -------------------------------------------------------------------------- */

FIELD_STDOPS_BODIES(dx_profile_t, beta, dxf_double_t)
FIELD_STDOPS_BODIES(dx_profile_t, eps, dxf_double_t)
FIELD_STDOPS_BODIES(dx_profile_t, div_freq, dxf_int_t)
FIELD_STDOPS_BODIES(dx_profile_t, exd_div_amount, dxf_double_t)
FIELD_STDOPS_BODIES(dx_profile_t, exd_div_date, dxf_dayid_t)
FIELD_STDOPS_BODIES(dx_profile_t, _52_high_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_profile_t, _52_low_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_profile_t, shares, dxf_double_t)
FIELD_STDOPS_BODIES(dx_profile_t, free_float, dxf_double_t)
FIELD_STDOPS_BODIES(dx_profile_t, high_limit_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_profile_t, low_limit_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_profile_t, halt_start_time, dxf_int_t)
FIELD_STDOPS_BODIES(dx_profile_t, halt_end_time, dxf_int_t)
FIELD_STDOPS_BODIES(dx_profile_t, flags, dxf_int_t)
FIELD_STDOPS_BODIES(dx_profile_t, description, dxf_const_string_t)
FIELD_STDOPS_BODIES(dx_profile_t, status_reason, dxf_const_string_t)

/* -------------------------------------------------------------------------- */
/*
 *	Market maker field setters/getters implementation
 */
 /* -------------------------------------------------------------------------- */

FIELD_STDOPS_BODIES(dx_market_maker_t, mm_exchange, dxf_char_t)
FIELD_STDOPS_BODIES(dx_market_maker_t, mm_id, dxf_int_t)
FIELD_STDOPS_BODIES(dx_market_maker_t, mmbid_time, dxf_int_t)
FIELD_STDOPS_BODIES(dx_market_maker_t, mmbid_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_market_maker_t, mmbid_size, dxf_int_t)
FIELD_STDOPS_BODIES(dx_market_maker_t, mmbid_count, dxf_int_t)
FIELD_STDOPS_BODIES(dx_market_maker_t, mmask_time, dxf_int_t)
FIELD_STDOPS_BODIES(dx_market_maker_t, mmask_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_market_maker_t, mmask_size, dxf_int_t)
FIELD_STDOPS_BODIES(dx_market_maker_t, mmask_count, dxf_int_t)

/* -------------------------------------------------------------------------- */
/*
 *	Order field setters/getters implementation
 */
 /* -------------------------------------------------------------------------- */

FIELD_STDOPS_BODIES(dx_order_t, index, dxf_int_t)
FIELD_STDOPS_BODIES(dx_order_t, time, dxf_int_t)
FIELD_STDOPS_BODIES(dx_order_t, time_nanos, dxf_int_t)
FIELD_STDOPS_BODIES(dx_order_t, sequence, dxf_int_t)
FIELD_STDOPS_BODIES(dx_order_t, price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_order_t, size, dxf_int_t)
FIELD_STDOPS_BODIES(dx_order_t, flags, dxf_int_t)
FIELD_STDOPS_BODIES(dx_order_t, mmid, dxf_int_t)
FIELD_STDOPS_BODIES(dx_order_t, count, dxf_int_t)

/* -------------------------------------------------------------------------- */
/*
 *	Time and Sale field setters/getters implementation
 */
 /* -------------------------------------------------------------------------- */

FIELD_STDOPS_BODIES(dx_time_and_sale_t, time, dxf_int_t)
FIELD_STDOPS_BODIES(dx_time_and_sale_t, sequence, dxf_int_t)
FIELD_STDOPS_BODIES(dx_time_and_sale_t, exchange_code, dxf_char_t)
FIELD_STDOPS_BODIES(dx_time_and_sale_t, price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_time_and_sale_t, size, dxf_int_t)
FIELD_STDOPS_BODIES(dx_time_and_sale_t, bid_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_time_and_sale_t, ask_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_time_and_sale_t, exchange_sale_conditions, dxf_int_t)
FIELD_STDOPS_BODIES(dx_time_and_sale_t, flags, dxf_int_t)
FIELD_STDOPS_BODIES(dx_time_and_sale_t, buyer, dxf_const_string_t)
FIELD_STDOPS_BODIES(dx_time_and_sale_t, seller, dxf_const_string_t)

/* -------------------------------------------------------------------------- */
/*
 *	Candle field setters/getters implementation
 */
/* -------------------------------------------------------------------------- */

FIELD_STDOPS_BODIES(dx_candle_t, time, dxf_int_t)
FIELD_STDOPS_BODIES(dx_candle_t, sequence, dxf_int_t)
FIELD_STDOPS_BODIES(dx_candle_t, count, dxf_double_t)
FIELD_STDOPS_BODIES(dx_candle_t, open, dxf_double_t)
FIELD_STDOPS_BODIES(dx_candle_t, high, dxf_double_t)
FIELD_STDOPS_BODIES(dx_candle_t, low, dxf_double_t)
FIELD_STDOPS_BODIES(dx_candle_t, close, dxf_double_t)
FIELD_STDOPS_BODIES(dx_candle_t, volume, dxf_double_t)
FIELD_STDOPS_BODIES(dx_candle_t, vwap, dxf_double_t)
FIELD_STDOPS_BODIES(dx_candle_t, bid_volume, dxf_double_t)
FIELD_STDOPS_BODIES(dx_candle_t, ask_volume, dxf_double_t)
FIELD_STDOPS_BODIES(dx_candle_t, open_interest, dxf_int_t)
FIELD_STDOPS_BODIES(dx_candle_t, imp_volatility, dxf_double_t)

/* -------------------------------------------------------------------------- */
/*
 *	SpreadOrder field setters/getters implementation
 */
/* -------------------------------------------------------------------------- */

FIELD_STDOPS_BODIES(dx_spread_order_t, index, dxf_int_t)
FIELD_STDOPS_BODIES(dx_spread_order_t, time, dxf_int_t)
FIELD_STDOPS_BODIES(dx_spread_order_t, sequence, dxf_int_t)
FIELD_STDOPS_BODIES(dx_spread_order_t, price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_spread_order_t, size, dxf_int_t)
FIELD_STDOPS_BODIES(dx_spread_order_t, count, dxf_int_t)
FIELD_STDOPS_BODIES(dx_spread_order_t, flags, dxf_int_t)
FIELD_STDOPS_BODIES(dx_spread_order_t, spread_symbol, dxf_const_string_t)

/* -------------------------------------------------------------------------- */
/*
 *	Greeks field setters/getters implementation
 */
/* -------------------------------------------------------------------------- */

FIELD_STDOPS_BODIES(dx_greeks_t, time, dxf_int_t)
FIELD_STDOPS_BODIES(dx_greeks_t, sequence, dxf_int_t)
FIELD_STDOPS_BODIES(dx_greeks_t, price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_greeks_t, volatility, dxf_double_t)
FIELD_STDOPS_BODIES(dx_greeks_t, delta, dxf_double_t)
FIELD_STDOPS_BODIES(dx_greeks_t, gamma, dxf_double_t)
FIELD_STDOPS_BODIES(dx_greeks_t, theta, dxf_double_t)
FIELD_STDOPS_BODIES(dx_greeks_t, rho, dxf_double_t)
FIELD_STDOPS_BODIES(dx_greeks_t, vega, dxf_double_t)

/* -------------------------------------------------------------------------- */
/*
 *	TheoPrice field setters/getters implementation
 */
/* -------------------------------------------------------------------------- */

FIELD_SETTER_BODY  (dx_theo_price_t, time, dxf_int_t)
FIELD_DEF_VAL_BODY (dx_theo_price_t, time, dxf_int_t)
FIELD_GETTER_BODY  (dx_theo_price_t, time, dxf_long_t)
FIELD_STDOPS_BODIES(dx_theo_price_t, price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_theo_price_t, underlying_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_theo_price_t, delta, dxf_double_t)
FIELD_STDOPS_BODIES(dx_theo_price_t, gamma, dxf_double_t)
FIELD_STDOPS_BODIES(dx_theo_price_t, dividend, dxf_double_t)
FIELD_STDOPS_BODIES(dx_theo_price_t, interest, dxf_double_t)

/* -------------------------------------------------------------------------- */
/*
 *	Underlying field setters/getters implementation
 */
/* -------------------------------------------------------------------------- */

FIELD_STDOPS_BODIES(dx_underlying_t, volatility, dxf_double_t)
FIELD_STDOPS_BODIES(dx_underlying_t, front_volatility, dxf_double_t)
FIELD_STDOPS_BODIES(dx_underlying_t, back_volatility, dxf_double_t)
FIELD_STDOPS_BODIES(dx_underlying_t, put_call_ratio, dxf_double_t)

/* -------------------------------------------------------------------------- */
/*
 *	Series field setters/getters implementation
 */
/* -------------------------------------------------------------------------- */

FIELD_STDOPS_BODIES(dx_series_t, index, dxf_int_t)
FIELD_STDOPS_BODIES(dx_series_t, time, dxf_int_t)
FIELD_STDOPS_BODIES(dx_series_t, sequence, dxf_int_t)
FIELD_STDOPS_BODIES(dx_series_t, expiration, dxf_dayid_t)
FIELD_STDOPS_BODIES(dx_series_t, volatility, dxf_double_t)
FIELD_STDOPS_BODIES(dx_series_t, put_call_ratio, dxf_double_t)
FIELD_STDOPS_BODIES(dx_series_t, forward_price, dxf_double_t)
FIELD_STDOPS_BODIES(dx_series_t, dividend, dxf_double_t)
FIELD_STDOPS_BODIES(dx_series_t, interest, dxf_double_t)

/* -------------------------------------------------------------------------- */
/*
 *	Configuration field setters/getters implementation
 */
/* -------------------------------------------------------------------------- */

FIELD_STDOPS_BODIES(dx_configuration_t, version, dxf_int_t)
FIELD_STDOPS_BODIES(dx_configuration_t, object, dxf_byte_array_t)

/* -------------------------------------------------------------------------- */

RECORD_EXCHANGE_CODE_GETTER_BODY(dx_rid_trade)
RECORD_EXCHANGE_CODE_GETTER_BODY(dx_rid_quote)
RECORD_EXCHANGE_CODE_GETTER_BODY(dx_rid_summary)
RECORD_EXCHANGE_CODE_GETTER_BODY(dx_rid_profile)
RECORD_EXCHANGE_CODE_GETTER_BODY(dx_rid_market_maker)
RECORD_EXCHANGE_CODE_GETTER_BODY(dx_rid_trade_eth)
