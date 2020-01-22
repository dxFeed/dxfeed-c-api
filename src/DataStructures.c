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


#include "DXFeed.h"

#include "DataStructures.h"
#include "BufferedIOCommon.h"
#include "DXAlgorithms.h"
#include "ConnectionContextData.h"
#include "DXErrorHandling.h"
#include "Logger.h"
#include "ServerMessageProcessor.h"


#define DX_RECORD_FIELD_STDOPS(r, f) DX_RECORD_FIELD_SETTER_NAME(r, f), DX_RECORD_FIELD_DEF_VAL_NAME(r, f), DX_RECORD_FIELD_GETTER_NAME(r, f)

/* -------------------------------------------------------------------------- */
/*
 *	Trade data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_trade[] = {
	{ dx_fid_compact_int,                       L"Last.Time",         DX_RECORD_FIELD_STDOPS(dx_trade_t, time),          dx_ft_common_field },
	{ dx_fid_compact_int,                       L"Last.Sequence",     DX_RECORD_FIELD_STDOPS(dx_trade_t, sequence),      dx_ft_common_field },
	{ dx_fid_compact_int,                       L"Last.TimeNanoPart", DX_RECORD_FIELD_STDOPS(dx_trade_t, time_nanos),    dx_ft_common_field },
	{ dx_fid_utf_char,                          L"Last.Exchange",     DX_RECORD_FIELD_STDOPS(dx_trade_t, exchange_code), dx_ft_common_field },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Last.Price",        DX_RECORD_FIELD_STDOPS(dx_trade_t, price),         dx_ft_common_field },
	{ dx_fid_compact_int,                       L"Last.Size",         DX_RECORD_FIELD_STDOPS(dx_trade_t, size),          dx_ft_common_field },
	{ dx_fid_compact_int,                       L"Last.Tick",         DX_RECORD_FIELD_STDOPS(dx_trade_t, tick),          dx_ft_common_field },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Last.Change",       DX_RECORD_FIELD_STDOPS(dx_trade_t, change),        dx_ft_common_field },
	{ dx_fid_compact_int,                       L"Last.Flags",        DX_RECORD_FIELD_STDOPS(dx_trade_t, flags),         dx_ft_common_field },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Volume",            DX_RECORD_FIELD_STDOPS(dx_trade_t, day_volume),    dx_ft_common_field },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"DayTurnover",       DX_RECORD_FIELD_STDOPS(dx_trade_t, day_turnover),  dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Quote data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_quote[] = {
	{ dx_fid_compact_int,                       L"Sequence",     DX_RECORD_FIELD_STDOPS(dx_quote_t, sequence),          dx_ft_common_field },
	{ dx_fid_compact_int,                       L"TimeNanoPart", DX_RECORD_FIELD_STDOPS(dx_quote_t, time_nanos),        dx_ft_common_field },
	{ dx_fid_compact_int,                       L"Bid.Time",     DX_RECORD_FIELD_STDOPS(dx_quote_t, bid_time),          dx_ft_common_field },
	{ dx_fid_utf_char,                          L"Bid.Exchange", DX_RECORD_FIELD_STDOPS(dx_quote_t, bid_exchange_code),	dx_ft_common_field },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Bid.Price",    DX_RECORD_FIELD_STDOPS(dx_quote_t, bid_price),         dx_ft_common_field },
	{ dx_fid_compact_int,                       L"Bid.Size",     DX_RECORD_FIELD_STDOPS(dx_quote_t, bid_size),          dx_ft_common_field },
	{ dx_fid_compact_int,                       L"Ask.Time",     DX_RECORD_FIELD_STDOPS(dx_quote_t, ask_time),          dx_ft_common_field },
	{ dx_fid_utf_char,                          L"Ask.Exchange", DX_RECORD_FIELD_STDOPS(dx_quote_t, ask_exchange_code), dx_ft_common_field },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Ask.Price",    DX_RECORD_FIELD_STDOPS(dx_quote_t, ask_price),         dx_ft_common_field },
	{ dx_fid_compact_int,                       L"Ask.Size",     DX_RECORD_FIELD_STDOPS(dx_quote_t, ask_size),          dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Summary data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_summary[] = {
	{ dx_fid_compact_int | dx_fid_flag_date,    L"DayId",              DX_RECORD_FIELD_STDOPS(dx_summary_t, day_id),               dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"DayOpen.Price",      DX_RECORD_FIELD_STDOPS(dx_summary_t, day_open_price),       dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"DayHigh.Price",      DX_RECORD_FIELD_STDOPS(dx_summary_t, day_high_price),       dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"DayLow.Price",       DX_RECORD_FIELD_STDOPS(dx_summary_t, day_low_price),        dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"DayClose.Price",     DX_RECORD_FIELD_STDOPS(dx_summary_t, day_close_price),      dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_date,    L"PrevDayId",          DX_RECORD_FIELD_STDOPS(dx_summary_t, prev_day_id),          dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"PrevDayClose.Price", DX_RECORD_FIELD_STDOPS(dx_summary_t, prev_day_close_price), dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"PrevDayVolume",      DX_RECORD_FIELD_STDOPS(dx_summary_t, prev_day_volume),      dx_ft_common_field },
    { dx_fid_compact_int,                       L"OpenInterest",       DX_RECORD_FIELD_STDOPS(dx_summary_t, open_interest),        dx_ft_common_field },
    { dx_fid_compact_int,                       L"Flags",              DX_RECORD_FIELD_STDOPS(dx_summary_t, flags),                dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Profile data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_profile[] = {
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Beta",            DX_RECORD_FIELD_STDOPS(dx_profile_t, beta),             dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Eps",             DX_RECORD_FIELD_STDOPS(dx_profile_t, eps),              dx_ft_common_field },
    { dx_fid_compact_int,                       L"DivFreq",         DX_RECORD_FIELD_STDOPS(dx_profile_t, div_freq),         dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"ExdDiv.Amount",   DX_RECORD_FIELD_STDOPS(dx_profile_t, exd_div_amount),   dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_date,    L"ExdDiv.Date",     DX_RECORD_FIELD_STDOPS(dx_profile_t, exd_div_date),     dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"52High.Price",    DX_RECORD_FIELD_STDOPS(dx_profile_t, _52_high_price),   dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"52Low.Price",     DX_RECORD_FIELD_STDOPS(dx_profile_t, _52_low_price),    dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Shares",          DX_RECORD_FIELD_STDOPS(dx_profile_t, shares),           dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"FreeFloat",       DX_RECORD_FIELD_STDOPS(dx_profile_t, free_float),       dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"HighLimitPrice",  DX_RECORD_FIELD_STDOPS(dx_profile_t, high_limit_price), dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"LowLimitPrice",   DX_RECORD_FIELD_STDOPS(dx_profile_t, low_limit_price),	dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_time,    L"Halt.StartTime",  DX_RECORD_FIELD_STDOPS(dx_profile_t, halt_start_time),  dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_time,    L"Halt.EndTime",    DX_RECORD_FIELD_STDOPS(dx_profile_t, halt_end_time),    dx_ft_common_field },
    { dx_fid_compact_int,                       L"Flags",           DX_RECORD_FIELD_STDOPS(dx_profile_t, flags),            dx_ft_common_field },
    { dx_fid_utf_char_array,                    L"Description",     DX_RECORD_FIELD_STDOPS(dx_profile_t, description),      dx_ft_common_field },
    { dx_fid_utf_char_array,                    L"StatusReason",    DX_RECORD_FIELD_STDOPS(dx_profile_t, status_reason),    dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Market maker data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_market_maker[] = {
	{ dx_fid_utf_char,                          L"MMExchange",  DX_RECORD_FIELD_STDOPS(dx_market_maker_t, mm_exchange), dx_ft_first_time_int_field  },
    { dx_fid_compact_int,                       L"MMID",        DX_RECORD_FIELD_STDOPS(dx_market_maker_t, mm_id),       dx_ft_second_time_int_field },
    { dx_fid_compact_int,                       L"MMBid.Time",  DX_RECORD_FIELD_STDOPS(dx_market_maker_t, mmbid_time),  dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"MMBid.Price", DX_RECORD_FIELD_STDOPS(dx_market_maker_t, mmbid_price), dx_ft_common_field },
    { dx_fid_compact_int,                       L"MMBid.Size",  DX_RECORD_FIELD_STDOPS(dx_market_maker_t, mmbid_size),  dx_ft_common_field },
    { dx_fid_compact_int,                       L"MMBid.Count", DX_RECORD_FIELD_STDOPS(dx_market_maker_t, mmbid_count), dx_ft_common_field },
    { dx_fid_compact_int,                       L"MMAsk.Time",  DX_RECORD_FIELD_STDOPS(dx_market_maker_t, mmask_time),  dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"MMAsk.Price", DX_RECORD_FIELD_STDOPS(dx_market_maker_t, mmask_price), dx_ft_common_field },
    { dx_fid_compact_int,                       L"MMAsk.Size",  DX_RECORD_FIELD_STDOPS(dx_market_maker_t, mmask_size),  dx_ft_common_field },
    { dx_fid_compact_int,                       L"MMAsk.Count", DX_RECORD_FIELD_STDOPS(dx_market_maker_t, mmask_count), dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Order data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_order[] = {
	{ dx_fid_compact_int,                       L"Index",        DX_RECORD_FIELD_STDOPS(dx_order_t, index),      dx_ft_second_time_int_field },
    { dx_fid_compact_int,                       L"Time",         DX_RECORD_FIELD_STDOPS(dx_order_t, time),       dx_ft_common_field },
	{ dx_fid_compact_int,                       L"TimeNanoPart", DX_RECORD_FIELD_STDOPS(dx_order_t, time_nanos), dx_ft_common_field },
    { dx_fid_compact_int,                       L"Sequence",     DX_RECORD_FIELD_STDOPS(dx_order_t, sequence),   dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Price",        DX_RECORD_FIELD_STDOPS(dx_order_t, price),      dx_ft_common_field },
    { dx_fid_compact_int,                       L"Size",         DX_RECORD_FIELD_STDOPS(dx_order_t, size),       dx_ft_common_field },
    { dx_fid_compact_int,                       L"Flags",        DX_RECORD_FIELD_STDOPS(dx_order_t, flags),      dx_ft_common_field },
    { dx_fid_compact_int,                       L"MMID",         DX_RECORD_FIELD_STDOPS(dx_order_t, mmid),       dx_ft_common_field },
    { dx_fid_compact_int,                       L"Count",        DX_RECORD_FIELD_STDOPS(dx_order_t, count),	     dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Time and Sale data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_time_and_sale[] = {
	{ dx_fid_compact_int | dx_fid_flag_time,     L"Time",                   DX_RECORD_FIELD_STDOPS(dx_time_and_sale_t, time),                     dx_ft_first_time_int_field  },
    { dx_fid_compact_int | dx_fid_flag_sequence, L"Sequence",               DX_RECORD_FIELD_STDOPS(dx_time_and_sale_t, sequence),                 dx_ft_second_time_int_field },
    { dx_fid_utf_char,                           L"Exchange",               DX_RECORD_FIELD_STDOPS(dx_time_and_sale_t, exchange_code),            dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Price",                  DX_RECORD_FIELD_STDOPS(dx_time_and_sale_t, price),                    dx_ft_common_field },
    { dx_fid_compact_int,                        L"Size",                   DX_RECORD_FIELD_STDOPS(dx_time_and_sale_t, size),                     dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Bid.Price",              DX_RECORD_FIELD_STDOPS(dx_time_and_sale_t, bid_price),                dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Ask.Price",              DX_RECORD_FIELD_STDOPS(dx_time_and_sale_t, ask_price),                dx_ft_common_field },
    { dx_fid_compact_int,                        L"ExchangeSaleConditions", DX_RECORD_FIELD_STDOPS(dx_time_and_sale_t, exchange_sale_conditions), dx_ft_common_field },
    { dx_fid_compact_int,                        L"Flags",                  DX_RECORD_FIELD_STDOPS(dx_time_and_sale_t, flags),                    dx_ft_common_field },
    { dx_fid_utf_char_array,                     L"Buyer",                  DX_RECORD_FIELD_STDOPS(dx_time_and_sale_t, buyer),                    dx_ft_common_field },
    { dx_fid_utf_char_array,                     L"Seller",                 DX_RECORD_FIELD_STDOPS(dx_time_and_sale_t, seller),                   dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Candle data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_candle[] = {
	{ dx_fid_compact_int | dx_fid_flag_time,     L"Time",          DX_RECORD_FIELD_STDOPS(dx_candle_t, time),           dx_ft_first_time_int_field  },
    { dx_fid_compact_int | dx_fid_flag_sequence, L"Sequence",      DX_RECORD_FIELD_STDOPS(dx_candle_t, sequence),       dx_ft_second_time_int_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Count",         DX_RECORD_FIELD_STDOPS(dx_candle_t, count),          dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Open",          DX_RECORD_FIELD_STDOPS(dx_candle_t, open),           dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"High",          DX_RECORD_FIELD_STDOPS(dx_candle_t, high),           dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Low",           DX_RECORD_FIELD_STDOPS(dx_candle_t, low),            dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Close",         DX_RECORD_FIELD_STDOPS(dx_candle_t, close),          dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Volume",        DX_RECORD_FIELD_STDOPS(dx_candle_t, volume),         dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"VWAP",          DX_RECORD_FIELD_STDOPS(dx_candle_t, vwap),           dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Bid.Volume",    DX_RECORD_FIELD_STDOPS(dx_candle_t, bid_volume),     dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Ask.Volume",    DX_RECORD_FIELD_STDOPS(dx_candle_t, ask_volume),     dx_ft_common_field },
    { dx_fid_compact_int,                        L"OpenInterest",  DX_RECORD_FIELD_STDOPS(dx_candle_t, open_interest),  dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"ImpVolatility", DX_RECORD_FIELD_STDOPS(dx_candle_t, imp_volatility), dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	TradeETH data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_trade_eth[] = {
	{ dx_fid_compact_int,                       L"ETHLast.Time",      DX_RECORD_FIELD_STDOPS(dx_trade_t, time),          dx_ft_common_field },
	{ dx_fid_compact_int,                       L"ETHLast.Sequence",  DX_RECORD_FIELD_STDOPS(dx_trade_t, sequence),      dx_ft_common_field },
	{ dx_fid_compact_int,                       L"Last.TimeNanoPart", DX_RECORD_FIELD_STDOPS(dx_trade_t, time_nanos),    dx_ft_common_field },
	{ dx_fid_utf_char,                          L"ETHLast.Exchange",  DX_RECORD_FIELD_STDOPS(dx_trade_t, exchange_code), dx_ft_common_field },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"ETHLast.Price",     DX_RECORD_FIELD_STDOPS(dx_trade_t, price),         dx_ft_common_field },
	{ dx_fid_compact_int,                       L"ETHLast.Size",      DX_RECORD_FIELD_STDOPS(dx_trade_t, size),          dx_ft_common_field },
	{ dx_fid_compact_int,                       L"ETHLast.Flags",     DX_RECORD_FIELD_STDOPS(dx_trade_t, flags),         dx_ft_common_field },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"ETHVolume",         DX_RECORD_FIELD_STDOPS(dx_trade_t, day_volume),    dx_ft_common_field },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"ETHDayTurnover",    DX_RECORD_FIELD_STDOPS(dx_trade_t, day_turnover),  dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Spread Order data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_spread_order[] = {
	{ dx_fid_compact_int,                       L"Index",        DX_RECORD_FIELD_STDOPS(dx_spread_order_t, index),         dx_ft_second_time_int_field },
    { dx_fid_compact_int,                       L"Time",         DX_RECORD_FIELD_STDOPS(dx_spread_order_t, time),          dx_ft_common_field },
    { dx_fid_compact_int,                       L"Sequence",     DX_RECORD_FIELD_STDOPS(dx_spread_order_t, sequence),      dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Price",        DX_RECORD_FIELD_STDOPS(dx_spread_order_t, price),         dx_ft_common_field },
    { dx_fid_compact_int,                       L"Size",         DX_RECORD_FIELD_STDOPS(dx_spread_order_t, size),          dx_ft_common_field },
    { dx_fid_compact_int,                       L"Count",        DX_RECORD_FIELD_STDOPS(dx_spread_order_t, count),         dx_ft_common_field },
    { dx_fid_compact_int,                       L"Flags",        DX_RECORD_FIELD_STDOPS(dx_spread_order_t, flags),         dx_ft_common_field },
    { dx_fid_utf_char_array,                    L"SpreadSymbol", DX_RECORD_FIELD_STDOPS(dx_spread_order_t, spread_symbol), dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Greeks data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_greeks[] = {
	{ dx_fid_compact_int | dx_fid_flag_time,     L"Time",         DX_RECORD_FIELD_STDOPS(dx_greeks_t, time),       dx_ft_first_time_int_field  },
    { dx_fid_compact_int | dx_fid_flag_sequence, L"Sequence",     DX_RECORD_FIELD_STDOPS(dx_greeks_t, sequence),   dx_ft_second_time_int_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Greeks.Price", DX_RECORD_FIELD_STDOPS(dx_greeks_t, price),      dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Volatility",   DX_RECORD_FIELD_STDOPS(dx_greeks_t, volatility), dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Delta",        DX_RECORD_FIELD_STDOPS(dx_greeks_t, delta),      dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Gamma",        DX_RECORD_FIELD_STDOPS(dx_greeks_t, gamma),      dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Theta",        DX_RECORD_FIELD_STDOPS(dx_greeks_t, theta),      dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Rho",          DX_RECORD_FIELD_STDOPS(dx_greeks_t, rho),        dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Vega",         DX_RECORD_FIELD_STDOPS(dx_greeks_t, vega),       dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	TheoPrice data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_theo_price[] = {
	{ dx_fid_compact_int | dx_fid_flag_time,    L"Theo.Time",            DX_RECORD_FIELD_STDOPS(dx_theo_price_t, time),             dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Theo.Price",           DX_RECORD_FIELD_STDOPS(dx_theo_price_t, price),            dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Theo.UnderlyingPrice", DX_RECORD_FIELD_STDOPS(dx_theo_price_t, underlying_price), dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Theo.Delta",           DX_RECORD_FIELD_STDOPS(dx_theo_price_t, delta),            dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Theo.Gamma",           DX_RECORD_FIELD_STDOPS(dx_theo_price_t, gamma),            dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Theo.Dividend",        DX_RECORD_FIELD_STDOPS(dx_theo_price_t, dividend),         dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Theo.Interest",        DX_RECORD_FIELD_STDOPS(dx_theo_price_t, interest),         dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Underlying data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_underlying[] = {
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Volatility",      DX_RECORD_FIELD_STDOPS(dx_underlying_t, volatility),       dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"FrontVolatility", DX_RECORD_FIELD_STDOPS(dx_underlying_t, front_volatility), dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"BackVolatility",  DX_RECORD_FIELD_STDOPS(dx_underlying_t, back_volatility),  dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"PutCallRatio",    DX_RECORD_FIELD_STDOPS(dx_underlying_t, put_call_ratio),   dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Series data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_series[] = {
    { dx_fid_compact_int,                        L"Index",        DX_RECORD_FIELD_STDOPS(dx_series_t, index),          dx_ft_second_time_int_field },
    { dx_fid_compact_int,                        L"Time",         DX_RECORD_FIELD_STDOPS(dx_series_t, time),           dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_sequence, L"Sequence",     DX_RECORD_FIELD_STDOPS(dx_series_t, sequence),       dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_date,     L"Expiration",   DX_RECORD_FIELD_STDOPS(dx_series_t, expiration),     dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Volatility",   DX_RECORD_FIELD_STDOPS(dx_series_t, volatility),     dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"PutCallRatio", DX_RECORD_FIELD_STDOPS(dx_series_t, put_call_ratio), dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"ForwardPrice", DX_RECORD_FIELD_STDOPS(dx_series_t, forward_price),  dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Dividend",     DX_RECORD_FIELD_STDOPS(dx_series_t, dividend),       dx_ft_common_field },
    { dx_fid_compact_int | dx_fid_flag_decimal,  L"Interest",     DX_RECORD_FIELD_STDOPS(dx_series_t, interest),       dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Configuration data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_configuration[] = {
	{ dx_fid_compact_int,                            L"Version",       DX_RECORD_FIELD_STDOPS(dx_configuration_t, version), dx_ft_common_field },
	{ dx_fid_byte_array | dx_fid_flag_serial_object, L"Configuration", DX_RECORD_FIELD_STDOPS(dx_configuration_t, object),  dx_ft_common_field }
};

/* -------------------------------------------------------------------------- */
/*
 *	Records
 */
