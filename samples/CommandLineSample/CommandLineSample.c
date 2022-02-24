/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 2.0 (the "License"); you may not use this file except in compliance with
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

#ifdef _WIN32
#	ifndef _CRT_STDIO_ISO_WIDE_SPECIFIERS
#		define _CRT_STDIO_ISO_WIDE_SPECIFIERS 1
#	endif
#endif

#ifdef _WIN32
#	pragma warning(push)
#	pragma warning(disable : 5105)
#	include <Windows.h>
#	pragma warning(pop)
#else
#	include <stdlib.h>
#	include <string.h>
#	include <unistd.h>
#	include <wctype.h>
#	define stricmp strcasecmp
#endif

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

#include "DXErrorCodes.h"
#include "DXFeed.h"

#ifndef __cplusplus
#	ifndef true
#		define true 1
#	endif

#	ifndef false
#		define false 0
#	endif
#endif

#if !defined(_WIN32) || defined(USE_PTHREADS)
#	include "pthread.h"
#	ifndef USE_PTHREADS
#		define USE_PTHREADS
#	endif
typedef pthread_t dxs_thread_t;
typedef pthread_key_t dxs_key_t;
typedef struct {
	pthread_mutex_t mutex;
	pthread_mutexattr_t attr;
} dxs_mutex_t;
#else  /* !defined(_WIN32) || defined(USE_PTHREADS) */
typedef HANDLE dxs_thread_t;
typedef DWORD dxs_key_t;
typedef LPCRITICAL_SECTION dxs_mutex_t;
#endif /* !defined(_WIN32) || defined(USE_PTHREADS) */

#ifdef _WIN32
// To fix problem with MS implementation of swprintf
#	define swprintf _snwprintf

void dxs_sleep(int milliseconds) { Sleep((DWORD)milliseconds); }

int dxs_mutex_create(dxs_mutex_t* mutex) {
	*mutex = calloc(1, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(*mutex);
	return true;
}

/* -------------------------------------------------------------------------- */

int dxs_mutex_destroy(dxs_mutex_t* mutex) {
	DeleteCriticalSection(*mutex);
	free(*mutex);
	return true;
}

/* -------------------------------------------------------------------------- */

int dxs_mutex_lock(dxs_mutex_t* mutex) {
	EnterCriticalSection(*mutex);
	return true;
}

/* -------------------------------------------------------------------------- */

int dxs_mutex_unlock(dxs_mutex_t* mutex) {
	LeaveCriticalSection(*mutex);
	return true;
}
#else
#	include "pthread.h"

void dxs_sleep(int milliseconds) {
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&ts, NULL);
}

