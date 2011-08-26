

#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "Logger.h"
#include <stdio.h>
#include <Windows.h>
#include "DXAlgorithms.h"
#include <time.h>

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

void on_reader_thread_terminate(const char* host, void* user_data) {
    EnterCriticalSection(&listener_thread_guard);
    is_listener_thread_terminated = true;
    LeaveCriticalSection(&listener_thread_guard);

    printf("\nTerminating listener thread, host: %s\n", host);
}

void print_timestamp(dxf_long_t timestamp){
		char timefmt[80];
		
		struct tm * timeinfo;
		int tmpint = (int)(timestamp /1000);
		timeinfo = localtime ( &tmpint );
		strftime(timefmt,80,"%Y%m%d-%H%M%S" ,timeinfo);
		printf("%s",timefmt);
}
/* -------------------------------------------------------------------------- */

void listener (int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count, void* user_data) {
    dxf_int_t i = 0;

	wprintf(L"%s{symbol=%s, ",dx_event_type_to_string(event_type), symbol_name);
	
    if (event_type == DXF_ET_QUOTE) {
	    dxf_quote_t* quotes = (dxf_quote_t*)data;

	    for (; i < data_count; ++i) {
			wprintf(L"bidTime=");
			print_timestamp(quotes[i].bid_time);
			wprintf(L" bidExchangeCode=%c, bidPrice=%f, bidSize=%I64i, ",
					quotes[i].bid_exchange_code, 
					quotes[i].bid_price,
					quotes[i].bid_size);
			wprintf(L"askTime=");
			print_timestamp(quotes[i].ask_time);
			wprintf(L" askExchangeCode=%c, askPrice=%f, askSize=%I64i}\n",
					quotes[i].ask_exchange_code, 
					quotes[i].ask_price,
					quotes[i].ask_size);
		}
    }
    
    if (event_type == DXF_ET_ORDER){
	    dxf_order_t* orders = (dxf_order_t*)data;

	    for (; i < data_count; ++i) {
		    wprintf(L"index=0x%I64X, side=%i, level=%i, time=",
		            orders[i].index, orders[i].side, orders[i].level);
					print_timestamp(orders[i].time);
			wprintf(L", exchange code=%c, market maker=%s, price=%f, size=%I64i}\n",
		            orders[i].exchange_code, orders[i].market_maker, orders[i].price, orders[i].size);
		}
    }
    
    if (event_type == DXF_ET_TRADE) {
	    dxf_trade_t* trades = (dx_trade_t*)data;

		for (; i < data_count; ++i) {
			print_timestamp(trades[i].time);
			wprintf(L", exchangeCode=%c, price=%f, size=%I64i, day volume=%.0f}\n",
		            trades[i].exchange_code, trades[i].price, trades[i].size, trades[i].day_volume);
		}
    }
    
    if (event_type == DXF_ET_SUMMARY) {
	    dxf_summary_t* s = (dxf_summary_t*)data;

	    for (; i < data_count; ++i) {
			wprintf(L"day high price=%f, day low price=%f, day open price=%f, prev day close price=%f, open interest=%I64i}\n",
		            s[i].day_high_price, s[i].day_low_price, s[i].day_open_price, s[i].prev_day_close_price, s[i].open_interest);
		}
    }
    
    if (event_type == DXF_ET_PROFILE) {
	    dxf_profile_t* p = (dxf_profile_t*)data;

	    for (; i < data_count ; ++i) {
			wprintf(L"Description=%s}\n",
				    p[i].description);
	    }
    }
    
    if (event_type == DXF_ET_TIME_AND_SALE) {
        dxf_time_and_sale_t* tns = (dxf_time_and_sale_t*)data;

        for (; i < data_count ; ++i) {
            wprintf(L"event id=%I64i, time=%I64i, exchange code=%c, price=%f, size=%I64i, bid price=%f, ask price=%f, "
				L"exchange sale conditions=\'%s\', is trade=%s, type=%i}\n",
                    tns[i].event_id, tns[i].time, tns[i].exchange_code, tns[i].price, tns[i].size,
                    tns[i].bid_price, tns[i].ask_price, tns[i].exchange_sale_conditions,
                    tns[i].is_trade ? L"True" : L"False", tns[i].type);
        }
    }
}
/* -------------------------------------------------------------------------- */

