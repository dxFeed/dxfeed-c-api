
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
#include <inttypes.h>

#define LS(s) LS2(s)
#define LS2(s) L##s

#ifndef true
typedef int bool;

#define true 1
#define false 0
#endif

// plus the name of the executable
#define STATIC_PARAMS_COUNT 4
#define DUMP_PARAM_SHORT_TAG "-d"
#define DUMP_PARAM_LONG_TAG "--dump"
#define TOKEN_PARAM_SHORT_TAG "-T"
#define SUBSCRIPTION_DATA_PARAM_TAG "-s"

dxf_const_string_t dx_event_type_to_string(int event_type) {
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

//Prevents file names globbing (converting * to all files in the current dir)
#ifdef __MINGW64_VERSION_MAJOR
int _CRT_glob = 0;
#endif

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

void process_last_error() {
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

#ifdef _WIN32
void on_reader_thread_terminate(dxf_connection_t connection, void* user_data) {
	EnterCriticalSection(&listener_thread_guard);
	is_listener_thread_terminated = true;
	LeaveCriticalSection(&listener_thread_guard);

	wprintf(L"\nTerminating listener thread\n");
	process_last_error();
}
#else
void on_reader_thread_terminate(dxf_connection_t connection, void* user_data) {
	is_listener_thread_terminated = true;
	wprintf(L"\nTerminating listener thread\n");
	process_last_error();
}
#endif

// The example of processing a connection status change
void on_connection_status_changed(dxf_connection_t connection, dxf_connection_status_t old_status,
		dxf_connection_status_t new_status, void *user_data) {
	switch (new_status) {
		case dxf_cs_connected:
			wprintf(L"Connected!\n");
			break;
		case dxf_cs_authorized:
			wprintf(L"Authorized!\n");
			break;
		default:;
	}
}

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
			wprintf(L"index=0x%llX, side=%i, scope=%i, time=",
					orders[i].index, orders[i].side, orders[i].scope);
					print_timestamp(orders[i].time);
			wprintf(L", exchange code=%c, market maker=%ls, price=%f, size=%d",
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
			wprintf(L", exchangeCode=%c, price=%f, size=%i, tick=%i, change=%f, day volume=%.0f, scope=%d}\n",
					trades[i].exchange_code, trades[i].price, trades[i].size, trades[i].tick, trades[i].change,
				trades[i].day_volume, (int)trades[i].scope);
		}
	}

	if (event_type == DXF_ET_SUMMARY) {
		dxf_summary_t* s = (dxf_summary_t*)data;

		for (; i < data_count; ++i) {
			wprintf(L"day id=%d, day open price=%f, day high price=%f, day low price=%f, day close price=%f, "
				L"prev day id=%d, prev day close price=%f, open interest=%i, flags=0x%X, exchange=%c, "
				L"day close price type=%i, prev day close price type=%i, scope=%d}\n",
				s[i].day_id, s[i].day_open_price, s[i].day_high_price, s[i].day_low_price, s[i].day_close_price,
				s[i].prev_day_id, s[i].prev_day_close_price, s[i].open_interest, s[i].raw_flags, s[i].exchange_code,
				s[i].day_close_price_type, s[i].prev_day_close_price_type, s[i].scope);
		}
	}

	if (event_type == DXF_ET_PROFILE) {
		dxf_profile_t* p = (dxf_profile_t*)data;

		for (; i < data_count; ++i) {
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

		for (; i < data_count; ++i) {
			wprintf(L"event id=%"LS(PRId64)L", time=", tns[i].index);
			print_timestamp(tns[i].time);
			wprintf(L", exchange code=%c, price=%f, size=%i, bid price=%f, ask price=%f, "
				L"exchange sale conditions=\'%ls\', is ETH trade=%ls, type=%i, buyer=\'%ls\', seller=\'%ls\', scope=%d}\n",
				tns[i].exchange_code, tns[i].price, tns[i].size,
				tns[i].bid_price, tns[i].ask_price, tns[i].exchange_sale_conditions,
				tns[i].is_eth_trade ? L"True" : L"False", tns[i].type,
				tns[i].buyer ? tns[i].buyer : L"<UNKNOWN>", 
				tns[i].seller ? tns[i].seller : L"<UNKNOWN>",
				tns[i].scope);
		}
	}

	if (event_type == DXF_ET_TRADE_ETH) {
		dxf_trade_t* trades = (dxf_trade_t*)data;

		for (; i < data_count; ++i) {
			print_timestamp(trades[i].time);
			wprintf(L", exchangeCode=%c, flags=%d, price=%f, size=%i, day volume=%.0f, scope=%d}\n",
				trades[i].exchange_code, trades[i].raw_flags, trades[i].price, trades[i].size, trades[i].day_volume,
				(int)trades[i].scope);
		}
	}

	if (event_type == DXF_ET_SPREAD_ORDER) {
		dxf_order_t* orders = (dxf_order_t*)data;

		for (; i < data_count; ++i) {
			wprintf(L"index=0x%llX, side=%i, scope=%i, time=",
				orders[i].index, orders[i].side, orders[i].scope);
			print_timestamp(orders[i].time);
			wprintf(L", sequence=%i, exchange code=%c, price=%f, size=%d, source=%ls, "
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
			wprintf(L"time=");
			print_timestamp(grks[i].time);
			wprintf(L", index=%"LS(PRId64)L", greeks price=%f, volatility=%f, "
				L"delta=%f, gamma=%f, theta=%f, rho=%f, vega=%f, index=0x%"LS(PRIX64)L"}\n",
				grks[i].index, grks[i].price, grks[i].volatility,
				grks[i].delta, grks[i].gamma, grks[i].theta, grks[i].rho, grks[i].vega, grks[i].index);
		}
	}

	if (event_type == DXF_ET_THEO_PRICE) {
		dxf_theo_price_t* tp = (dxf_theo_price_t*)data;

		for (; i < data_count; ++i) {
			wprintf(L"theo time=");
			print_timestamp(tp[i].time);
			wprintf(L", theo price=%f, theo underlying price=%f, theo delta=%f, "
				L"theo gamma=%f, theo dividend=%f, theo_interest=%f}\n",
				tp[i].price, tp[i].underlying_price, tp[i].delta,
				tp[i].gamma, tp[i].dividend, tp[i].interest);
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
			wprintf(L"expiration=%d, index=%"LS(PRId64)L", volatility=%f, put call ratio=%f, "
				L"forward_price=%f, dividend=%f, interest=%f, index=0x%"LS(PRIX64)L"}\n",
				srs[i].expiration, srs[i].index, srs[i].volatility, srs[i].put_call_ratio,
				srs[i].forward_price, srs[i].dividend, srs[i].interest, srs[i].index);
		}
	}

	if (event_type == DXF_ET_CONFIGURATION) {
		dxf_configuration_t* cnf = (dxf_configuration_t*)data;

		for (; i < data_count; ++i) {
			wprintf(L"object=%ls}\n",
				cnf[i].object);
		}
	}
}
/* -------------------------------------------------------------------------- */

dxf_string_t ansi_to_unicode(const char* ansi_str, size_t len) {
#ifdef _WIN32
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
	size_t wide_size = mbstowcs(NULL, ansi_str, len); // len is ignored

	if (wide_size > 0 && wide_size != (size_t)-1) {
		wide_str = calloc(len + 1, sizeof(dxf_char_t));
		mbstowcs(wide_str, ansi_str, len);
	}

	return wide_str;
#endif /* _WIN32 */
}

/* -------------------------------------------------------------------------- */

typedef struct {
	int type;
	const char* name;
} event_type_data_t;

static event_type_data_t types_data[] = {
	{ DXF_ET_TRADE, "TRADE" },
	{ DXF_ET_QUOTE, "QUOTE" },
	{ DXF_ET_SUMMARY, "SUMMARY" },
	{ DXF_ET_PROFILE, "PROFILE" },
	{ DXF_ET_ORDER, "ORDER" },
	{ DXF_ET_TIME_AND_SALE, "TIME_AND_SALE" },
	/* The Candle event is not supported in this sample
	{ DXF_ET_CANDLE, "CANDLE" },*/
	{ DXF_ET_TRADE_ETH, "TRADE_ETH" },
	{ DXF_ET_SPREAD_ORDER, "SPREAD_ORDER" },
	{ DXF_ET_GREEKS, "GREEKS" },
	{ DXF_ET_THEO_PRICE, "THEO_PRICE" },
	{ DXF_ET_UNDERLYING, "UNDERLYING" },
	{ DXF_ET_SERIES, "SERIES" },
	{ DXF_ET_CONFIGURATION, "CONFIGURATION" }
};

static const int types_count = sizeof(types_data) / sizeof(types_data[0]);

bool get_next_substring(OUT char** substring_start, OUT size_t* substring_length) {
	char* string = *substring_start;
	char* sep_pos;
	if (strlen(string) == 0)
		return false;
	//remove separators from begin of string
	while ((sep_pos = strchr(string, ',')) == string) {
		string++;
		if (strlen(string) == 0)
			return false;
	}
	if (sep_pos == NULL)
		*substring_length = strlen(string);
	else
		*substring_length = sep_pos - string;
	*substring_start = string;
	return true;
}

bool is_equal_type_names(event_type_data_t type_data, const char* type_name, size_t len) {
#ifdef _WIN32
	return strlen(type_data.name) == len && strnicmp(type_data.name, type_name, len) == 0;
#else
	return strlen(type_data.name) == len && strncasecmp(type_data.name, type_name, len) == 0;
#endif
}

bool parse_event_types(char* types_string, OUT int* types_bitmask) {
	char* next_string = types_string;
	size_t next_len = 0;
	int i;
	if (types_string == NULL || types_bitmask == NULL) {
		printf("Invalid input parameter.\n");
		return false;
	}
	*types_bitmask = 0;
	while (get_next_substring(&next_string, &next_len)) {
		bool is_found = false;
		for (i = 0; i < types_count; i++) {
			if (is_equal_type_names(types_data[i], (const char*)next_string, next_len)) {
				*types_bitmask |= types_data[i].type;
				is_found = true;
			}
		}
		if (!is_found)
			printf("Invalid type parameter begining with:%s\n", next_string);
		next_string += next_len;
	}
	return true;
}

void free_symbols(dxf_string_t* symbols, int symbol_count) {
	int i;
	if (symbols == NULL)
		return;
	for (i = 0; i < symbol_count; i++) {
		free(symbols[i]);
	}
	free(symbols);
}

bool parse_symbols(char* symbols_string, OUT dxf_string_t** symbols, OUT int* symbol_count) {
	int count = 0;
	char* next_string = symbols_string;
	size_t next_len = 0;
	dxf_string_t* symbol_array = NULL;
	if (symbols_string == NULL || symbols == NULL || symbol_count == NULL) {
		printf("Invalid input parameter.\n");
		return false;
	}
	while (get_next_substring(&next_string, &next_len)) {
		dxf_string_t symbol = ansi_to_unicode(next_string, next_len);

		if (symbol == NULL) {
			free_symbols(symbol_array, count);
			return false;
		}

		if (symbol_array == NULL) {
			symbol_array = calloc(count + 1, sizeof(dxf_string_t));
			if (symbol_array == NULL) {
				free(symbol);
				return false;
			}
			symbol_array[count] = symbol;
		} else {
			dxf_string_t* temp = calloc(count + 1, sizeof(dxf_string_t));
			if (temp == NULL) {
				free_symbols(symbol_array, count);
				free(symbol);
				return false;
			}
			memcpy(temp, symbol_array, count * sizeof(dxf_string_t));
			temp[count] = symbol;
			free(symbol_array);
			symbol_array = temp;
		}

		count++;
		next_string += next_len;
	}

	*symbols = symbol_array;
	*symbol_count = count;

	return true;
}

dx_event_subscr_flag subscr_data_to_flags(char* subscr_data) {
	if (subscr_data == NULL) {
		return dx_esf_default;
	}

	if (strcmp(subscr_data, "TICKER") == 0) {
		return dx_esf_force_ticker;
	} else if (strcmp(subscr_data, "STREAM") == 0) {
		return dx_esf_force_stream;
	} else if (strcmp(subscr_data, "HISTORY") == 0) {
		return dx_esf_force_history;
	}

	return dx_esf_default;
}

/* -------------------------------------------------------------------------- */

int main (int argc, char* argv[]) {
	dxf_connection_t connection;
	dxf_subscription_t subscription;
	int loop_counter = 604800;
	int event_type;
	dxf_string_t* symbols = NULL;
	int symbol_count = 0;
	char* dxfeed_host = NULL;
	dxf_string_t dxfeed_host_u = NULL;

	if ( argc < STATIC_PARAMS_COUNT ) {
		printf("DXFeed command line sample.\n"
				"Usage: CommandLineSample <server address> <event type> <symbol> "
				"[" DUMP_PARAM_LONG_TAG " | " DUMP_PARAM_SHORT_TAG " <filename>] [" TOKEN_PARAM_SHORT_TAG " <token>] "
				"[" SUBSCRIPTION_DATA_PARAM_TAG " <subscr_data>]\n"
				"  <server address> - The DXFeed server address, e.g. demo.dxfeed.com:7300\n"
				"                     If you want to use file instead of server data just\n"
				"                     write there path to file e.g. path\\to\\raw.bin\n"
				"  <event type>     - The event type, any of the following: TRADE, QUOTE,\n"
				"                     SUMMARY, PROFILE, ORDER, TIME_AND_SALE, TRADE_ETH,\n"
				"                     SPREAD_ORDER, GREEKS, THEO_PRICE, UNDERLYING, SERIES,\n"
				"                     CONFIGURATION\n"
				"  <symbol>         - The trade symbols, e.g. C, MSFT, YHOO, IBM. All symbols - *\n"
				"  " DUMP_PARAM_LONG_TAG " | " DUMP_PARAM_SHORT_TAG " <filename> - The filename to dump the raw data\n"
				"  " TOKEN_PARAM_SHORT_TAG " <token>             - The authorization token\n"
				"  " SUBSCRIPTION_DATA_PARAM_TAG " <subscr_data>       - The subscription data: TICKER, STREAM or HISTORY\n"
				"Example: CommandLineSample.exe demo.dxfeed.com:7300 TRADE,ORDER MSFT,IBM\n"
				);

		return 0;
	}

	dxf_initialize_logger("command-line-api.log", true, true, true);

	dxfeed_host = argv[1];

	if (!parse_event_types(argv[2], &event_type))
		return -1;

	if (!parse_symbols(argv[3], &symbols, &symbol_count))
		return -1;

	char* dump_file_name = NULL;
	char* token = NULL;
	char* subscr_data = NULL;

	if (argc > STATIC_PARAMS_COUNT) {
		int i = 0;
		bool dump_filename_is_set = false;
		bool token_is_set = false;
		bool subscr_data_is_set = false;

		for (i = STATIC_PARAMS_COUNT; i < argc; i++) {
			if (dump_filename_is_set == false &&
					(strcmp(argv[i], DUMP_PARAM_SHORT_TAG) == 0 || strcmp(argv[i], DUMP_PARAM_LONG_TAG) == 0)) {
				if (i + 1 == argc) {
					wprintf(L"The dump argument error\n");

					return -1;
				}

				dump_file_name = argv[++i];
				dump_filename_is_set = true;
			} else if (token_is_set == false && strcmp(argv[i], TOKEN_PARAM_SHORT_TAG) == 0) {
				if (i + 1 == argc) {
					wprintf(L"The token argument error\n");

					return -1;
				}

				token = argv[++i];
				token_is_set = true;
			} else if (subscr_data_is_set == false && strcmp(argv[i], SUBSCRIPTION_DATA_PARAM_TAG) == 0) {
				if (i + 1 == argc) {
					wprintf(L"The subscription data argument error\n");

					return -1;
				}

				subscr_data = argv[++i];
				subscr_data_is_set = true;
			}
		}
	}

	wprintf(L"CommandLineSample started.\n");
	dxfeed_host_u = ansi_to_unicode(dxfeed_host, strlen(dxfeed_host));
	wprintf(L"Connecting to host %ls...\n", dxfeed_host_u);
	free(dxfeed_host_u);

#ifdef _WIN32
	InitializeCriticalSection(&listener_thread_guard);
#endif

	if (token != NULL && token[0] != '\0') {
		if (!dxf_create_connection_auth_bearer(dxfeed_host, token, on_reader_thread_terminate, on_connection_status_changed, NULL, NULL, NULL, &connection)) {
			process_last_error();
			free_symbols(symbols, symbol_count);
			return -1;
		}
	} else if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, on_connection_status_changed, NULL, NULL, NULL, &connection)) {
		process_last_error();
		free_symbols(symbols, symbol_count);
		return -1;
	}

	wprintf(L"Connection successful!\n");

	if (dump_file_name != NULL) {
		dxf_write_raw_data(connection, dump_file_name);
	}

	dx_event_subscr_flag subsc_flags = subscr_data_to_flags(subscr_data);

	ERRORCODE subscription_result;

	if (subsc_flags == dx_esf_default) {
		subscription_result = dxf_create_subscription(connection, event_type, &subscription);
	} else {
		subscription_result = dxf_create_subscription_with_flags(connection, event_type, subsc_flags, &subscription);
	}

	if (subscription_result == DXF_FAILURE) {
		process_last_error();
		free_symbols(symbols, symbol_count);
		return -1;
	}

	if (!dxf_add_symbols(subscription, (dxf_const_string_t*)symbols, symbol_count)) {
		process_last_error();
		free_symbols(symbols, symbol_count);
		return -1;
	}

	free_symbols(symbols, symbol_count);

	if (!dxf_attach_event_listener(subscription, listener, NULL)) {
		process_last_error();

		return -1;
	}
	wprintf(L"Subscription successful!\n");

	while (!is_thread_terminate() && loop_counter--) {
#ifdef _WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
	}

	wprintf(L"Disconnecting from host...\n");

	if (!dxf_close_connection(connection)) {
		process_last_error();

		return -1;
	}

	wprintf(L"Disconnect successful!\n");

#ifdef _WIN32
	DeleteCriticalSection(&listener_thread_guard);
#endif

	return 0;
}