/* -------------------------------------------------------------------------- */

static const int g_record_field_counts[dx_rid_count] = {
	sizeof(dx_fields_trade) / sizeof(dx_fields_trade[0]),
	sizeof(dx_fields_quote) / sizeof(dx_fields_quote[0]),
	sizeof(dx_fields_summary) / sizeof(dx_fields_summary[0]),
	sizeof(dx_fields_profile) / sizeof(dx_fields_profile[0]),
	sizeof(dx_fields_market_maker) / sizeof(dx_fields_market_maker[0]),
	sizeof(dx_fields_order) / sizeof(dx_fields_order[0]),
	sizeof(dx_fields_time_and_sale) / sizeof(dx_fields_time_and_sale[0]),
	sizeof(dx_fields_candle) / sizeof(dx_fields_candle[0]),
	sizeof(dx_fields_trade_eth) / sizeof(dx_fields_trade_eth[0]),
	sizeof(dx_fields_spread_order) / sizeof(dx_fields_spread_order[0]),
	sizeof(dx_fields_greeks) / sizeof(dx_fields_greeks[0]),
	sizeof(dx_fields_theo_price) / sizeof(dx_fields_theo_price[0]),
	sizeof(dx_fields_underlying) / sizeof(dx_fields_underlying[0]),
	sizeof(dx_fields_series) / sizeof(dx_fields_series[0]),
	sizeof(dx_fields_configuration) / sizeof(dx_fields_configuration[0])
};

