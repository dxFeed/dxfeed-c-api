#include "DXFeed.h"

#include "EventSubscriptionTest.h"
#include "EventSubscription.h"
#include "SymbolCodec.h"
#include "BufferedIOCommon.h"
#include "DXAlgorithms.h"
#include "ConnectionContextData.h"

static int last_event_type = 0;
static dxf_const_string_t last_symbol = NULL;
static int visit_count = 0;

void dummy_listener(int event_type, dxf_const_string_t symbol_name,
                    const dxf_event_data_t* data, int data_count, void* user_data) {
    ++visit_count;
    last_event_type = event_type;
    last_symbol = symbol_name;
}

bool event_subscription_test (void) {
    dxf_connection_t connection;
    dxf_subscription_t sub1;
    dxf_subscription_t sub2;
    dxf_const_string_t large_symbol_set[] = { L"SYMA", L"SYMB", L"SYMC" };
    dxf_const_string_t middle_symbol_set[] = { L"SYMB", L"SYMD" };
    dxf_const_string_t small_symbol_set[] = { L"SYMB" };
    dxf_int_t symbol_code;
    dxf_event_params_t empty_event_params = { 0, 0, 0 };
    dxf_quote_t quote_data = { 0, 'A', 1.0, 1, 0, 'A', 2.0, 1 };
    const dxf_event_data_t quote_event_data = &quote_data;
    dxf_trade_t trade_data = { 0, 'A', 1.0, 1, 1, 1.0, 1.0 };
    const dxf_event_data_t trade_event_data = &trade_data;
    
    if (dx_init_symbol_codec() != true) {
        return false;
    }
    
    connection = dx_init_connection();
    
    sub1 = dx_create_event_subscription(connection, DXF_ET_TRADE | DXF_ET_QUOTE, 0, 0);
    sub2 = dx_create_event_subscription(connection, DXF_ET_QUOTE, 0, 0);
    
    if (sub1 == dx_invalid_subscription || sub2 == dx_invalid_subscription) {
        return false;
    }
    
    if (!dx_add_symbols(sub1, large_symbol_set, 3)) {
        return false;
    }
    
    if (!dx_add_symbols(sub2, middle_symbol_set, 2)) {
        return false;
    }
    
    // sub1 - SYMA, SYMB, SYMC; QUOTE | TRADE
    // sub2 - SYMB, SYMD; QUOTE
    
    if (!dx_add_listener(sub1, dummy_listener, NULL)) {
        return false;
    }
    
    if (!dx_add_listener(sub2, dummy_listener, NULL)) {
        return false;
    }
    
    // testing the symbol retrieval
    {
        dxf_const_string_t* symbols = NULL;
        size_t symbol_count = 0;
        size_t i = 0;
        
        if (!dx_get_event_subscription_symbols(sub2, &symbols, &symbol_count)) {
            return false;
        }
        
        if (symbol_count != 2) {
            return false;
        }
        
        for (; i < symbol_count; ++i) {
            if (dx_compare_strings(symbols[i], middle_symbol_set[i])) {
                return false;
            }
        }
    }
    
    symbol_code = dx_encode_symbol_name(L"SYMB");
    
    if (!dx_process_event_data(connection, dx_eid_quote, L"SYMB", symbol_code, quote_event_data, 1, &empty_event_params)) {
        return false;
    }
    
    // both sub1 and sub2 should receive the data
    
    if (last_event_type != DXF_ET_QUOTE || dx_compare_strings(last_symbol, L"SYMB") || visit_count != 2) {
        return false;
    }
    
    symbol_code = dx_encode_symbol_name(L"SYMZ");
    
    // unknown symbol SYMZ must be rejected
    
    if (!dx_process_event_data(connection, dx_eid_trade, L"SYMZ", symbol_code, trade_event_data, 1, &empty_event_params) ||
        dx_compare_strings(last_symbol, L"SYMB") || 
        visit_count > 2) {
        return false;
    }
    
    symbol_code = dx_encode_symbol_name(L"SYMD");

    if (!dx_process_event_data(connection, dx_eid_trade, L"SYMD", symbol_code, trade_event_data, 1, &empty_event_params)) {
        return false;
    }
    
    // SYMD is a known symbol to sub2, but sub2 doesn't support TRADEs
    
    if (last_event_type != DXF_ET_QUOTE || dx_compare_strings(last_symbol, L"SYMB") || visit_count != 2) {
        return false;
    }
    
    if (!dx_remove_symbols(sub1, small_symbol_set, 1)) {
        return false;
    }
    
    // sub1 is no longer receiving SYMBs
    
    symbol_code = dx_encode_symbol_name(L"SYMB");
    
    if (!dx_process_event_data(connection, dx_eid_quote, L"SYMB", symbol_code, quote_event_data, 1, &empty_event_params)) {
        return false;
    }
    
    // ... but sub2 still does
    
    if (last_event_type != DXF_ET_QUOTE || dx_compare_strings(last_symbol, L"SYMB") || visit_count != 3) {
        return false;
    }
    
    symbol_code = dx_encode_symbol_name(L"SYMA");
    
    if (!dx_process_event_data(connection, dx_eid_trade, L"SYMA", symbol_code, trade_event_data, 1, &empty_event_params)) {
        return false;
    }
    
    // SYMA must be processed by sub1
    
    if (last_event_type != DXF_ET_TRADE || dx_compare_strings(last_symbol, L"SYMA") || visit_count != 4) {
        return false;
    }
    
    if (!dx_remove_listener(sub2, dummy_listener)) {
        return false;
    }
    
    symbol_code = dx_encode_symbol_name(L"SYMB");

    // SYMB is still supported by sub2, but sub2 no longer has a listener
    
    if (!dx_process_event_data(connection, dx_eid_quote, L"SYMB", symbol_code, quote_event_data, 1, &empty_event_params)) {
        return false;
    }
    
    if (last_event_type != DXF_ET_TRADE || dx_compare_strings(last_symbol, L"SYMA") || visit_count != 4) {
        return false;
    }
    
    if (!dx_close_event_subscription(sub1)) {
        return false;
    }
    
    if (!dx_close_event_subscription(sub2)) {
        return false;
    }
    
    return true;
}