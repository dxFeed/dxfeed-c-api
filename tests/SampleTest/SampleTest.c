

#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "Logger.h"
#include <stdio.h>
#include <Windows.h>

const char dxfeed_host[] = "demo.dxfeed.com:7300";

dx_const_string_t dx_event_type_to_string (int event_type) {
	switch (event_type){
	case DXF_ET_TRADE: return L"Trade"; 
	case DXF_ET_QUOTE: return L"Quote"; 
	case DXF_ET_SUMMARY: return L"Summary"; 
	case DXF_ET_PROFILE: return L"Profile"; 
	case DXF_ET_ORDER: return L"Order"; 
	default: return L"";
	}	
}

/* -------------------------------------------------------------------------- */
static bool is_listener_thread_terminated = false;
CRITICAL_SECTION listener_thread_guard;

bool is_thread_terminate() {
    bool res;
    EnterCriticalSection(&listener_thread_guard);
    res = is_listener_thread_terminated;
    LeaveCriticalSection(&listener_thread_guard);

    return res;
}

/* -------------------------------------------------------------------------- */

void on_reader_thread_terminate(const char* host ) {
    EnterCriticalSection(&listener_thread_guard);
    is_listener_thread_terminated = true;
    LeaveCriticalSection(&listener_thread_guard);

    printf("\nTerminating listener thread, host: %s\n", host);
    dx_logging_verbose_info(L"Terminating listener thread");
}

/* -------------------------------------------------------------------------- */

void listener (int event_type, dx_const_string_t symbol_name, const dx_event_data_t* data, int data_count) {
    dx_int_t i = 0;

    wprintf(L"Event: %s Symbol: %s\n",dx_event_type_to_string(event_type), symbol_name);

    if (event_type == DXF_ET_QUOTE) {
	    dxf_quote_t* quotes = (dxf_quote_t*)data;

	    for (; i < data_count; ++i) {
		    wprintf(L"bid time=%i, bid exchange code=%C, bid price=%f, bid size=%i; "
		            L"ask time=%i, ask exchange code=%C, ask price=%f, ask size=%i\n",
		            quotes[i].bid_time, quotes[i].bid_exchange_code, quotes[i].bid_price, quotes[i].bid_size,
		            quotes[i].ask_time, quotes[i].ask_exchange_code, quotes[i].ask_price, quotes[i].ask_size);
		}
    }
    
    if (event_type == DXF_ET_ORDER){
	    dxf_order_t* orders = (dxf_order_t*)data;

	    for (; i < data_count; ++i) {
		    wprintf(L"index=%i, side=%i, level=%i, time=%i, exchange code=%C, market maker=%s, price=%f, size=%i\n",
		            orders[i].index, orders[i].side, orders[i].level, orders[i].time,
		            orders[i].exchange_code, orders[i].market_maker, orders[i].price, orders[i].size);
		}
    }
    
    if (event_type == DXF_ET_TRADE) {
	    dxf_trade_t* trades = (dx_trade_t*)data;

	    for (; i < data_count; ++i) {
		    wprintf(L"time=%i, exchange code=%C, price=%f, size=%i, day volume=%f\n",
		            trades[i].time, trades[i].exchange_code, trades[i].price, trades[i].size, trades[i].day_volume);
		}
    }
    
    if (event_type == DXF_ET_SUMMARY) {
	    dxf_summary_t* s = (dxf_summary_t*)data;

	    for (; i < data_count; ++i) {
		    wprintf(L"day high price=%f, day low price=%f, day open price=%f, prev day close price=%f, open interest=%i\n",
		            s[i].day_high_price, s[i].day_low_price, s[i].day_open_price, s[i].prev_day_close_price, s[i].open_interest);
		}
    }
    
    if (event_type == DXF_ET_PROFILE){
	    dxf_profile_t* p = (dxf_profile_t*)data;

	    for (; i < data_count ; ++i) {
		    wprintf(L"Description=%s\n",
				    p[i].description);
	    }
    }	
}
/* -------------------------------------------------------------------------- */

void process_last_error () {
    int subsystem_id = dx_sc_invalid_subsystem;
    int error_code = DX_INVALID_ERROR_CODE;
    const char* error_descr = NULL;
    int res;
    
    res = dxf_get_last_error(&subsystem_id, &error_code, &error_descr);
    
    if (res == DXF_SUCCESS) {
        if (subsystem_id == dx_sc_invalid_subsystem && error_code == DX_INVALID_ERROR_CODE) {
            printf("WTF - no error information is stored");
            
            return;
        }
        
        printf("Error occurred and successfully retrieved:\n"
               "subsystem code = %d, error code = %d, description = \"%s\"\n",
               subsystem_id, error_code, error_descr);
        return;
    }
    
    printf("An error occurred but the error subsystem failed to initialize\n");
}

/* -------------------------------------------------------------------------- */

int main (int argc, char* argv[]) {
    dxf_subscription_t subscription;
    int loop_counter = 10000;

    dx_logger_initialize( "log.log", true, true, true );

	printf("Sample test started.\n");    
    printf("Connecting to host %s...\n", dxfeed_host);
    
    if (!dxf_connect_feed(dxfeed_host, on_reader_thread_terminate)) {
        process_last_error();
        return -1;
    }

    printf("Connection successful!\n");
 
	if (!dxf_create_subscription(DXF_ET_TRADE | DXF_ET_QUOTE | DXF_ET_ORDER | DXF_ET_SUMMARY | DXF_ET_PROFILE, &subscription )) {
        process_last_error();
        
        return -1;
    };
	
	if (!dxf_add_symbol(subscription, L"IBM")) {
        process_last_error();
        
        return -1;
    }; 

	if (!dxf_attach_event_listener(subscription, listener)) {
        process_last_error();
        
        return -1;
    };
    printf("Subscription successful!\n");

    InitializeCriticalSection(&listener_thread_guard);
    while (!is_thread_terminate() && loop_counter--) {
        Sleep(100);
    }
    DeleteCriticalSection(&listener_thread_guard);

    printf("Disconnecting from host...\n");
    
    if (!dxf_disconnect_feed()) {
        process_last_error();
        
        return -1;
    }
    
    printf("Disconnect successful!\n"
           "Connection test completed successfully!\n");
           
    return 0;
}