static const dx_record_info_t g_record_info[dx_rid_count] = {
	{ L"Trade", sizeof(dx_fields_trade) / sizeof(dx_fields_trade[0]), dx_fields_trade },
	{ L"Quote", sizeof(dx_fields_quote) / sizeof(dx_fields_quote[0]), dx_fields_quote },
	{ L"Summary", sizeof(dx_fields_summary) / sizeof(dx_fields_summary[0]), dx_fields_summary },
	{ L"Profile", sizeof(dx_fields_profile) / sizeof(dx_fields_profile[0]), dx_fields_profile },
	{ L"MarketMaker", sizeof(dx_fields_market_maker) / sizeof(dx_fields_market_maker[0]), dx_fields_market_maker },
	{ L"Order", sizeof(dx_fields_order) / sizeof(dx_fields_order[0]), dx_fields_order },
	{ L"TimeAndSale", sizeof(dx_fields_time_and_sale) / sizeof(dx_fields_time_and_sale[0]), dx_fields_time_and_sale },
	{ L"Candle", sizeof(dx_fields_candle) / sizeof(dx_fields_candle[0]), dx_fields_candle },
	{ L"TradeETH", sizeof(dx_fields_trade_eth) / sizeof(dx_fields_trade_eth[0]), dx_fields_trade_eth },
	{ L"SpreadOrder", sizeof(dx_fields_spread_order) / sizeof(dx_fields_spread_order[0]), dx_fields_spread_order },
	{ L"Greeks", sizeof(dx_fields_greeks) / sizeof(dx_fields_greeks[0]), dx_fields_greeks },
	{ L"TheoPrice", sizeof(dx_fields_theo_price) / sizeof(dx_fields_theo_price[0]), dx_fields_theo_price },
	{ L"Underlying", sizeof(dx_fields_underlying) / sizeof(dx_fields_underlying[0]), dx_fields_underlying },
	{ L"Series", sizeof(dx_fields_series) / sizeof(dx_fields_series[0]), dx_fields_series },
	{ L"Configuration", sizeof(dx_fields_configuration) / sizeof(dx_fields_configuration[0]), dx_fields_configuration }
};