void process_last_error () {
    int error_code = dx_ec_success;
    dxf_const_string_t error_descr = NULL;
    int res;

    res = dxf_get_last_error(&error_code, &error_descr);

    if (res == DXF_SUCCESS) {
        if (error_code == dx_ec_success) {
            printf("WTF - no error information is stored");

            return;
        }

        wprintf(L"Error occurred and successfully retrieved:\n"
            L"error code = %d, description = \"%s\"\n",
            error_code, error_descr);
        return;
    }

    printf("An error occurred but the error subsystem failed to initialize\n");
}

/* -------------------------------------------------------------------------- */
dxf_string_t ansi_to_unicode (const char* ansi_str) {
#ifdef _WIN32
    size_t len = strlen(ansi_str);
    dxf_string_t wide_str = NULL;

    // get required size
    int wide_size = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, ansi_str, (int)len, wide_str, 0);

    if (wide_size > 0) {
        wide_str = calloc(wide_size + 1, sizeof(dxf_char_t));
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, ansi_str, (int)len, wide_str, wide_size);
    }

    return wide_str;
#else /* _WIN32 */
    return NULL; /* todo */
#endif /* _WIN32 */
}

int main (int argc, char* argv[]) {
    dxf_connection_t connection;
    dxf_subscription_t subscription;
    int loop_counter = 100000;
    char* event_type_name = NULL;
    int event_type;
    dxf_string_t symbol = NULL;
    char* dxfeed_host = NULL;


    if ( argc < 4 ) {
        printf("DXFeed command line sample.\n"
               "Usage: CommandLineSample <server address> <event type> <symbol>\n"
               "  <server address> - a DXFeed server address, e.g. demo.dxfeed.com:7300\n"
               "  <event type> - an event type, one of the following: TRADE, QUOTE, SUMMARY,\n"
               "                 PROFILE, ORDER, TIME_AND_SALE\n"
               "  <symbol> - a trade symbol, e.g. C, MSFT, YHOO, IBM\n");
        
        return 0;
    }

    dxf_initialize_logger( "log.log", true, true, true );

    dxfeed_host = argv[1];

    event_type_name = argv[2];
    if (stricmp(event_type_name, "TRADE") == 0) {
        event_type = DXF_ET_TRADE;
    } else if (stricmp(event_type_name, "QUOTE") == 0) {
        event_type = DXF_ET_QUOTE;
    } else if (stricmp(event_type_name, "SUMMARY") == 0) {
        event_type = DXF_ET_SUMMARY;
    } else if (stricmp(event_type_name, "PROFILE") == 0) {
        event_type = DXF_ET_PROFILE;
    } else if (stricmp(event_type_name, "ORDER") == 0) {
        event_type = DXF_ET_ORDER;
    } else if (stricmp(event_type_name, "TIME_AND_SALE") == 0) {
        event_type = DXF_ET_TIME_AND_SALE;
    } else {
        printf("Unknown event type.\n");
        return -1;
    }

    symbol = ansi_to_unicode(argv[3]);

    if (symbol == NULL) {
        return -1;
    } else {
        int i = 0;
        for (; symbol[i]; i++)
            symbol[i] = towupper(symbol[i]);
    }

    printf("Sample test started.\n");    
    printf("Connecting to host %s...\n", dxfeed_host);

    InitializeCriticalSection(&listener_thread_guard);

    if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, &connection)) {
        process_last_error();
        return -1;
    }

    printf("Connection successful!\n");

    if (!dxf_create_subscription(connection, event_type, &subscription)) {
        process_last_error();

        return -1;
    };

    if (!dxf_add_symbol(subscription, symbol)) {
        process_last_error();

        return -1;
    }; 

    if (!dxf_attach_event_listener(subscription, listener, NULL)) {
        process_last_error();

        return -1;
    };
    printf("Subscription successful!\n");

    while (!is_thread_terminate() && loop_counter--) {
        Sleep(100);
    }
    
	printf("Disconnecting from host...\n");

    if (!dxf_close_connection(connection)) {
        process_last_error();

        return -1;
    }

    printf("Disconnect successful!\n"
        "Connection test completed successfully!\n");

    DeleteCriticalSection(&listener_thread_guard);
    
    return 0;
}

