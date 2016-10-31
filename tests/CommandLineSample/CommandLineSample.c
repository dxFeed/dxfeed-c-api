
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <string.h>
#include <wctype.h>
#include <stdlib.h>
#define stricmp strcasecmp
#endif

#include "DXFeed.h"
#include "DXErrorCodes.h"
#include <stdio.h>
#include <time.h>

typedef int bool;

#define true 1
#define false 0

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
    case DXF_ET_THEO_PRICE: return L"THEO_PRICE";
    case DXF_ET_UNDERLYING: return L"Underlying";
    case DXF_ET_SERIES: return L"Series";
    default: return L"";
    }
}

/* -------------------------------------------------------------------------- */
#ifdef _WIN32
static bool is_listener_thread_terminated = false;
CRITICAL_SECTION listener_thread_guard;

bool is_thread_terminate() {
    bool res;
    EnterCriticalSection(&listener_thread_guard);
    res = is_listener_thread_terminated;
    LeaveCriticalSection(&listener_thread_guard);

    return res;
}
#else
static volatile bool is_listener_thread_terminated = false;
bool is_thread_terminate() {
    bool res;
    res = is_listener_thread_terminated;
    return res;
}
#endif


/* -------------------------------------------------------------------------- */

#ifdef _WIN32
void on_reader_thread_terminate(dxf_connection_t connection, void* user_data) {
    EnterCriticalSection(&listener_thread_guard);
    is_listener_thread_terminated = true;
    LeaveCriticalSection(&listener_thread_guard);

    wprintf(L"\nTerminating listener thread\n");
}
#else
void on_reader_thread_terminate(dxf_connection_t connection, void* user_data) {
    is_listener_thread_terminated = true;
    wprintf(L"\nTerminating listener thread\n");
}
#endif

void print_timestamp(dxf_long_t timestamp){
		wchar_t timefmt[80];
		
		struct tm * timeinfo;
		time_t tmpint = (time_t)(timestamp /1000);
		timeinfo = localtime ( &tmpint );
		wcsftime(timefmt,80, L"%Y%m%d-%H%M%S" ,timeinfo);
		wprintf(L"%ls",timefmt);
}
/* -------------------------------------------------------------------------- */