/* -------------------------------------------------------------------------- */
/*
 *	Data structures connection context
 */
/* -------------------------------------------------------------------------- */

#define RECORD_ID_VECTOR_SIZE   1000

typedef struct {
	dxf_int_t server_record_id;
	dx_record_id_t local_record_id;
} dx_record_id_pair_t;

typedef struct {
	dx_record_id_pair_t* elements;
	size_t size;
	size_t capacity;
} dx_record_id_map_t;

typedef struct {
	dx_record_id_t frequent_ids[RECORD_ID_VECTOR_SIZE];
	dx_record_id_map_t id_map;
} dx_server_local_record_id_map_t;

/* Struct is stores record items
 *      elements        - pointer to record items array
 *      size            - size of elements array
 *      capacity        - capacity of elements array
 *      new_record_id   - the index of the first unsubscribe record to server;
 *                        when record will be subscribe, this value will be equal to 'size'
 */
typedef struct {
	dx_record_item_t* elements;
	size_t size;
	size_t capacity;
	dx_record_id_t new_record_id;
} dx_record_list_t;

typedef struct {
	dxf_connection_t connection;

	dx_server_local_record_id_map_t record_id_map;
	dx_record_server_support_state_list_t record_server_support_states;
	dx_record_list_t records_list;
	dx_mutex_t guard_records_list;
} dx_data_structures_connection_context_t;

