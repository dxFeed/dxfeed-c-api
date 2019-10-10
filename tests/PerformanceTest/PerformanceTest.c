

#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "Logger.h"
#include <time.h>

#include <Windows.h>
#include "signal.h"
#include <stdio.h>

#include <inttypes.h>

#define LS(s) LS2(s)
#define LS2(s) L##s

#ifndef true
typedef int bool;

#define true 1
#define false 0
#endif

const char dxfeed_host[] = "mddqa.in.devexperts.com:7400";
//const char dxfeed_host[] = "demo.dxfeed.com:7300";

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
	case DXF_ET_CONFIGURATION: return L"Configuration";
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

/* -------------------------------------------------------------------------- */
int quotes_counter = 0;
bool doPrint = false;

void print_current_time (void) {
	SYSTEMTIME current_time;

	GetLocalTime(&current_time);
	wprintf(L"%.4u-%.2u-%.2u %.2u:%.2u:%.2u.%.3u:",
			current_time.wYear, current_time.wMonth, current_time.wDay,
			current_time.wHour, current_time.wMinute, current_time.wSecond,
			current_time.wMilliseconds);

}
void print_timestamp(dxf_long_t timestamp){
		char timefmt[80];

		struct tm * timeinfo;
		time_t tmpint = (int)(timestamp /1000);
		timeinfo = localtime ( &tmpint );
		strftime(timefmt,80,"%Y%m%d-%H%M%S" ,timeinfo);
		printf("%s",timefmt);
}

void listener(int event_type, dxf_const_string_t symbol_name,
			const dxf_event_data_t* data, int data_count, void* user_data) {
	dxf_int_t i = 0;

	++quotes_counter;

	if (!doPrint)
		return;

	wprintf(L"%ls{symbol=%ls, ",dx_event_type_to_string(event_type), symbol_name);

	if (event_type == DXF_ET_QUOTE) {
		dxf_quote_t* quotes = (dxf_quote_t*)data;

		for (; i < data_count; ++i) {
			wprintf(L"bidTime=");
			print_timestamp(quotes[i].bid_time);
			wprintf(L" bidExchangeCode=%c, bidPrice=%f, bidSize=%i, ",
					quotes[i].bid_exchange_code,
					quotes[i].bid_price,
					quotes[i].bid_size);
			wprintf(L"askTime=");
			print_timestamp(quotes[i].ask_time);
			wprintf(L" askExchangeCode=%c, askPrice=%f, askSize=%i, scope=%d}\n",
					quotes[i].ask_exchange_code,
					quotes[i].ask_price,
					quotes[i].ask_size, (int)quotes[i].scope);
		}
	}

	if (event_type == DXF_ET_ORDER){
		dxf_order_t* orders = (dxf_order_t*)data;

		for (; i < data_count; ++i) {
			wprintf(L"index=0x%"LS(PRIX64)L", side=%i, scope=%i, time=",
					orders[i].index, orders[i].side, orders[i].scope);
					print_timestamp(orders[i].time);
			wprintf(L", exchange code=%c, market maker=%ls, price=%f, size=%i}\n",
					orders[i].exchange_code, orders[i].market_maker, orders[i].price, orders[i].size);
		}
	}

	if (event_type == DXF_ET_TRADE) {
		dxf_trade_t* trades = (dxf_trade_t*)data;

		for (; i < data_count; ++i) {
			print_timestamp(trades[i].time);
			wprintf(L", exchangeCode=%c, price=%f, size=%i, tick=%i, change=%f, day volume=%.0f, scope=%d}\n",
					trades[i].exchange_code, trades[i].price, trades[i].size, trades[i].tick, trades[i].change,
				trades[i].day_volume, (int)trades[i].scope);
		}
	}

	if (event_type == DXF_ET_SUMMARY) {
		dxf_summary_t* s = (dxf_summary_t*)data;

		for (; i < data_count; ++i) {
			wprintf(L"day high price=%f, day low price=%f, day open price=%f, prev day close price=%f, open interest=%i}\n",
					s[i].day_high_price, s[i].day_low_price, s[i].day_open_price, s[i].prev_day_close_price, s[i].open_interest);
		}
	}

	if (event_type == DXF_ET_PROFILE) {
		dxf_profile_t* p = (dxf_profile_t*)data;

		for (; i < data_count ; ++i) {
			wprintf(L"Beta=%f, eps=%f, div freq=%i, exd div amount=%f, exd div date=%i, 52 high price=%f, "
				L"52 low price=%f, shares=%f, Description=%ls, flags=%i, status_reason=%ls, halt start time=",
				p[i].beta, p[i].eps, p[i].div_freq, p[i].exd_div_amount, p[i].exd_div_date, p[i]._52_high_price,
				p[i]._52_low_price, p[i].shares, p[i].description, p[i].raw_flags, p[i].status_reason);
			print_timestamp(p[i].halt_start_time);
			wprintf(L", halt end time=");
			print_timestamp(p[i].halt_end_time);
			wprintf(L", high limit price=%f, low limit price=%f}\n", p[i].high_limit_price, p[i].low_limit_price);
		}
	}

	if (event_type == DXF_ET_TIME_AND_SALE) {
		dxf_time_and_sale_t* tns = (dxf_time_and_sale_t*)data;

		for (; i < data_count ; ++i) {
			wprintf(L"event id=%"LS(PRId64)L", time=%"LS(PRId64)L", exchange code=%c, price=%f, size=%i, bid price=%f, ask price=%f, "
				L"exchange sale conditions=%ls, is ETH trade=%ls, type=%i}\n",
					tns[i].index, tns[i].time, tns[i].exchange_code, tns[i].price, tns[i].size,
					tns[i].bid_price, tns[i].ask_price, tns[i].exchange_sale_conditions,
					tns[i].is_eth_trade ? L"True" : L"False", tns[i].type);
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
			L"error code = %d, description = \"%ls\"\n",
			error_code, error_descr);
		return;
	}

	printf("An error occurred but the error subsystem failed to initialize\n");
}
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

