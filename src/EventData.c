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

#include "EventData.h"
#include "DXAlgorithms.h"
#include "DXErrorHandling.h"
#include "Logger.h"
#include "EventSubscription.h"
#include "DataStructures.h"

/* -------------------------------------------------------------------------- */
/*
 *	Various common data
 */
/* -------------------------------------------------------------------------- */

static const int g_event_data_sizes[dx_eid_count] = {
	sizeof(dxf_trade_t),
	sizeof(dxf_quote_t),
	sizeof(dxf_summary_t),
	sizeof(dxf_profile_t),
	sizeof(dxf_order_t),
	sizeof(dxf_time_and_sale_t),
	sizeof(dxf_candle_t),
	sizeof(dxf_trade_t), // As trade ETH
	sizeof(dxf_order_t), // As spread order
	sizeof(dxf_greeks_t),
	sizeof(dxf_theo_price_t),
	sizeof(dxf_underlying_t),
	sizeof(dxf_series_t),
	sizeof(dxf_configuration_t)
};

static const dxf_char_t g_quote_tmpl[] = L"Quote&";
static const dxf_char_t g_order_tmpl[] = L"Order#";
static const dxf_char_t g_trade_tmpl[] = L"Trade&";
static const dxf_char_t g_summary_tmpl[] = L"Summary&";
static const dxf_char_t g_trade_eth_tmpl[] = L"TradeETH&";
static const dxf_char_t g_time_and_sale_tmpl[] = L"TimeAndSale&";

#define STRLEN(char_array) (sizeof(char_array) / sizeof(char_array[0]) - 1)
#define QUOTE_TMPL_LEN STRLEN(g_quote_tmpl)
#define ORDER_TMPL_LEN STRLEN(g_order_tmpl)
#define TRADE_TMPL_LEN STRLEN(g_trade_tmpl)
#define SUMMARY_TMPL_LEN STRLEN(g_summary_tmpl)
#define TRADE_ETH_TMPL_LEN STRLEN(g_trade_eth_tmpl)
#define TIME_AND_SALE_TMPL_LEN STRLEN(g_time_and_sale_tmpl)

/* -------------------------------------------------------------------------- */
/*
 *	Event functions implementation
 */
/* -------------------------------------------------------------------------- */

dxf_const_string_t dx_event_type_to_string (int event_type) {
	switch (event_type) {
	case DXF_ET_TRADE: return L"Trade";
	case DXF_ET_QUOTE: return L"Quote";
	case DXF_ET_SUMMARY: return L"Summary";
	case DXF_ET_PROFILE: return L"Profile";
	case DXF_ET_ORDER: return L"Order";
	case DXF_ET_TIME_AND_SALE: return L"Time&Sale";
	case DXF_ET_CANDLE: return L"Candle";
	case DXF_ET_TRADE_ETH: return L"TradeETH";
	case DXF_ET_SPREAD_ORDER: return L"SpreadOrder";
	case DXF_ET_GREEKS: return L"Greeks";
	case DXF_ET_THEO_PRICE: return L"TheoPrice";
	case DXF_ET_UNDERLYING: return L"Underlying";
	case DXF_ET_SERIES: return L"Series";
	case DXF_ET_CONFIGURATION: return L"Configuration";
	default: return L"";
	}
}

/* -------------------------------------------------------------------------- */

int dx_get_event_data_struct_size (int event_id) {
	return g_event_data_sizes[(dx_event_id_t)event_id];
}

/* -------------------------------------------------------------------------- */

dx_event_id_t dx_get_event_id_by_bitmask (int event_bitmask) {
	dx_event_id_t event_id = dx_eid_begin;

	if (!dx_is_only_single_bit_set(event_bitmask)) {
		return dx_eid_invalid;
	}

	for (; (event_bitmask >>= 1) != 0; ++event_id);

	return event_id;
}