int dxs_mutex_create(dxs_mutex_t* mutex) {
	if (pthread_mutexattr_init(&mutex->attr) != 0) {
		return false;
	}

	if (pthread_mutexattr_settype(&mutex->attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
		return false;
	}

	if (pthread_mutex_init(&mutex->mutex, &mutex->attr) != 0) {
		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

int dxs_mutex_destroy(dxs_mutex_t* mutex) {
	if (pthread_mutex_destroy(&mutex->mutex) != 0) {
		return false;
	}

	if (pthread_mutexattr_destroy(&mutex->attr) != 0) {
		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

int dxs_mutex_lock(dxs_mutex_t* mutex) {
	if (pthread_mutex_lock(&mutex->mutex) != 0) {
		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

int dxs_mutex_unlock(dxs_mutex_t* mutex) {
	if (pthread_mutex_unlock(&mutex->mutex) != 0) {
		return false;
	}

	return true;
}

#endif	//_WIN32

#define LS(s)  LS2(s)
#define LS2(s) L##s

// plus the name of the executable
#define STATIC_PARAMS_COUNT			4
#define DUMP_PARAM_SHORT_TAG		"-d"
#define DUMP_PARAM_LONG_TAG			"--dump"
#define TOKEN_PARAM_SHORT_TAG		"-T"
#define SUBSCRIPTION_DATA_PARAM_TAG "-s"
#define LOG_DATA_TRANSFER_TAG		"-p"
#define TIMEOUT_TAG					"-o"
#define LOG_HEARTBEAT_TAG			"-b"
#define RECONNECT_TAG			    "-r"

// Prevents file names globbing (converting * to all files in the current dir)
#ifdef __MINGW64_VERSION_MAJOR
int _CRT_glob = 0;
#endif

static int is_listener_thread_terminated = false;
static dxs_mutex_t listener_thread_guard;

int is_thread_terminate() {
	int res;
	dxs_mutex_lock(&listener_thread_guard);
	res = is_listener_thread_terminated;
	dxs_mutex_unlock(&listener_thread_guard);

	return res;
}

void process_last_error() {
	int error_code = dx_ec_success;
	dxf_const_string_t error_descr = NULL;
	int res;

	res = dxf_get_last_error(&error_code, &error_descr);

	if (res == DXF_SUCCESS) {
		if (error_code == dx_ec_success) {
			wprintf(L"No error information is stored");
			return;
		}

		wprintf(
			L"Error occurred and successfully retrieved:\n"
			L"error code = %d, description = \"%ls\"\n",
			error_code, error_descr);
		return;
	}

	wprintf(L"An error occurred but the error subsystem failed to initialize\n");
}

void on_reader_thread_terminate(dxf_connection_t connection, void* user_data) {
	(void)connection;
	(void)user_data;
	dxs_mutex_lock(&listener_thread_guard);
	is_listener_thread_terminated = true;
	dxs_mutex_unlock(&listener_thread_guard);

	wprintf(L"\nTerminating listener thread\n");
	process_last_error();
}

dxf_const_string_t connection_status_to_string(dxf_connection_status_t status) {
	switch (status) {
		case dxf_cs_not_connected:
			return L"Not connected";
		case dxf_cs_connected:
			return L"Connected";
		case dxf_cs_login_required:
			return L"Login required";
		case dxf_cs_authorized:
			return L"Authorized";
		default:
			return L"";
	}

	return L"";
}

// The example of processing a connection status change
void on_connection_status_changed(dxf_connection_t connection, dxf_connection_status_t old_status,
								  dxf_connection_status_t new_status, void* user_data) {
	wprintf(L"The connection status has been changed: %ls -> %ls\n", connection_status_to_string(old_status),
			connection_status_to_string(new_status));
}

void print_timestamp(dxf_long_t timestamp) {
	wchar_t timefmt[80];

	struct tm* timeinfo;
	time_t tmpint = (time_t)(timestamp / 1000);
	timeinfo = localtime(&tmpint);
	wcsftime(timefmt, 80, L"%Y%m%d-%H%M%S", timeinfo);
	wprintf(L"%ls", timefmt);
}
/* -------------------------------------------------------------------------- */

void listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
			  void* user_data) {
	wprintf(L"%ls{symbol=%ls, ", dx_event_type_to_string(event_type), symbol_name);

	if (event_type == DXF_ET_QUOTE) {
		dxf_quote_t* q = (dxf_quote_t*)data;

		wprintf(L"bidTime=");
		print_timestamp(q->bid_time);
		wprintf(L" bidExchangeCode=%c, bidPrice=%.15g, bidSize=%.15g, ", q->bid_exchange_code, q->bid_price,
				q->bid_size);
		wprintf(L"askTime=");
		print_timestamp(q->ask_time);
		wprintf(L" askExchangeCode=%c, askPrice=%.15g, askSize=%.15g, scope=%d}\n", q->ask_exchange_code, q->ask_price,
				q->ask_size, (int)q->scope);
	}

	if (event_type == DXF_ET_ORDER) {
		dxf_order_t* o = (dxf_order_t*)data;

		wprintf(L"index=0x%llX, side=%i, scope=%i, time=", o->index, o->side, o->scope);
		print_timestamp(o->time);
		wprintf(L", exchange code=%c, market maker=%ls, price=%.15g, size=%.15g", o->exchange_code, o->market_maker,
				o->price, o->size);

		if (wcslen(o->source) > 0) wprintf(L", source=%ls", o->source);

		wprintf(L", count=%.15g, flags=0x%X}\n", o->count, o->event_flags);
	}

	if (event_type == DXF_ET_TRADE) {
		dxf_trade_t* tr = (dxf_trade_t*)data;

		print_timestamp(tr->time);
		wprintf(
			L", exchangeCode=%c, price=%.15g, size=%.15g, tick=%i, change=%.15g, day id=%d, day volume=%.15g, "
			L"scope=%d}\n",
			tr->exchange_code, tr->price, tr->size, tr->tick, tr->change, tr->day_id, tr->day_volume, (int)tr->scope);
	}

	if (event_type == DXF_ET_SUMMARY) {
		dxf_summary_t* s = (dxf_summary_t*)data;

		wprintf(
			L"day id=%d, day open price=%.15g, day high price=%.15g, day low price=%.15g, day close price=%.15g, "
			L"prev day id=%d, prev day close price=%.15g, open interest=%.15g, flags=0x%X, exchange=%c, "
			L"day close price type=%i, prev day close price type=%i, scope=%d}\n",
			s->day_id, s->day_open_price, s->day_high_price, s->day_low_price, s->day_close_price, s->prev_day_id,
			s->prev_day_close_price, s->open_interest, s->raw_flags, s->exchange_code, s->day_close_price_type,
			s->prev_day_close_price_type, s->scope);
	}

	if (event_type == DXF_ET_PROFILE) {
		dxf_profile_t* p = (dxf_profile_t*)data;

		wprintf(
			L"Beta=%f, eps=%.15g, div freq=%.15g, exd div amount=%.15g, exd div date=%i, 52 high price=%.15g, "
			L"52 low price=%.15g, shares=%.15g, Description=%ls, flags=%i, status_reason=%ls, halt start time=",
			p->beta, p->eps, p->div_freq, p->exd_div_amount, p->exd_div_date, p->high_52_week_price,
			p->low_52_week_price, p->shares, p->description, p->raw_flags, p->status_reason);
		print_timestamp(p->halt_start_time);
		wprintf(L", halt end time=");
		print_timestamp(p->halt_end_time);
		wprintf(L", high limit price=%.15g, low limit price=%.15g}\n", p->high_limit_price, p->low_limit_price);
	}

	if (event_type == DXF_ET_TIME_AND_SALE) {
		dxf_time_and_sale_t* tns = (dxf_time_and_sale_t*)data;

		wprintf(L"event id=%" LS(PRId64) L", time=", tns->index);
		print_timestamp(tns->time);
		wprintf(
			L", exchange code=%c, price=%.15g, size=%.15g, bid price=%.15g, ask price=%.15g, "
			L"exchange sale conditions=\'%ls\', is ETH trade=%ls, type=%i, buyer=\'%ls\', seller=\'%ls\', "
			L"scope=%d, flags=0x%X, raw_flags=0x%X}\n",
			tns->exchange_code, tns->price, tns->size, tns->bid_price, tns->ask_price, tns->exchange_sale_conditions,
			tns->is_eth_trade ? L"True" : L"False", tns->type, tns->buyer ? tns->buyer : L"<UNKNOWN>",
			tns->seller ? tns->seller : L"<UNKNOWN>", tns->scope, tns->event_flags, tns->raw_flags);
	}

	if (event_type == DXF_ET_TRADE_ETH) {
		dxf_trade_t* tr = (dxf_trade_t*)data;

		print_timestamp(tr->time);
		wprintf(
			L", exchangeCode=%c, flags=%d, price=%.15g, size=%.15g, change=%.15g, day id=%d, day volume=%.15g, "
			L"scope=%d}\n",
			tr->exchange_code, tr->raw_flags, tr->price, tr->size, tr->change, tr->day_id, tr->day_volume,
			(int)tr->scope);
	}

	if (event_type == DXF_ET_SPREAD_ORDER) {
		dxf_order_t* o = (dxf_order_t*)data;

		wprintf(L"index=0x%llX, side=%i, scope=%i, time=", o->index, o->side, o->scope);
		print_timestamp(o->time);
		wprintf(
			L", sequence=%i, exchange code=%c, price=%.15g, size=%.15g, source=%ls, "
			L"count=%.15g, flags=%i, spread symbol=%ls}\n",
			o->sequence, o->exchange_code, o->price, o->size, wcslen(o->source) > 0 ? o->source : L"", o->count,
			o->event_flags, wcslen(o->spread_symbol) > 0 ? o->spread_symbol : L"");
	}

	if (event_type == DXF_ET_GREEKS) {
		dxf_greeks_t* grks = (dxf_greeks_t*)data;

		wprintf(L"time=");
		print_timestamp(grks->time);
		wprintf(L", index=%"LS(PRId64)L", greeks price=%.15g, volatility=%f, "
			L"delta=%.15g, gamma=%.15g, theta=%.15g, rho=%.15g, vega=%.15g, index=0x%"LS(PRIX64)L", flags=0x%X}\n",
			grks->index, grks->price, grks->volatility,
			grks->delta, grks->gamma, grks->theta, grks->rho, grks->vega, grks->index,
			grks->event_flags);
	}

	if (event_type == DXF_ET_THEO_PRICE) {
		dxf_theo_price_t* tp = (dxf_theo_price_t*)data;

		wprintf(L"theo time=");
		print_timestamp(tp->time);
		wprintf(
			L", theo price=%.15g, theo underlying price=%.15g, theo delta=%.15g, "
			L"theo gamma=%.15g, theo dividend=%.15g, theo_interest=%.15g}\n",
			tp->price, tp->underlying_price, tp->delta, tp->gamma, tp->dividend, tp->interest);
	}

	if (event_type == DXF_ET_UNDERLYING) {
		dxf_underlying_t* u = (dxf_underlying_t*)data;

		wprintf(
			L"volatility=%.15g, front volatility=%.15g, back volatility=%.15g, call volume=%.15g, put volume=%.15g, "
			L"option volume=%.15g, put call ratio=%.15g}\n",
			u->volatility, u->front_volatility, u->back_volatility, u->call_volume, u->put_volume, u->option_volume,
			u->put_call_ratio);
	}

	if (event_type == DXF_ET_SERIES) {
		dxf_series_t* srs = (dxf_series_t*)data;

		wprintf(L"expiration=%d, index=%"LS(PRId64)L", volatility=%.15g, call volume=%.15g, put volume=%.15g, "
			L"option volume=%.15g, put call ratio=%.15g, forward_price=%.15g, dividend=%.15g, interest=%.15g, "
			L"index=0x%"LS(PRIX64)L", flags=0x%X}\n",
			srs->expiration, srs->index, srs->volatility, srs->call_volume, srs->put_volume,
			srs->option_volume, srs->put_call_ratio, srs->forward_price, srs->dividend, srs->interest,
			srs->index, srs->event_flags);
	}

	if (event_type == DXF_ET_CONFIGURATION) {
		dxf_configuration_t* cnf = (dxf_configuration_t*)data;

		wprintf(L"object=%ls}\n", cnf->object);
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
#else  /* _WIN32 */
	dxf_string_t wide_str = NULL;
	size_t wide_size = mbstowcs(NULL, ansi_str, len);  // len is ignored

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

static event_type_data_t types_data[] = {{DXF_ET_TRADE, "TRADE"},
										 {DXF_ET_QUOTE, "QUOTE"},
										 {DXF_ET_SUMMARY, "SUMMARY"},
										 {DXF_ET_PROFILE, "PROFILE"},
										 {DXF_ET_ORDER, "ORDER"},
										 {DXF_ET_TIME_AND_SALE, "TIME_AND_SALE"},
										 /* The Candle event is not supported in this sample
										 { DXF_ET_CANDLE, "CANDLE" },*/
										 {DXF_ET_TRADE_ETH, "TRADE_ETH"},
										 {DXF_ET_SPREAD_ORDER, "SPREAD_ORDER"},
										 {DXF_ET_GREEKS, "GREEKS"},
										 {DXF_ET_THEO_PRICE, "THEO_PRICE"},
										 {DXF_ET_UNDERLYING, "UNDERLYING"},
										 {DXF_ET_SERIES, "SERIES"},
										 {DXF_ET_CONFIGURATION, "CONFIGURATION"}};

static const int types_count = sizeof(types_data) / sizeof(types_data[0]);

int get_next_substring(OUT char** substring_start, OUT size_t* substring_length) {
	char* string = *substring_start;
	char* sep_pos;
	if (strlen(string) == 0) return false;
	// remove separators from begin of string
	while ((sep_pos = strchr(string, ',')) == string) {
		string++;
		if (strlen(string) == 0) return false;
	}
	if (sep_pos == NULL)
		*substring_length = strlen(string);
	else
		*substring_length = sep_pos - string;
	*substring_start = string;
	return true;
}

int is_equal_type_names(event_type_data_t type_data, const char* type_name, size_t len) {
#ifdef _WIN32
	return strlen(type_data.name) == len && strnicmp(type_data.name, type_name, len) == 0;
#else
	return strlen(type_data.name) == len && strncasecmp(type_data.name, type_name, len) == 0;
#endif
}

int parse_event_types(char* types_string, OUT int* types_bitmask) {
	char* next_string = types_string;
	size_t next_len = 0;
	int i;
	if (types_string == NULL || types_bitmask == NULL) {
		wprintf(L"Invalid input parameter.\n");
		return false;
	}
	*types_bitmask = 0;
	while (get_next_substring(&next_string, &next_len)) {
		int is_found = false;
		for (i = 0; i < types_count; i++) {
			if (is_equal_type_names(types_data[i], (const char*)next_string, next_len)) {
				*types_bitmask |= types_data[i].type;
				is_found = true;
			}
		}
		if (!is_found) wprintf(L"Invalid type parameter starting with:%hs\n", next_string);
		next_string += next_len;
	}
	return true;
}

void free_symbols(dxf_string_t* symbols, int symbol_count) {
	int i;
	if (symbols == NULL) return;
	for (i = 0; i < symbol_count; i++) {
		free(symbols[i]);
	}
	free(symbols);
}

int parse_symbols(char* symbols_string, OUT dxf_string_t** symbols, OUT int* symbol_count) {
	int count = 0;
	char* next_string = symbols_string;
	size_t next_len = 0;
	dxf_string_t* symbol_array = NULL;
	if (symbols_string == NULL || symbols == NULL || symbol_count == NULL) {
		wprintf(L"Invalid input parameter.\n");
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

int atoi2(char* str, int* result) {
	if (str == NULL || str[0] == '\0' || result == NULL) {
		return false;
	}

	if (str[0] == '0' && str[1] == '\0') {
		*result = 0;

		return true;
	}

	int r = atoi(str);

	if (r == 0) {
		return false;
	}

	*result = r;

	return true;
}

void on_server_heartbeat_notifier(dxf_connection_t connection, dxf_long_t server_millis, dxf_int_t server_lag_mark,
								  dxf_int_t connection_rtt, void* user_data) {
	fwprintf(stderr, L"\n##### Server time (UTC) = %" PRId64 " ms, Server lag = %d us, RTT = %d us #####\n",
			 server_millis, server_lag_mark, connection_rtt);
}

/* -------------------------------------------------------------------------- */

int main(int argc, char* argv[]) {
	dxf_connection_t connection;
	dxf_subscription_t subscription;
	int event_type;
	dxf_string_t* symbols = NULL;
	int symbol_count = 0;
	char* dxfeed_host = NULL;

	if (argc < STATIC_PARAMS_COUNT) {
		printf(
			"DXFeed command line sample.\n"
			"Usage: CommandLineSample <server address>|<path> <event type> <symbol> "
			"[" DUMP_PARAM_LONG_TAG " | " DUMP_PARAM_SHORT_TAG " <filename>] [" TOKEN_PARAM_SHORT_TAG
			" <token>] "
			"[" SUBSCRIPTION_DATA_PARAM_TAG " <subscr_data>] [" LOG_DATA_TRANSFER_TAG "] [" TIMEOUT_TAG
			" <timeout>] [" LOG_HEARTBEAT_TAG
			"] [" RECONNECT_TAG "]\n"
			"  <server address> - The DXFeed server address, e.g. demo.dxfeed.com:7300\n"
			"                     If you want to use file instead of server data just\n"
			"                     write there path to file e.g. path\\to\\raw.bin\n"
			"  <path>           - The path to `tape` file or raw dump\n"
			"                     If you want to use file instead of server data just\n"
			"                     write there path to file e.g. path\\to\\raw.bin\n"
			"  <event type>     - The event type, any of the following: TRADE, QUOTE,\n"
			"                     SUMMARY, PROFILE, ORDER, TIME_AND_SALE, TRADE_ETH,\n"
			"                     SPREAD_ORDER, GREEKS, THEO_PRICE, UNDERLYING, SERIES,\n"
			"                     CONFIGURATION\n"
			"  <symbol>         - The trade symbols, e.g. C, MSFT, YHOO, IBM. All symbols - *\n"
			"  " DUMP_PARAM_LONG_TAG " | " DUMP_PARAM_SHORT_TAG
			" <filename> - The filename to dump the raw data\n"
			"  " TOKEN_PARAM_SHORT_TAG
			" <token>             - The authorization token\n"
			"  " SUBSCRIPTION_DATA_PARAM_TAG
			" <subscr_data>       - The subscription data: TICKER, STREAM or HISTORY\n"
			"  " LOG_DATA_TRANSFER_TAG
			"                     - Enables the data transfer logging\n"
			"  " TIMEOUT_TAG
			" <timeout>           - Sets the program timeout in seconds (default = 604800, i.e a week)\n"
			"  " LOG_HEARTBEAT_TAG
			"                     - Enables the server's heartbeat logging to console\n"
			"  " RECONNECT_TAG
			"                     - Enables the reconnection\n"
			"Examples:\n"
			"    CommandLineSample.exe demo.dxfeed.com:7300 TRADE,ORDER MSFT,IBM\n"
			"    CommandLineSample.exe ./tape_file TRADE,ORDER MSFT,IBM\n");

		return 0;
	}

	dxfeed_host = argv[1];

	if (!parse_event_types(argv[2], &event_type)) return -1;

	if (!parse_symbols(argv[3], &symbols, &symbol_count)) return -1;

	char* dump_file_name = NULL;
	char* token = NULL;
	char* subscr_data = NULL;
	int log_data_transfer_flag = false;
	int program_timeout = 604800;  // a week
	int log_heartbeat_is_set = false;
	int reconnect_is_set = false;

	if (argc > STATIC_PARAMS_COUNT) {
		int dump_filename_is_set = false;
		int token_is_set = false;
		int subscr_data_is_set = false;
		int program_timeout_is_set = false;

		for (int i = STATIC_PARAMS_COUNT; i < argc; i++) {
			if (dump_filename_is_set == false &&
				(strcmp(argv[i], DUMP_PARAM_SHORT_TAG) == 0 || strcmp(argv[i], DUMP_PARAM_LONG_TAG) == 0)) {
				if (i + 1 == argc) {
					wprintf(L"The dump argument error\n");

					return 1;
				}

				dump_file_name = argv[++i];
				dump_filename_is_set = true;
			} else if (token_is_set == false && strcmp(argv[i], TOKEN_PARAM_SHORT_TAG) == 0) {
				if (i + 1 == argc) {
					wprintf(L"The token argument error\n");

					return 2;
				}

				token = argv[++i];
				token_is_set = true;
			} else if (subscr_data_is_set == false && strcmp(argv[i], SUBSCRIPTION_DATA_PARAM_TAG) == 0) {
				if (i + 1 == argc) {
					wprintf(L"The subscription data argument error\n");

					return 3;
				}

				subscr_data = argv[++i];
				subscr_data_is_set = true;
			} else if (log_data_transfer_flag == false && strcmp(argv[i], LOG_DATA_TRANSFER_TAG) == 0) {
				log_data_transfer_flag = true;
			} else if (program_timeout_is_set == false && strcmp(argv[i], TIMEOUT_TAG) == 0) {
				if (i + 1 == argc) {
					wprintf(L"The program timeout argument error\n");

					return 4;
				}

				int new_program_timeout = -1;

				if (!atoi2(argv[++i], &new_program_timeout)) {
					wprintf(L"The program timeout argument parsing error\n");

					return 5;
				}

				program_timeout = new_program_timeout;
				program_timeout_is_set = true;
			} else if (log_heartbeat_is_set == false && strcmp(argv[i], LOG_HEARTBEAT_TAG) == 0) {
				log_heartbeat_is_set = true;
			} else if (reconnect_is_set == false && strcmp(argv[i], RECONNECT_TAG) == 0) {
				reconnect_is_set = true;
			}
		}
	}

	dxf_initialize_logger_v2("command-line-api.log", true, true, true, log_data_transfer_flag);
	//dxf_load_config_from_string("network.heartbeatTimeout = 11\n");

	wprintf(L"Command line sample started.\n");
	wprintf(L"Connecting to host %hs...\n", dxfeed_host);

	dxs_mutex_create(&listener_thread_guard);

	ERRORCODE connection_result;
	if (token != NULL && token[0] != '\0') {
		connection_result =
			dxf_create_connection_auth_bearer(dxfeed_host, token, on_reader_thread_terminate,
											  on_connection_status_changed, NULL, NULL, NULL, &connection);
	} else {
		connection_result = dxf_create_connection(dxfeed_host, on_reader_thread_terminate, on_connection_status_changed,
												  NULL, NULL, NULL, &connection);
	}

	if (connection_result == DXF_FAILURE) {
		free_symbols(symbols, symbol_count);
		process_last_error();
		dxs_mutex_destroy(&listener_thread_guard);

		return 10;
	}

	wprintf(L"Connected\n");

	if (log_heartbeat_is_set) {
		dxf_set_on_server_heartbeat_notifier(connection, on_server_heartbeat_notifier, NULL);
	}

	if (dump_file_name != NULL) {
		if (!dxf_write_raw_data(connection, dump_file_name)) {
			free_symbols(symbols, symbol_count);
			process_last_error();
			dxf_close_connection(connection);
			dxs_mutex_destroy(&listener_thread_guard);

			return 20;
		}
	}

	dx_event_subscr_flag subsc_flags = subscr_data_to_flags(subscr_data);

	ERRORCODE subscription_result;

	if (subsc_flags == dx_esf_default) {
		subscription_result = dxf_create_subscription(connection, event_type, &subscription);
	} else {
		subscription_result = dxf_create_subscription_with_flags(connection, event_type, subsc_flags, &subscription);
	}

	if (subscription_result == DXF_FAILURE) {
		free_symbols(symbols, symbol_count);
		process_last_error();
		dxf_close_connection(connection);
		dxs_mutex_destroy(&listener_thread_guard);

		return 30;
	}

	if (!dxf_attach_event_listener(subscription, listener, NULL)) {
		free_symbols(symbols, symbol_count);
		process_last_error();
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);
		dxs_mutex_destroy(&listener_thread_guard);

		return 31;
	}

	if (!dxf_add_symbols(subscription, (dxf_const_string_t*)symbols, symbol_count)) {
		free_symbols(symbols, symbol_count);
		process_last_error();
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);
		dxs_mutex_destroy(&listener_thread_guard);

		return 32;
	}

	wprintf(L"Subscribed\n");
	free_symbols(symbols, symbol_count);

	while (program_timeout--) {
		if (!reconnect_is_set && is_thread_terminate()) {
			break;
		}

		dxs_sleep(1000);
	}

	wprintf(L"Unsubscribing...\n");

	if (!dxf_close_subscription(subscription)) {
		process_last_error();
		dxs_mutex_destroy(&listener_thread_guard);

		return 33;
	}

	wprintf(L"Disconnecting from host...\n");

	if (!dxf_close_connection(connection)) {
		process_last_error();
		dxs_mutex_destroy(&listener_thread_guard);

		return 12;
	}

	wprintf(L"Disconnected\n");

	dxs_mutex_destroy(&listener_thread_guard);

	return 0;
}