/* -------------------------------------------------------------------------- */


int main (int argc, char* argv[]) {
	dxf_connection_t connection;
	dxf_subscription_t subscription;
	int loop_counter = 1000; // *100 = msec of running
	int i = 0;
	int arg = 1;
	int line_size = 200;
	char line[200];
	char * pch;
	char** symbols;
	int symbols_pos = 0;
	int symbols_max = 10000;
	time_t start, end;
	int diff_time;
	int cmp;
	int subscription_types = 0;

	symbols = (char**)malloc (symbols_max*sizeof(char*));
	if (argc > 1){ //we have params
		cmp = strcmp( argv[1] , "print" );
		if (!strcmp( argv[1] , "print" )){
			doPrint = true;
			++arg;
		}
		for (i = arg; i < argc ; ++i ){
			FILE *file;
			file = fopen (argv[i], "rt");
			if (file == NULL){
				printf("Couldn't open input file.");
				return -1;
			}
			while(fgets(line, line_size, file) != NULL){
				if (line[0] == '#')
					continue;
				pch = strtok (line,",");
				pch = strtok (NULL, ",");// we need a second token
				if (symbols_pos < symbols_max){
					symbols[symbols_pos] = strdup(pch);
					++symbols_pos;
				}
			}
			fclose(file);
		}
	}

	dxf_initialize_logger( "log.log", true, true, true );
	InitializeCriticalSection(&listener_thread_guard);

	printf("Sample test started.\n");
	printf("Connecting to host %s...\n", dxfeed_host);

	if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, NULL, &connection)) {
		process_last_error();
		return -1;
	}

	printf("Connection successful!\n");
	time (&start);
	if (!dxf_create_subscription(connection, DXF_ET_TRADE | DXF_ET_QUOTE | DXF_ET_ORDER | DXF_ET_SUMMARY | DXF_ET_PROFILE, &subscription)) {
		process_last_error();
		return -1;
	}

	if (symbols_pos > 0){
		for (i = 0; i < symbols_pos; ++i){
			printf("Subscribing to: %s\n",symbols[i]);
			if (!dxf_add_symbol(subscription, ansi_to_unicode(symbols[i]))) {
				process_last_error();
				return -1;
			};
		};
	};

	if (!dxf_attach_event_listener(subscription, listener, NULL)) {
		process_last_error();

		return -1;
	};
	printf("Subscription successful!\n");

	while (!is_thread_terminate() && loop_counter--) {
		Sleep(100);
	}
	DeleteCriticalSection(&listener_thread_guard);

	printf("Disconnecting from host...\n");

	if (!dxf_close_connection(connection)) {
		process_last_error();

		return -1;
	}
	time(&end);
	diff_time = (int)difftime(end, start);

	printf("Disconnect successful!\n"
			"Connection test completed successfully!\n");

	printf("received %i quotes in %i sec. %i qoutes in 1 sec\n", quotes_counter, diff_time, (int )(quotes_counter / diff_time));

	return 0;
}