#define CTX(context) \
	((dx_data_structures_connection_context_t*)context)

/* -------------------------------------------------------------------------- */

void dx_clear_record_server_support_states(dx_data_structures_connection_context_t* context) {
	dx_record_server_support_state_list_t *states = NULL;
	if (context == NULL)
		return;
	states = &(context->record_server_support_states);
	if (states->elements == NULL)
		return;

	dx_free(states->elements);
	states->elements = NULL;
	states->size = 0;
	states->capacity = 0;
}

void dx_clear_record_info(dx_record_item_t *record) {
	dx_free(record->name);
	record->name = NULL;
}

void dx_clear_records_list(dx_data_structures_connection_context_t* context) {
	dx_record_list_t* records_list = &(context->records_list);
	dx_record_id_t i;
	dx_record_item_t* record = records_list->elements;

	dx_mutex_lock(&context->guard_records_list);

	dx_record_id_t records_count = (dx_record_id_t)records_list->size;
	for (i = 0; i < records_count; i++) {
		dx_clear_record_info(record);
		record++;
	}
	dx_free(records_list->elements);
	records_list->elements = NULL;
	records_list->capacity = 0;
	records_list->size = 0;
	records_list->new_record_id = 0;

	dx_mutex_unlock(&context->guard_records_list);
}