/* -------------------------------------------------------------------------- */
/*
 *	Event subscription implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_add_subscription_param_to_list(dxf_connection_t connection, dx_event_subscription_param_list_t* param_list,
										dxf_const_string_t record_name, dx_subscription_type_t subscription_type) {
	bool failed = false;
	dx_event_subscription_param_t param;
	dx_record_id_t record_id = dx_add_or_get_record_id(connection, record_name);
	if (record_id < 0) {
		dx_set_last_error(dx_esec_invalid_subscr_id);
		return false;
	}

	param.record_id = record_id;
	param.subscription_type = subscription_type;
	DX_ARRAY_INSERT(*param_list, dx_event_subscription_param_t, param, param_list->size, dx_capacity_manager_halfer, failed);
	if (failed)
		dx_set_last_error(dx_sec_not_enough_memory);
	return !failed;
}

dx_subscription_type_t dx_infer_subscription_type(dx_event_subscr_flag subscr_flags, dx_subscription_type_t default_type) {
	dx_subscription_type_t subscription_type = default_type;

	if (IS_FLAG_SET(subscr_flags, dx_esf_wildcard) || IS_FLAG_SET(subscr_flags, dx_esf_force_stream)) {
		subscription_type = dx_st_stream;
	} else if (IS_FLAG_SET(subscr_flags, dx_esf_force_ticker)) {
		subscription_type = dx_st_ticker;
	} else if (IS_FLAG_SET(subscr_flags, dx_esf_force_history)) {
		subscription_type = dx_st_history;
	}

	return subscription_type;
}

bool dx_get_single_order_subscription_params(dxf_connection_t connection, dx_order_source_array_ptr_t order_source,
											 dx_event_subscr_flag subscr_flags,
											OUT dx_event_subscription_param_list_t* param_list) {
	dxf_char_t order_name_buf[ORDER_TMPL_LEN + DXF_RECORD_SUFFIX_SIZE] = { 0 };

	if (!IS_FLAG_SET(subscr_flags, dx_esf_single_record)) {
		return false;
	}

	const dx_subscription_type_t subscription_type = dx_infer_subscription_type(subscr_flags, dx_st_history);

	if (IS_FLAG_SET(subscr_flags, dx_esf_sr_market_maker_order)) {
		CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, L"MarketMaker", subscription_type);
	} else {
		if (order_source->size > 1) {
			return false;
		}
		if (order_source->size == 0) {
			CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, L"Order", subscription_type);
		} else {
			dx_copy_string(order_name_buf, g_order_tmpl);
			dx_copy_string_len(&order_name_buf[ORDER_TMPL_LEN], order_source->elements[0].suffix, DXF_RECORD_SUFFIX_SIZE);
			CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, order_name_buf, subscription_type);
		}
	}
	return true;
}

bool dx_get_quote_subscription_params(dxf_connection_t connection, dx_event_subscr_flag subscr_flags,
									OUT dx_event_subscription_param_list_t* param_list) {
	const dx_subscription_type_t subscription_type = dx_infer_subscription_type(subscr_flags, dx_st_ticker);
	dxf_char_t ch = L'A';
	dxf_char_t quote_name_buf[QUOTE_TMPL_LEN + 2] = { 0 };

	/* fill quotes Quote&A..Quote&Z */
	dx_copy_string(quote_name_buf, g_quote_tmpl);
	for (; ch <= L'Z'; ch++) {
		quote_name_buf[QUOTE_TMPL_LEN] = ch;
		CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, quote_name_buf, subscription_type);
	}

	if (!IS_FLAG_SET(subscr_flags, dx_esf_quotes_regional)) {
		return dx_add_subscription_param_to_list(connection, param_list, L"Quote", subscription_type);
	}

	return true;
}

bool dx_get_order_subscription_params(dxf_connection_t connection, dx_order_source_array_ptr_t order_source,
									dx_event_subscr_flag subscr_flags,
									OUT dx_event_subscription_param_list_t* param_list) {
	dxf_char_t ch = L'A';
	dxf_char_t order_name_buf[ORDER_TMPL_LEN + DXF_RECORD_SUFFIX_SIZE] = { 0 };
	dxf_char_t quote_name_buf[QUOTE_TMPL_LEN + 2] = { 0 };
	size_t i;

	if (IS_FLAG_SET(subscr_flags, dx_esf_single_record)) {
		return dx_get_single_order_subscription_params(connection, order_source, subscr_flags, param_list);
	}

	CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, L"Quote", dx_infer_subscription_type(subscr_flags, dx_st_ticker));
	CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, L"MarketMaker", dx_infer_subscription_type(subscr_flags, dx_st_history));
	CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, L"Order", dx_infer_subscription_type(subscr_flags, dx_st_history));

	dx_copy_string(order_name_buf, g_order_tmpl);
	for (i = 0; i < order_source->size; ++i) {
		dx_copy_string_len(&order_name_buf[ORDER_TMPL_LEN], order_source->elements[i].suffix, DXF_RECORD_SUFFIX_SIZE);
		CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, order_name_buf, dx_infer_subscription_type(subscr_flags, dx_st_history));
	}

	/* fill quotes Quote&A..Quote&Z */
	dx_copy_string(quote_name_buf, g_quote_tmpl);
	for (; ch <= L'Z'; ch++) {
		quote_name_buf[QUOTE_TMPL_LEN] = ch;
		CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, quote_name_buf, dx_infer_subscription_type(subscr_flags, dx_st_ticker));
	}

	return true;
}

