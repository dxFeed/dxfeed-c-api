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
    sizeof(dxf_time_and_sale_t)
};

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

static const dx_event_subscription_param_t g_trade_subscription_params[] = {
    { dx_rid_trade, dx_st_ticker }
};

static const dx_event_subscription_param_t g_quote_subscription_params[] = {
    { dx_rid_quote, dx_st_ticker }
};

static const dx_event_subscription_param_t g_summary_subscription_params[] = {
    { dx_rid_fundamental, dx_st_ticker }
};

static const dx_event_subscription_param_t g_profile_subscription_params[] = {
    { dx_rid_profile, dx_st_ticker }
};

static const dx_event_subscription_param_t g_order_subscription_params[] = {
    { dx_rid_quote, dx_st_ticker },
    { dx_rid_market_maker, dx_st_history },
    { dx_rid_order, dx_st_history }
};

static const dx_event_subscription_param_t g_time_and_sale_subscription_params[] = {
    { dx_rid_time_and_sale, dx_st_stream }
};

typedef struct {
    const dx_event_subscription_param_t* params;
    int param_count;
} dx_event_subscription_param_roster;

static const dx_event_subscription_param_roster g_event_param_rosters[dx_eid_count] = {
    { g_trade_subscription_params, sizeof(g_trade_subscription_params) / sizeof(g_trade_subscription_params[0]) },
    { g_quote_subscription_params, sizeof(g_quote_subscription_params) / sizeof(g_quote_subscription_params[0]) },
    { g_summary_subscription_params, sizeof(g_summary_subscription_params) / sizeof(g_summary_subscription_params[0]) },
    { g_profile_subscription_params, sizeof(g_profile_subscription_params) / sizeof(g_profile_subscription_params[0]) },
    { g_order_subscription_params, sizeof(g_order_subscription_params) / sizeof(g_order_subscription_params[0]) },
    { g_time_and_sale_subscription_params, sizeof(g_time_and_sale_subscription_params) / sizeof(g_time_and_sale_subscription_params[0]) }
};

//int dx_get_event_subscription_params (dx_event_id_t event_id, OUT const dx_event_subscription_param_t** params) {
//
//    *params = g_event_param_rosters[event_id].params;
//    
//    return g_event_param_rosters[event_id].param_count;
//}

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

bool dx_get_order_subscription_params(dxf_connection_t connection, OUT dx_event_subscription_param_list_t* param_list) {
    dxf_char_t ch = 'A';
    dxf_const_string_t quote_tmpl = L"Quote&";
    CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, L"Quote", dx_st_ticker, );
    CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, L"MarketMaker", dx_st_history);
    CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, L"Order", dx_st_history);
    CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, L"Order#NTV", dx_st_history);

    //fill quotes Quote&A..Quote&Z
    for (; ch <= 'Z'; ch++) {
        int suffix_index = dx_string_length(quote_tmpl);
        dxf_string_t record_name = dx_create_string(suffix_index + 1);
        dx_copy_string(record_name, quote_tmpl);
        record_name[suffix_index] = ch;
        CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, record_name, dx_st_ticker);
        dx_free(record_name);
    }

    //TODO: add order subscriptions

    return true;
}

bool dx_get_trade_subscription_params(dxf_connection_t connection, OUT dx_event_subscription_param_list_t* param_list) {
    dxf_char_t ch = 'A';
    dxf_const_string_t trade_tmpl = L"Trade&";
    CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, L"Trade", dx_st_ticker);

    //fill trades Trade&A..Trade&Z
    for (; ch <= 'Z'; ch++) {
        int suffix_index = dx_string_length(trade_tmpl);
        dxf_string_t record_name = dx_create_string(suffix_index + 1);
        dx_copy_string(record_name, trade_tmpl);
        record_name[suffix_index] = ch;
        CHECKED_CALL_4(dx_add_subscription_param_to_list, connection, param_list, record_name, dx_st_ticker);
        dx_free(record_name);
    }

    return true;
}

/*
 * Returns the list of subscription params. Fills records list according to event_id.
 *
 * You need to call dx_free(params.elements) to free resources.
*/
int dx_get_event_subscription_params2(dxf_connection_t connection, dx_event_id_t event_id, 
                                      OUT dx_event_subscription_param_list_t* params) {
    bool result = true;
    dx_event_subscription_param_list_t param_list = { NULL, 0, 0 };
    switch (event_id) {
    case dx_eid_trade:
        result = dx_get_trade_subscription_params(connection, &param_list);
        break;
    case dx_eid_quote:
        result = dx_add_subscription_param_to_list(connection, &param_list, L"Quote", dx_st_ticker);
        break;
    case dx_eid_summary:
        result = dx_add_subscription_param_to_list(connection, &param_list, L"Fundamental", dx_st_ticker);
        break;
    case dx_eid_profile:
        result = dx_add_subscription_param_to_list(connection, &param_list, L"Profile", dx_st_ticker);
        break;
    case dx_eid_order:
        result = dx_get_order_subscription_params(connection, &param_list);
        break;
    case dx_eid_time_and_sale:
        result = dx_add_subscription_param_to_list(connection, &param_list, L"TimeAndSale", dx_st_stream);
        break;
    }

    if (!result) {
        dx_logging_last_error();
        dx_logging_info(L"Unable to create subscription to event %d (%s)", event_id, dx_event_type_to_string(event_id));
    }

    *params = param_list;
    return param_list.size;
}
    
/* -------------------------------------------------------------------------- */
/*
 *	Event data navigation
 */
/* -------------------------------------------------------------------------- */

typedef const dxf_event_data_t (*dx_event_data_navigator) (const dxf_event_data_t data, int index);
#define EVENT_DATA_NAVIGATOR_NAME(struct_name) \
    struct_name##_data_navigator
    
#define EVENT_DATA_NAVIGATOR_BODY(struct_name) \
    const dxf_event_data_t EVENT_DATA_NAVIGATOR_NAME(struct_name) (const dxf_event_data_t data, int index) { \
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

static const dx_event_data_navigator g_event_data_navigators[dx_eid_count] = {
    EVENT_DATA_NAVIGATOR_NAME(dxf_trade_t),
    EVENT_DATA_NAVIGATOR_NAME(dxf_quote_t),
    EVENT_DATA_NAVIGATOR_NAME(dxf_summary_t),
    EVENT_DATA_NAVIGATOR_NAME(dxf_profile_t),
    EVENT_DATA_NAVIGATOR_NAME(dxf_order_t),
    EVENT_DATA_NAVIGATOR_NAME(dxf_time_and_sale_t)
};

/* -------------------------------------------------------------------------- */

const dxf_event_data_t dx_get_event_data_item (int event_mask, const dxf_event_data_t data, int index) {
    return g_event_data_navigators[dx_get_event_id_by_bitmask(event_mask)](data, index);
}