void dx_free_data_structures_context(dx_data_structures_connection_context_t** context) {
	if (context == NULL)
		return;
	dx_clear_record_server_support_states(*context);
	dx_clear_records_list(*context);
	dx_mutex_destroy(&(*context)->guard_records_list);
	dx_free(*context);
	context = NULL;
}

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_data_structures) {
	dx_data_structures_connection_context_t* context = dx_calloc(1, sizeof(dx_data_structures_connection_context_t));
	int i = 0;

	if (context == NULL) {
		return false;
	}

	context->connection = connection;
	context->record_server_support_states.elements = NULL;
	context->record_server_support_states.size = 0;
	context->record_server_support_states.capacity = 0;
	context->records_list.elements = NULL;
	context->records_list.size = 0;
	context->records_list.capacity = 0;
	dx_mutex_create(&context->guard_records_list);

	for (; i < RECORD_ID_VECTOR_SIZE; ++i) {
		context->record_id_map.frequent_ids[i] = DX_RECORD_ID_INVALID;
	}

	if (!dx_set_subsystem_data(connection, dx_ccs_data_structures, context)) {
		dx_free_data_structures_context(&context);

		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_data_structures) {
	bool res = true;
	dx_data_structures_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_data_structures, &res);

	if (context == NULL) {
		return res;
	}

	CHECKED_FREE(context->record_id_map.id_map.elements);
	dx_free_data_structures_context(&context);

	return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_data_structures) {
	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Connection context functions implementation
 */
/* -------------------------------------------------------------------------- */

void* dx_get_data_structures_connection_context (dxf_connection_t connection) {
	return dx_get_subsystem_data(connection, dx_ccs_data_structures, NULL);
}

/* -------------------------------------------------------------------------- */
/*
 *	Record functions implementation
 */
/* -------------------------------------------------------------------------- */

#define RECORD_ID_PAIR_COMPARATOR(left, right) \
	DX_NUMERIC_COMPARATOR((left).server_record_id, (right).server_record_id)

dx_record_id_t dx_get_record_id(void* context, dxf_int_t server_record_id) {
	dx_server_local_record_id_map_t* record_id_map = &(CTX(context)->record_id_map);

	if (server_record_id >= 0 && server_record_id < RECORD_ID_VECTOR_SIZE) {
		return record_id_map->frequent_ids[server_record_id];
	} else {
		size_t idx;
		bool found = false;
		dx_record_id_pair_t dummy;

		dummy.server_record_id = server_record_id;

		DX_ARRAY_SEARCH(record_id_map->id_map.elements, 0, record_id_map->id_map.size, dummy, RECORD_ID_PAIR_COMPARATOR, true, found, idx);

		if (!found) {
			return DX_RECORD_ID_INVALID;
		}

		return record_id_map->id_map.elements[(dx_record_id_t)idx].local_record_id;
	}
}

/* -------------------------------------------------------------------------- */

bool dx_assign_server_record_id(void* context, dx_record_id_t record_id, dxf_int_t server_record_id) {
	dx_server_local_record_id_map_t* record_id_map = &(CTX(context)->record_id_map);

	if (server_record_id >= 0 && server_record_id < RECORD_ID_VECTOR_SIZE) {
		record_id_map->frequent_ids[server_record_id] = record_id;
	} else {
		size_t idx;
		bool found = false;
		bool failed = false;
		dx_record_id_pair_t rip;

		rip.server_record_id = server_record_id;
		rip.local_record_id = record_id;

		DX_ARRAY_SEARCH(record_id_map->id_map.elements, 0, record_id_map->id_map.size, rip, RECORD_ID_PAIR_COMPARATOR, true, found, idx);

		if (found) {
			record_id_map->id_map.elements[idx].local_record_id = record_id;

			return true;
		}

		DX_ARRAY_INSERT(record_id_map->id_map, dx_record_id_pair_t, rip, idx, dx_capacity_manager_halfer, failed);

		if (failed) {
			return false;
		}
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_validate_record_id(void* context, dx_record_id_t record_id) {
	dx_record_list_t* records_list = &(CTX(context)->records_list);
	if (record_id < 0 || record_id >= (dx_record_id_t)records_list->size) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}
	return true;
}

/* -------------------------------------------------------------------------- */

/*
 * Returns pointer to record data.
 * Don't modify any field of struct and don't free this resources.
 *
 * context - a data structures connection context.
 * record_id - id of record to get data.
 * Return: pointer to records item or NULL if connection or record_id is not valid.
 */
const dx_record_item_t* dx_get_record_by_id(void* context, dx_record_id_t record_id) {
	dx_record_item_t* value = NULL;
	dx_data_structures_connection_context_t* dscc = CTX(context);
	dx_record_list_t* records_list = &(dscc->records_list);
	dx_mutex_lock(&dscc->guard_records_list);
	if (!dx_validate_record_id(context, record_id)) {
		dx_mutex_unlock(&dscc->guard_records_list);
		return NULL;
	}
	value = &records_list->elements[record_id];
	dx_mutex_unlock(&dscc->guard_records_list);
	return value;
}

/* -------------------------------------------------------------------------- */

dx_record_id_t dx_get_record_id_by_name(void* context, dxf_const_string_t record_name) {
	dx_record_id_t record_id;
	dx_data_structures_connection_context_t* dscc = CTX(context);
	dx_record_list_t* records_list = &(dscc->records_list);

	dx_mutex_lock(&dscc->guard_records_list);

	for (record_id = 0; record_id < (dx_record_id_t)records_list->size; ++record_id) {
		if (dx_compare_strings(records_list->elements[record_id].name, record_name) == 0) {
			dx_mutex_unlock(&dscc->guard_records_list);
			return record_id;
		}
	}

	dx_mutex_unlock(&dscc->guard_records_list);

	return DX_RECORD_ID_INVALID;
}

/* -------------------------------------------------------------------------- */

dx_record_id_t dx_get_next_unsubscribed_record_id(void* context, bool isUpdate) {
	dx_record_id_t record_id = DX_RECORD_ID_INVALID;
	dx_data_structures_connection_context_t* dscc = CTX(context);
	dx_record_list_t* records_list = &(dscc->records_list);

	dx_mutex_lock(&dscc->guard_records_list);

	if (records_list->new_record_id < (dx_record_id_t)records_list->size && isUpdate) {
		records_list->new_record_id += 1;
	}

	record_id = records_list->new_record_id;

	dx_mutex_unlock(&dscc->guard_records_list);

	return record_id;
}

/* -------------------------------------------------------------------------- */

void dx_drop_unsubscribe_counter(void* context) {
	dx_data_structures_connection_context_t* dscc = CTX(context);
	dx_record_list_t* records_list = &(dscc->records_list);

	dx_mutex_lock(&dscc->guard_records_list);

	records_list->new_record_id = 0;

	dx_mutex_unlock(&dscc->guard_records_list);
}

/* -------------------------------------------------------------------------- */

int dx_find_record_field(const dx_record_item_t* record_info, dxf_const_string_t field_name,
						dxf_int_t field_type) {
	int cur_field_index = 0;
	dx_field_info_t* fields = (dx_field_info_t*)record_info->fields;

	for (; cur_field_index < record_info->field_count; ++cur_field_index) {
		if (dx_compare_strings(fields[cur_field_index].name, field_name) == 0 &&
			(fields[cur_field_index].type & dx_fid_mask_serialization)== (field_type & dx_fid_mask_serialization)) {
			return cur_field_index;
		}
	}

	return INVALID_INDEX;
}

/* -------------------------------------------------------------------------- */

dxf_char_t dx_get_record_exchange_code(void* context, dx_record_id_t record_id) {
	dx_data_structures_connection_context_t* dscc = CTX(context);
	dx_record_list_t* records_list = &(dscc->records_list);
	dxf_char_t exchange_code = 0;

	dx_mutex_lock(&dscc->guard_records_list);

	if (dx_validate_record_id(context, record_id))
		exchange_code = records_list->elements[record_id].exchange_code;

	dx_mutex_unlock(&dscc->guard_records_list);

	return exchange_code;
}

bool dx_set_record_exchange_code(void* context, dx_record_id_t record_id,
								dxf_char_t exchange_code) {
	dx_data_structures_connection_context_t* dscc = CTX(context);
	dx_record_list_t* records_list = &(dscc->records_list);
	dx_mutex_lock(&dscc->guard_records_list);
	if (!dx_validate_record_id(context, record_id)) {
		dx_mutex_unlock(&dscc->guard_records_list);
		return false;
	}

	records_list->elements[record_id].exchange_code = exchange_code;

	dx_mutex_unlock(&dscc->guard_records_list);

	return true;
}

/* -------------------------------------------------------------------------- */

/*
 * Creates subscription time field according to record model. Function uses
 * dx_ft_first_time_int_field and dx_ft_second_time_int_field flags of the
 * record to compose time value. This time value is necessary for correct event
 * subscription.
 *
 * context      - data structures connection context pointer
 * record_id    - subscribed record id
 * time         - unix time in milliseconds
 * value        - the result subscription time
 * return true if no errors occur otherwise returns false
 */
bool dx_create_subscription_time(void* context, dx_record_id_t record_id,
								dxf_long_t time, OUT dxf_long_t* value) {
	int i;
	bool has_first_time = false;
	bool has_second_time = false;
	bool is_timed_record = false;
	dxf_int_t seconds = dx_get_seconds_from_time(time);
	dxf_int_t millis = dx_get_millis_from_time(time);
	dxf_long_t subscription_time = 0;
	dx_data_structures_connection_context_t* dscc = CTX(context);
	const dx_record_item_t* record_info = NULL;

	if (context == NULL || value == NULL)
		return dx_set_error_code(dx_ec_invalid_func_param_internal);

	if (time == 0) {
		*value = 0;
		return true;
	}

	record_info = dx_get_record_by_id(dscc, record_id);
	if (record_info == NULL)
		return dx_set_error_code(dx_ec_invalid_func_param_internal);

	for (i = 0; i < record_info->field_count; ++i) {
		dx_field_info_t field = record_info->fields[i];
		if (field.time == dx_ft_first_time_int_field) {
			subscription_time |= ((dxf_long_t)seconds) << 32;
			has_first_time = true;
			is_timed_record = ((field.type & dx_fid_flag_time) & ~dx_fid_flag_decimal) != 0;
		} else if (field.time == dx_ft_second_time_int_field) {
			subscription_time |= millis;
			has_second_time = true;
		}
		if (has_first_time && has_second_time)
			break;
	}

	//if record have pseudo time field just returns input time value
	*value = is_timed_record ? subscription_time : time;

	return true;
}

/* -------------------------------------------------------------------------- */

dx_record_server_support_state_list_t* dx_get_record_server_support_states(void* context) {
	return &(CTX(context)->record_server_support_states);
}

bool dx_get_record_server_support_state_value(dx_record_server_support_state_list_t* states,
											dx_record_id_t record_id,
											OUT dx_record_server_support_state_t **value) {
	if (record_id < 0 || record_id >= (dx_record_id_t)states->size)
		return false;
	*value = &(states->elements[record_id]);
	return true;
}

/* -------------------------------------------------------------------------- */

/* Functions for working with records list */

bool dx_add_record_to_list(dxf_connection_t connection, dx_record_item_t record, dx_record_id_t index) {
	bool failed = false;
	dx_record_item_t new_record;
	dx_record_server_support_state_t new_state;
	dx_data_structures_connection_context_t* dscc = NULL;

	/* Getting context */
	dscc = dx_get_data_structures_connection_context(connection);
	if (dscc == NULL) {
		return dx_set_error_code(dx_cec_connection_context_not_initialized);
	}

	/* Update records list */
	new_record.name = dx_create_string_src(record.name);
	new_record.field_count = record.field_count;
	new_record.fields = record.fields;
	new_record.info_id = record.info_id;
	dx_memcpy(new_record.suffix, record.suffix, sizeof(record.suffix));
	new_record.exchange_code = record.exchange_code;

	DX_ARRAY_INSERT(dscc->records_list, dx_record_item_t, new_record, index, dx_capacity_manager_halfer, failed);

	if (failed) {
		return dx_set_error_code(dx_sec_not_enough_memory);
	}

	/* Update record server support states */
	new_state = 0;
	DX_ARRAY_INSERT(dscc->record_server_support_states, dx_record_server_support_state_t, new_state, index, dx_capacity_manager_halfer, failed);

	if (failed) {
		DX_ARRAY_DELETE(dscc->records_list, dx_record_item_t, index, dx_capacity_manager_halfer, failed);
		return dx_set_error_code(dx_sec_not_enough_memory);
	}

	/* Update record digests */
	if (!dx_add_record_digest_to_list(connection, index)) {
		DX_ARRAY_DELETE(dscc->records_list, dx_record_item_t, index, dx_capacity_manager_halfer, failed);
		DX_ARRAY_DELETE(dscc->record_server_support_states, dx_record_server_support_state_t, index, dx_capacity_manager_halfer, failed);
		return false;
	}

	return true;
}

dx_record_info_id_t dx_string_to_record_info(dxf_const_string_t name)
{
	// Note: dx_rid_trade_eth must be always go before dx_rid_trade to avoid
	//       wrong record detection by name
	if (dx_compare_strings_num(name, g_record_info[dx_rid_trade_eth].default_name,
							dx_string_length(g_record_info[dx_rid_trade_eth].default_name)) == 0)
		return dx_rid_trade_eth;
	else if (dx_compare_strings_num(name, g_record_info[dx_rid_trade].default_name,
									dx_string_length(g_record_info[dx_rid_trade].default_name)) == 0)
		return dx_rid_trade;
	else if (dx_compare_strings_num(name, g_record_info[dx_rid_quote].default_name,
									dx_string_length(g_record_info[dx_rid_quote].default_name)) == 0)
		return dx_rid_quote;
	else if (dx_compare_strings_num(name, g_record_info[dx_rid_summary].default_name,
									dx_string_length(g_record_info[dx_rid_summary].default_name)) == 0)
		return dx_rid_summary;
	else if (dx_compare_strings_num(name, g_record_info[dx_rid_profile].default_name,
									dx_string_length(g_record_info[dx_rid_profile].default_name)) == 0)
		return dx_rid_profile;
	else if (dx_compare_strings_num(name, g_record_info[dx_rid_market_maker].default_name,
									dx_string_length(g_record_info[dx_rid_market_maker].default_name)) == 0)
		return dx_rid_market_maker;
	else if (dx_compare_strings_num(name, g_record_info[dx_rid_order].default_name,
									dx_string_length(g_record_info[dx_rid_order].default_name)) == 0)
		return dx_rid_order;
	else if (dx_compare_strings_num(name, g_record_info[dx_rid_time_and_sale].default_name,
									dx_string_length(g_record_info[dx_rid_time_and_sale].default_name)) == 0)
		return dx_rid_time_and_sale;
	else if (dx_compare_strings_num(name, g_record_info[dx_rid_candle].default_name,
									dx_string_length(g_record_info[dx_rid_candle].default_name)) == 0)
		return dx_rid_candle;
	else if (dx_compare_strings_num(name, g_record_info[dx_rid_spread_order].default_name,
									dx_string_length(g_record_info[dx_rid_spread_order].default_name)) == 0)
		return dx_rid_spread_order;
	else if (dx_compare_strings_num(name, g_record_info[dx_rid_greeks].default_name,
									dx_string_length(g_record_info[dx_rid_greeks].default_name)) == 0)
		return dx_rid_greeks;
	else if (dx_compare_strings_num(name, g_record_info[dx_rid_theo_price].default_name,
									dx_string_length(g_record_info[dx_rid_theo_price].default_name)) == 0)
		return dx_rid_theo_price;
	else if (dx_compare_strings_num(name, g_record_info[dx_rid_underlying].default_name,
									dx_string_length(g_record_info[dx_rid_underlying].default_name)) == 0)
		return dx_rid_underlying;
	else if (dx_compare_strings_num(name, g_record_info[dx_rid_series].default_name,
									dx_string_length(g_record_info[dx_rid_series].default_name)) == 0)
		return dx_rid_series;
	else if (dx_compare_strings_num(name, g_record_info[dx_rid_configuration].default_name,
									dx_string_length(g_record_info[dx_rid_configuration].default_name)) == 0)
		return dx_rid_configuration;
	else
		return dx_rid_invalid;
}

bool init_record_info(dx_record_item_t *record, dxf_const_string_t name) {
	dx_record_info_id_t record_info_id;
	dx_record_info_t record_info;
	size_t suffix_index;
	size_t name_length = dx_string_length(name);

	record_info_id = dx_string_to_record_info(name);
	if (record_info_id == dx_rid_invalid) {
		dx_set_last_error(dx_ec_invalid_func_param_internal);
		return false;
	}
	record_info = g_record_info[record_info_id];

	record->name = dx_create_string_src(name);
	record->field_count = g_record_info[record_info_id].field_count;
	record->fields = g_record_info[record_info_id].fields;
	record->info_id = record_info_id;
	dx_memset(record->suffix, 0, sizeof(record->suffix));
	record->exchange_code = 0;

	suffix_index = dx_string_length(record_info.default_name);
	if (name_length < suffix_index + 1)
		return true;
	if (record_info_id == dx_rid_order ||
		record_info_id == dx_rid_spread_order) {
		if (record->name[suffix_index] != L'#')
			return true;
		dx_copy_string_len(record->suffix, &record->name[suffix_index + 1], name_length - suffix_index);
	} else if (record_info_id == dx_rid_trade ||
			record_info_id == dx_rid_quote ||
			record_info_id == dx_rid_summary ||
			record_info_id == dx_rid_trade_eth ||
			record_info_id == dx_rid_time_and_sale) {
		if (record->name[suffix_index] != L'&')
			return true;
		dx_copy_string_len(record->suffix, &record->name[suffix_index + 1], 1);
		record->exchange_code = record->suffix[0];
	}

	return true;
}

#define DX_RECORDS_COMPARATOR(l, r) (dx_compare_strings(l.name, r.name))

dx_record_id_t dx_add_or_get_record_id(dxf_connection_t connection, dxf_const_string_t name) {
	bool result = true;
	bool found = false;
	dx_record_id_t index = DX_RECORD_ID_INVALID;
	dx_record_item_t record;
	dx_data_structures_connection_context_t* dscc = NULL;

	/* Getting context */
	dscc = dx_get_data_structures_connection_context(connection);
	if (dscc == NULL) {
		dx_set_error_code(dx_cec_connection_context_not_initialized);
		return DX_RECORD_ID_INVALID;
	}

	dx_mutex_lock(&dscc->guard_records_list);

	if (!init_record_info(&record, name)) {
		dx_mutex_unlock(&dscc->guard_records_list);
		return DX_RECORD_ID_INVALID;
	}

	if (dscc->records_list.elements == NULL) {
		index = 0;
		result = dx_add_record_to_list(connection, record, index);
	} else {
		size_t search_res;
		DX_ARRAY_SEARCH(dscc->records_list.elements, 0, dscc->records_list.size, record, DX_RECORDS_COMPARATOR, false, found, search_res);
		index = (dx_record_id_t)search_res;
		if (!found) {
			result = dx_add_record_to_list(connection, record, index);
		}
	}

	dx_clear_record_info(&record);

	dx_mutex_unlock(&dscc->guard_records_list);

	if (!result) {
		dx_logging_last_error();
		return DX_RECORD_ID_INVALID;
	}

	return index;
}

dx_record_id_t dx_get_records_list_count(void* context) {
	size_t size = 0;
	dx_data_structures_connection_context_t* dscc = CTX(context);

	dx_mutex_lock(&dscc->guard_records_list);

	size = dscc->records_list.size;

	dx_mutex_unlock(&dscc->guard_records_list);

	return (dx_record_id_t)size;
}