bool dx_get_trade_subscription_params(dxf_connection_t connection, dx_event_subscr_flag subscr_flags, OUT dx_event_subscription_param_list_t* param_list) {
	const dx_subscription_type_t subscription_type = dx_infer_subscription_type(subscr_flags, dx_st_ticker);
	dxf_char_t ch = L'A';
	dxf_char_t trade_name_buf[TRADE_TMPL_LEN + 2] = { 0 };
	CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, L"Trade", subscription_type);

	/* fill trades Trade&A..Trade&Z */
	dx_copy_string(trade_name_buf, g_trade_tmpl);
	for (; ch <= L'Z'; ch++) {
		trade_name_buf[TRADE_TMPL_LEN] = ch;
		CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, trade_name_buf, subscription_type);
	}

	return true;
}

bool dx_get_summary_subscription_params(dxf_connection_t connection, dx_event_subscr_flag subscr_flags, OUT dx_event_subscription_param_list_t* param_list) {
	dx_subscription_type_t subscription_type = dx_infer_subscription_type(subscr_flags, dx_st_ticker);
	dxf_char_t ch = L'A';
	dxf_char_t summary_name_buf[SUMMARY_TMPL_LEN + 2] = { 0 };
	CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, L"Summary", subscription_type);

	/* fill summaries Summary&A..Summary&Z */
	dx_copy_string(summary_name_buf, g_summary_tmpl);
	for (; ch <= L'Z'; ch++) {
		summary_name_buf[SUMMARY_TMPL_LEN] = ch;
		CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, summary_name_buf, subscription_type);
	}

	return true;
}

bool dx_get_profile_subscription_params(dxf_connection_t connection, dx_event_subscr_flag subscr_flags, OUT dx_event_subscription_param_list_t* param_list) {
	const dx_subscription_type_t subscription_type = dx_infer_subscription_type(subscr_flags, dx_st_ticker);

	return dx_add_subscription_param_to_list(connection, param_list, L"Profile", subscription_type);
}

bool dx_get_time_and_sale_subscription_params(dxf_connection_t connection, dx_event_subscr_flag subscr_flags, OUT dx_event_subscription_param_list_t* param_list) {
	const dx_subscription_type_t subscription_type = dx_infer_subscription_type(subscr_flags, IS_FLAG_SET(subscr_flags, dx_esf_time_series) ? dx_st_history : dx_st_stream);

	CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, L"TimeAndSale", subscription_type);

	dxf_char_t ch = L'A';
	dxf_char_t time_and_sale_name_buf[TIME_AND_SALE_TMPL_LEN + 2] = {0};

	/* fill trades TimeAndSale&A..TimeAndSale&Z */
	dx_copy_string(time_and_sale_name_buf, g_time_and_sale_tmpl);
	for (; ch <= L'Z'; ch++) {
		time_and_sale_name_buf[TIME_AND_SALE_TMPL_LEN] = ch;
		CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, time_and_sale_name_buf, subscription_type);
	}

	return true;
}

bool dx_get_candle_subscription_params(dxf_connection_t connection, dx_event_subscr_flag subscr_flags, OUT dx_event_subscription_param_list_t* param_list) {
	const dx_subscription_type_t subscription_type = dx_infer_subscription_type(subscr_flags, dx_st_history);

	return dx_add_subscription_param_to_list(connection, param_list, L"Candle", subscription_type);
}

bool dx_get_trade_eth_subscription_params(dxf_connection_t connection, dx_event_subscr_flag subscr_flags, OUT dx_event_subscription_param_list_t* param_list) {
	const dx_subscription_type_t subscription_type = dx_infer_subscription_type(subscr_flags, dx_st_ticker);
	dxf_char_t ch = L'A';
	dxf_char_t trade_name_buf[TRADE_ETH_TMPL_LEN + 2] = { 0 };
	CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, L"TradeETH", subscription_type);

	/* fill trades TradeETH&A..TradeETH&Z */
	dx_copy_string(trade_name_buf, g_trade_eth_tmpl);
	for (; ch <= L'Z'; ch++) {
		trade_name_buf[TRADE_ETH_TMPL_LEN] = ch;
		CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, trade_name_buf, subscription_type);
	}

	return true;
}