void listener(int event_type, dxf_const_string_t symbol_name,
              const dxf_event_data_t* data, int data_count, void* user_data) {
    dxf_int_t i = 0;

    wprintf(L"%ls{symbol=%ls, ", dx_event_type_to_string(event_type), symbol_name);
	
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
		    wprintf(L"index=0x%llX, side=%i, level=%i, time=",
		            orders[i].index, orders[i].side, orders[i].level);
					print_timestamp(orders[i].time);
			wprintf(L", exchange code=%c, market maker=%ls, price=%f, size=%lld",
		            orders[i].exchange_code, orders[i].market_maker, orders[i].price, orders[i].size);
            if (wcslen(orders[i].source) > 0)
                wprintf(L", source=%ls", orders[i].source);
            wprintf(L", count=%d}\n", orders[i].count);
		}
    }
    
    if (event_type == DXF_ET_TRADE) {
        dxf_trade_t* trades = (dxf_trade_t*)data;

        for (; i < data_count; ++i) {
            print_timestamp(trades[i].time);
            wprintf(L", exchangeCode=%c, price=%f, size=%I64i, tick=%I64i, change=%f, day volume=%.0f}\n",
                    trades[i].exchange_code, trades[i].price, trades[i].size, trades[i].tick, trades[i].change, trades[i].day_volume);
        }
    }
    
    if (event_type == DXF_ET_SUMMARY) {
	    dxf_summary_t* s = (dxf_summary_t*)data;

	    for (; i < data_count; ++i) {
			wprintf(L"day id=%d, day open price=%f, day high price=%f, day low price=%f, day close price=%f, "
                L"prev day id=%d, prev day close price=%f, open interest=%I64i, flags=%I64i, exchange=%c}\n", 
                s[i].day_id, s[i].day_open_price, s[i].day_high_price, s[i].day_low_price, s[i].day_close_price, 
                s[i].prev_day_id, s[i].prev_day_close_price, s[i].open_interest, s[i].flags, s[i].exchange_code);
        }
    }

    if (event_type == DXF_ET_PROFILE) {
        dxf_profile_t* p = (dxf_profile_t*)data;

        for (; i < data_count; ++i) {
            wprintf(L"Beta=%f, eps=%f, div freq=%I64i, exd div amount=%f, exd div date=%i, 52 high price=%f, "
                L"52 low price=%f, shares=%f, Description=%ls, flags=%I64i, status_reason=%ls, halt start time=",
                p[i].beta, p[i].eps, p[i].div_freq, p[i].exd_div_amount, p[i].exd_div_date, p[i]._52_high_price,
                p[i]._52_low_price, p[i].shares, p[i].description, p[i].flags, p[i].status_reason);
            print_timestamp(p[i].halt_start_time);
            wprintf(L", halt end time=");
            print_timestamp(p[i].halt_end_time);
            wprintf(L", high limit price=%f, low limit price=%f}\n", p[i].high_limit_price, p[i].low_limit_price);
        }
    }
    
    if (event_type == DXF_ET_TIME_AND_SALE) {
        dxf_time_and_sale_t* tns = (dxf_time_and_sale_t*)data;

        for (; i < data_count; ++i) {
            wprintf(L"event id=%I64i, time=%I64i, exchange code=%c, price=%f, size=%I64i, bid price=%f, ask price=%f, "
				L"exchange sale conditions=\'%ls\', is trade=%ls, type=%i}\n",
                    tns[i].event_id, tns[i].time, tns[i].exchange_code, tns[i].price, tns[i].size,
                    tns[i].bid_price, tns[i].ask_price, tns[i].exchange_sale_conditions,
                    tns[i].is_trade ? L"True" : L"False", tns[i].type);
        }
    }

    if (event_type == DXF_ET_TRADE_ETH) {
        dxf_trade_eth_t* trades = (dxf_trade_eth_t*)data;

        for (; i < data_count; ++i) {
            print_timestamp(trades[i].time);
            wprintf(L", exchangeCode=%c, flags=%d, price=%f, size=%I64i, day volume=%.0f}\n",
                trades[i].exchange_code, trades[i].flags, trades[i].price, trades[i].size, trades[i].eth_volume);
        }
    }

    if (event_type == DXF_ET_SPREAD_ORDER) {
        dxf_spread_order_t* orders = (dxf_spread_order_t*)data;

        for (; i < data_count; ++i) {
            wprintf(L"index=0x%llX, side=%i, level=%i, time=",
                orders[i].index, orders[i].side, orders[i].level);
            print_timestamp(orders[i].time);
            wprintf(L", sequence=%i, exchange code=%c, price=%f, size=%lld, source=%ls, "
                L"count=%i, flags=%i, spread symbol=%ls}\n",
                orders[i].sequence, orders[i].exchange_code, orders[i].price, orders[i].size, 
                wcslen(orders[i].source) > 0 ? orders[i].source : L"", 
                orders[i].count, orders[i].event_flags, 
                wcslen(orders[i].spread_symbol) > 0 ? orders[i].spread_symbol : L"");
        }
    }

    if (event_type == DXF_ET_GREEKS) {
        dxf_greeks_t* grks = (dxf_greeks_t*)data;

        for (; i < data_count; ++i) {
            wprintf(L"time=%I64i, sequence=%d, greeks price=%f, volatility=%f, "
                L"delta=%f, gamma=%f, theta=%f, rho=%f, vega=%f}\n", 
                grks[i].time, grks[i].sequence, grks[i].greeks_price, grks[i].volatility,
                grks[i].delta, grks[i].gamma, grks[i].theta, grks[i].rho, grks[i].vega);
        }
    }

    if (event_type == DXF_ET_THEO_PRICE) {
        dxf_theo_price_t* tp = (dxf_theo_price_t*)data;

        for (; i < data_count; ++i) {
            wprintf(L"theo time=%I64i, theo price=%f, theo underlying price=%f, theo delta=%f, "
                L"theo gamma=%f, theo dividend=%f, theo_interest=%f",
                tp[i].theo_time, tp[i].theo_price, tp[i].theo_underlying_price, tp[i].theo_delta,
                tp[i].theo_gamma, tp[i].theo_dividend, tp[i].theo_interest);
        }
    }

    if (event_type == DXF_ET_UNDERLYING) {
        dxf_underlying_t* u = (dxf_underlying_t*)data;

        for (; i < data_count; ++i) {
            wprintf(L"volatility=%f, front volatility=%f, back volatility=%f, put call ratio=%f}\n",
                u[i].volatility, u[i].front_volatility, u[i].back_volatility, u[i].put_call_ratio);
        }
    }

    if (event_type == DXF_ET_SERIES) {
        dxf_series_t* srs = (dxf_series_t*)data;

        for (; i < data_count; ++i) {
            wprintf(L"expiration=%d, sequence=%d, volatility=%f, put call ratio=%f, forward_price=%f, dividend=%f, interest=%f}\n",
                srs[i].expiration, srs[i].sequence, srs[i].volatility, srs[i].put_call_ratio, 
                srs[i].forward_price, srs[i].dividend, srs[i].interest);
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
            wprintf(L"WTF - no error information is stored");
            return;
        }

        wprintf(L"Error occurred and successfully retrieved:\n"
            L"error code = %d, description = \"%ls\"\n",
            error_code, error_descr);
        return;
    }

    wprintf(L"An error occurred but the error subsystem failed to initialize\n");
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
    dxf_string_t wide_str = NULL;
    size_t wide_size = mbstowcs(NULL, ansi_str, 0);
    if (wide_size > 0) {
        wide_str = calloc(wide_size + 1, sizeof(dxf_char_t));
        mbstowcs(wide_str, ansi_str, wide_size + 1);
    }

    return wide_str; /* todo */
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
	dxf_string_t dxfeed_host_u = NULL;


    if ( argc < 4 ) {
        wprintf(L"DXFeed command line sample.\n"
                L"Usage: CommandLineSample <server address> <event type> <symbol>\n"
                L"  <server address> - a DXFeed server address, e.g. demo.dxfeed.com:7300\n"
                L"  <event type> - an event type, one of the following: TRADE, QUOTE, SUMMARY,\n"
                L"                 PROFILE, ORDER, TIME_AND_SALE, TRADE_ETH, SPREAD_ORDER\n"
                L"                 GREEKS, THEO_PRICE, UNDERLYING, SERIES\n"
                L"  <symbol> - a trade symbol, e.g. C, MSFT, YHOO, IBM\n");
        
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
    } else if (stricmp(event_type_name, "TRADE_ETH") == 0) {
        event_type = DXF_ET_TRADE_ETH;
    } else if (stricmp(event_type_name, "SPREAD_ORDER") == 0) {
        event_type = DXF_ET_SPREAD_ORDER;
    } else if (stricmp(event_type_name, "GREEKS") == 0) {
        event_type = DXF_ET_GREEKS;
    } else if (stricmp(event_type_name, "THEO_PRICE") == 0) {
        event_type = DXF_ET_THEO_PRICE;
    } else if (stricmp(event_type_name, "UNDERLYING") == 0) {
        event_type = DXF_ET_UNDERLYING;
    } else if (stricmp(event_type_name, "SERIES") == 0) {
        event_type = DXF_ET_SERIES;
    } else {
        wprintf(L"Unknown event type.\n");
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

    wprintf(L"Sample test started.\n");
	dxfeed_host_u = ansi_to_unicode(dxfeed_host);
    wprintf(L"Connecting to host %ls...\n", dxfeed_host_u);
	free(dxfeed_host_u);

#ifdef _WIN32
    InitializeCriticalSection(&listener_thread_guard);
#endif

    if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, &connection)) {
        process_last_error();
        return -1;
    }

    wprintf(L"Connection successful!\n");

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
    wprintf(L"Subscription successful!\n");

    while (!is_thread_terminate() && loop_counter--) {
#ifdef _WIN32
        Sleep(100);
#else
		sleep(1);
#endif
    }
    
    wprintf(L"Disconnecting from host...\n");

    if (!dxf_close_connection(connection)) {
        process_last_error();

        return -1;
    }

    wprintf(L"Disconnect successful!\nConnection test completed successfully!\n");
    wprintf(L"loops remain:%d\n", loop_counter);

#ifdef _WIN32
    DeleteCriticalSection(&listener_thread_guard);
#endif
    
    return 0;
}