bool dx_get_spread_order_subscription_params(dxf_connection_t connection, dx_event_subscr_flag subscr_flags, OUT dx_event_subscription_param_list_t* param_list) {
	const dx_subscription_type_t subscription_type = dx_infer_subscription_type(subscr_flags, dx_st_history);

	return dx_add_subscription_param_to_list(connection, param_list, L"SpreadOrder", subscription_type)
		&& dx_add_subscription_param_to_list(connection, param_list, L"SpreadOrder#ISE", subscription_type);
}

bool dx_get_greeks_subscription_params(dxf_connection_t connection, dx_event_subscr_flag subscr_flags, OUT dx_event_subscription_param_list_t* param_list) {
	const dx_subscription_type_t subscription_type = dx_infer_subscription_type(subscr_flags, IS_FLAG_SET(subscr_flags, dx_esf_time_series) ? dx_st_history : dx_st_ticker);

	return dx_add_subscription_param_to_list(connection, param_list, L"Greeks", subscription_type);
}

bool dx_get_theo_price_subscription_params(dxf_connection_t connection, dx_event_subscr_flag subscr_flags, OUT dx_event_subscription_param_list_t* param_list) {
	const dx_subscription_type_t subscription_type = dx_infer_subscription_type(subscr_flags, dx_st_ticker);

	return dx_add_subscription_param_to_list(connection, param_list, L"TheoPrice", subscription_type);
}

bool dx_get_underlying_subscription_params(dxf_connection_t connection, dx_event_subscr_flag subscr_flags, OUT dx_event_subscription_param_list_t* param_list) {
	const dx_subscription_type_t subscription_type = dx_infer_subscription_type(subscr_flags, dx_st_ticker);

	return dx_add_subscription_param_to_list(connection, param_list, L"Underlying", subscription_type);
}

bool dx_get_series_subscription_params(dxf_connection_t connection, dx_event_subscr_flag subscr_flags, OUT dx_event_subscription_param_list_t* param_list) {
	const dx_subscription_type_t subscription_type = dx_infer_subscription_type(subscr_flags, dx_st_history);

	return dx_add_subscription_param_to_list(connection, param_list, L"Series", subscription_type);
}

bool dx_get_configuration_subscription_params(dxf_connection_t connection, dx_event_subscr_flag subscr_flags, OUT dx_event_subscription_param_list_t* param_list) {
	const dx_subscription_type_t subscription_type = dx_infer_subscription_type(subscr_flags, dx_st_ticker);

	return dx_add_subscription_param_to_list(connection, param_list, L"Configuration", subscription_type);
}

/*
 * Returns the list of subscription params. Fills records list according to event_id.
 *
 * You need to call dx_free(params.elements) to free resources.
 */
size_t dx_get_event_subscription_params(dxf_connection_t connection, dx_order_source_array_ptr_t order_source, dx_event_id_t event_id,
										dx_event_subscr_flag subscr_flags, OUT dx_event_subscription_param_list_t* params) {
	bool result = true;
	dx_event_subscription_param_list_t param_list = { NULL, 0, 0 };

	switch (event_id) {
	case dx_eid_trade:
		result = dx_get_trade_subscription_params(connection, subscr_flags, &param_list);
		break;
	case dx_eid_quote:
		result = dx_get_quote_subscription_params(connection, subscr_flags, &param_list);
		break;
	case dx_eid_summary:
		result = dx_get_summary_subscription_params(connection, subscr_flags, &param_list);
		break;
	case dx_eid_profile:
		result = dx_get_profile_subscription_params(connection, subscr_flags, &param_list);
		break;
	case dx_eid_order:
		result = dx_get_order_subscription_params(connection, order_source, subscr_flags, &param_list);
		break;
	case dx_eid_time_and_sale:
		result = dx_get_time_and_sale_subscription_params(connection, subscr_flags, &param_list);
		break;
	case dx_eid_candle:
		result = dx_get_candle_subscription_params(connection, subscr_flags, &param_list);
		break;
	case dx_eid_trade_eth:
		result = dx_get_trade_eth_subscription_params(connection, subscr_flags, &param_list);
		break;
	case dx_eid_spread_order:
		result = dx_get_spread_order_subscription_params(connection, subscr_flags, &param_list);
		break;
	case dx_eid_greeks:
		result = dx_get_greeks_subscription_params(connection, subscr_flags, &param_list);
		break;
	case dx_eid_theo_price:
		result = dx_get_theo_price_subscription_params(connection, subscr_flags, &param_list);
		break;
	case dx_eid_underlying:
		result = dx_get_underlying_subscription_params(connection, subscr_flags, &param_list);
		break;
	case dx_eid_series:
		result = dx_get_series_subscription_params(connection, subscr_flags, &param_list);
		break;
	case dx_eid_configuration:
		result = dx_get_configuration_subscription_params(connection, subscr_flags, &param_list);
		break;
	}

	if (!result) {
		dx_logging_last_error();
		dx_logging_info(L"Unable to create subscription to event %d (%ls)", event_id, dx_event_type_to_string(event_id));
	}

	*params = param_list;

	return param_list.size;
}

/* -------------------------------------------------------------------------- */
/*
 *	Event data navigation
 */
/* -------------------------------------------------------------------------- */

typedef const dxf_event_data_t (*dx_event_data_navigator) (const dxf_event_data_t data, size_t index);
#define EVENT_DATA_NAVIGATOR_NAME(struct_name) \
	struct_name##_data_navigator

#define EVENT_DATA_NAVIGATOR_BODY(struct_name) \
	const dxf_event_data_t EVENT_DATA_NAVIGATOR_NAME(struct_name) (const dxf_event_data_t data, size_t index) { \
		struct_name* buffer = (struct_name*)data; \
		\
		return (const dxf_event_data_t)(buffer + index); \
	}

EVENT_DATA_NAVIGATOR_BODY(dxf_trade_t)
EVENT_DATA_NAVIGATOR_BODY(dxf_quote_t)
EVENT_DATA_NAVIGATOR_BODY(dxf_summary_t)
EVENT_DATA_NAVIGATOR_BODY(dxf_profile_t)
EVENT_DATA_NAVIGATOR_BODY(dxf_order_t)
EVENT_DATA_NAVIGATOR_BODY(dxf_time_and_sale_t)
EVENT_DATA_NAVIGATOR_BODY(dxf_candle_t)
EVENT_DATA_NAVIGATOR_BODY(dxf_greeks_t)
EVENT_DATA_NAVIGATOR_BODY(dxf_theo_price_t)
EVENT_DATA_NAVIGATOR_BODY(dxf_underlying_t)
EVENT_DATA_NAVIGATOR_BODY(dxf_series_t)
EVENT_DATA_NAVIGATOR_BODY(dxf_configuration_t)

static const dx_event_data_navigator g_event_data_navigators[dx_eid_count] = {
	EVENT_DATA_NAVIGATOR_NAME(dxf_trade_t),
	EVENT_DATA_NAVIGATOR_NAME(dxf_quote_t),
	EVENT_DATA_NAVIGATOR_NAME(dxf_summary_t),
	EVENT_DATA_NAVIGATOR_NAME(dxf_profile_t),
	EVENT_DATA_NAVIGATOR_NAME(dxf_order_t),
	EVENT_DATA_NAVIGATOR_NAME(dxf_time_and_sale_t),
	EVENT_DATA_NAVIGATOR_NAME(dxf_candle_t),
	EVENT_DATA_NAVIGATOR_NAME(dxf_trade_t), // As trade ETH
	EVENT_DATA_NAVIGATOR_NAME(dxf_order_t), // As spread order
	EVENT_DATA_NAVIGATOR_NAME(dxf_greeks_t),
	EVENT_DATA_NAVIGATOR_NAME(dxf_theo_price_t),
	EVENT_DATA_NAVIGATOR_NAME(dxf_underlying_t),
	EVENT_DATA_NAVIGATOR_NAME(dxf_series_t),
	EVENT_DATA_NAVIGATOR_NAME(dxf_configuration_t)
};

/* -------------------------------------------------------------------------- */

const dxf_event_data_t dx_get_event_data_item (int event_mask, const dxf_event_data_t data, size_t index) {
	return g_event_data_navigators[dx_get_event_id_by_bitmask(event_mask)](data, index);
}