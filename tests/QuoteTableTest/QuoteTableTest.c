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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "DXErrorCodes.h"
#include "DXFeed.h"
#include "Logger.h"

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
#else /* !defined(_WIN32) || defined(USE_PTHREADS) */
#	pragma warning(push)
#	pragma warning(disable : 5105)
#	include <Windows.h>
#	pragma warning(pop)
#	define USE_WIN32_THREADS
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

static char dxfeed_host_default[] = "demo.dxfeed.com:7300";

struct event_info_t {
	int event_type;
	dxf_event_listener_t listener;
};

#define SYMBOLS_COUNT 63
// static const dx_const_string_t g_symbols[] = { {L"IBM"}, {L"MSFT"}, {L"YHOO"}, {L"C"} };
static dxf_const_string_t g_symbols[] = {
	L"BP",	L"AMD", L"BAC", L"BP",	L"C",	L"EMC", L"F",	L"GE",	L"HPQ", L"IBM",
	L"JPM", L"LCC", L"LSI", L"MGM", L"MO",	L"MOT", L"MU",	L"NOK", L"PFE", L"RBS",
	L"S",	L"SAP", L"T",	L"VMW", L"WFC", L"XTO", L"BXD", L"BXM", L"BXN", L"BXR",
	L"BXY", L"CEX", L"CYX", L"DDA", L"DDB", L"EVZ", L"EXQ", L"FVX", L"GOX", L"GVZ",
	L"INX", L"IRX", L"OIX", L"OVX", L"RVX", L"TNX", L"TXX", L"TYX", L"VIO", L"VIX",
	L"VWA", L"VWB", L"VXB", L"VXD", L"VXN", L"VXO", L"VXV", L"ZIO", L"ZOC", L"OEX",
	L"SPX", L"XEO", L"XSP"};

static const dxf_int_t g_symbols_size = sizeof(g_symbols) / sizeof(g_symbols[0]);

#define EVENTS_COUNT 4

static dxf_trade_t* trades[SYMBOLS_COUNT] = {0};
static dxf_quote_t* quotes[SYMBOLS_COUNT] = {0};
static dxf_summary_t* summaries[SYMBOLS_COUNT] = {0};
static dxf_profile_t* profiles[SYMBOLS_COUNT] = {0};

/* -------------------------------------------------------------------------- */
void trade_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					void* user_data);
void quote_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					void* user_data);
void summary_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					  void* user_data);
void profile_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					  void* user_data);

static struct event_info_t event_info[EVENTS_COUNT] = {{DXF_ET_TRADE, trade_listener},
													   {DXF_ET_QUOTE, quote_listener},
													   {DXF_ET_SUMMARY, summary_listener},
													   {DXF_ET_PROFILE, profile_listener}};

/* -------------------------------------------------------------------------- */

static int is_listener_thread_terminated = false;
static dxs_mutex_t listener_thread_guard;

int is_thread_terminate() {
	int res;
	dxs_mutex_lock(&listener_thread_guard);
	res = is_listener_thread_terminated;
	dxs_mutex_unlock(&listener_thread_guard);

	return res;
}

/* -------------------------------------------------------------------------- */

void on_reader_thread_terminate(dxf_connection_t connection, void* user_data) {
	char* host = (char*)user_data;
	dxs_mutex_lock(&listener_thread_guard);
	is_listener_thread_terminated = true;
	dxs_mutex_unlock(&listener_thread_guard);

	wprintf(L"\nTerminating listener thread, host: %hs\n", host);
}

/* -------------------------------------------------------------------------- */

int get_symbol_index(dxf_const_string_t symbol_name) {
	int i = 0;
	for (; i < g_symbols_size; ++i) {
		if (wcscmp(symbol_name, g_symbols[i]) == 0) {
			return i;
		}
	}

	return -1;
}

/* -------------------------------------------------------------------------- */

void output_data(int i) {
	dxf_trade_t* trade = trades[i];
	dxf_quote_t* quote = quotes[i];
	dxf_summary_t* summary = summaries[i];
	dxf_profile_t* profile = profiles[i];
	wchar_t dummy[2] = L"-";

	wprintf(
		L"\nSymbol: %ls, Last: %f, LastEx: %c, Change: %+f, Bid: %f, BidEx: %c, Ask: %f, AskEx: %c, High: %f, "
		L"Low: %f, Open: %f, BidSize: %i, AskSize: %i, LastSize: %i, LastTick: %i, LastChange: %f, Volume: %i, "
		L"Description: %ls\n",
		g_symbols[i], trade ? trade->price : 0.0, trade ? trade->exchange_code : dummy[0],
		(trade && summary) ? (trade->price - summary->prev_day_close_price) : 0.0, quote ? quote->bid_price : 0.0,
		quote ? quote->bid_exchange_code : dummy[0], quote ? quote->ask_price : 0.0,
		quote ? quote->ask_exchange_code : dummy[0], summary ? summary->day_high_price : 0.0,
		summary ? summary->day_low_price : 0.0, summary ? summary->day_open_price : 0.0,
		quote ? (int)quote->bid_size : 0, quote ? (int)quote->ask_size : 0, trade ? (int)trade->size : 0,
		trade ? (int)trade->tick : 0, trade ? trade->change : 0.0, trade ? (int)trade->day_volume : 0,
		profile ? profile->description : dummy);
}

/* -------------------------------------------------------------------------- */
dxf_event_data_t getData(int event_type, int i) {
	if (event_type == DXF_ET_TRADE) {
		if (trades[i] == NULL) {
			trades[i] = (dxf_trade_t*)calloc(1, sizeof(dxf_trade_t));
		}

		return trades[i];
	} else if (event_type == DXF_ET_QUOTE) {
		if (quotes[i] == NULL) {
			quotes[i] = (dxf_quote_t*)calloc(1, sizeof(dxf_quote_t));
		}

		return quotes[i];
	} else if (event_type == DXF_ET_SUMMARY) {
		if (summaries[i] == NULL) {
			summaries[i] = (dxf_summary_t*)calloc(1, sizeof(dxf_summary_t));
		}

		return summaries[i];
	} else if (event_type == DXF_ET_PROFILE) {
		if (profiles[i] == NULL) {
			profiles[i] = (dxf_profile_t*)calloc(1, sizeof(dxf_profile_t));
		}

		return profiles[i];
	} else
		return NULL;
}

void trade_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					void* user_data) {
	int symb_ind = get_symbol_index(symbol_name);
	if (symb_ind == -1) {
		wprintf(L"Error: unexpected symbol: %ls\n", symbol_name);
		return;
	}

	if (event_type != DXF_ET_TRADE) {
		wprintf(L"Error: event: %ls Symbol: %ls, expected event: Trade\n", dx_event_type_to_string(event_type),
				symbol_name);
		return;
	}

	dxf_trade_t* trades_data = (dxf_trade_t*)data;
	dxf_trade_t* dst_trade = (dxf_trade_t*)getData(event_type, symb_ind);
	if (dst_trade == NULL) {
		wprintf(L"Internal error");
		return;
	}

	for (int i = 0; i < data_count; ++i) {
		*dst_trade = trades_data[i];
		output_data(symb_ind);
	}
}

/* -------------------------------------------------------------------------- */

void quote_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					void* user_data) {
	int symb_ind = get_symbol_index(symbol_name);
	if (symb_ind == -1) {
		wprintf(L"Error: unexpected symbol: %ls\n", symbol_name);
		return;
	}

	if (event_type != DXF_ET_QUOTE) {
		wprintf(L"Error: event: %ls Symbol: %ls, expected event: Trade\n", dx_event_type_to_string(event_type),
				symbol_name);
		return;
	}

	dxf_quote_t* quotes_data = (dxf_quote_t*)data;
	dxf_quote_t* dst_quote = (dxf_quote_t*)getData(event_type, symb_ind);

	if (dst_quote == NULL) {
		wprintf(L"Internal error");
		return;
	}

	for (int i = 0; i < data_count; ++i) {
		*dst_quote = quotes_data[i];
		output_data(symb_ind);
	}
}

/* -------------------------------------------------------------------------- */

void summary_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					  void* user_data) {
	int symb_ind = get_symbol_index(symbol_name);
	if (symb_ind == -1) {
		wprintf(L"Error: unexpected symbol: %ls\n", symbol_name);
		return;
	}

	if (event_type != DXF_ET_SUMMARY) {
		wprintf(L"Error: event: %ls Symbol: %ls, expected event: Trade\n", dx_event_type_to_string(event_type),
				symbol_name);
		return;
	}

	dxf_summary_t* summaries_data = (dxf_summary_t*)data;
	dxf_summary_t* dst_summary = (dxf_summary_t*)getData(event_type, symb_ind);

	if (dst_summary == NULL) {
		wprintf(L"Internal error");
		return;
	}

	for (int i = 0; i < data_count; ++i) {
		*dst_summary = summaries_data[i];
		output_data(symb_ind);
	}
}

/* -------------------------------------------------------------------------- */

void profile_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					  void* user_data) {
	int symb_ind = get_symbol_index(symbol_name);
	if (symb_ind == -1) {
		wprintf(L"Error: unexpected symbol: %ls\n", symbol_name);
		return;
	}

	if (event_type != DXF_ET_PROFILE) {
		wprintf(L"Error: event: %ls Symbol: %ls, expected event: Trade\n", dx_event_type_to_string(event_type),
				symbol_name);
		return;
	}

	dxf_profile_t* profiles_data = (dxf_profile_t*)data;
	dxf_profile_t* dst_profile = (dxf_profile_t*)getData(event_type, symb_ind);

	if (dst_profile == NULL) {
		wprintf(L"Internal error");
		return;
	}

	for (int i = 0; i < data_count; ++i) {
		*dst_profile = profiles_data[i];
		output_data(symb_ind);
	}
}

/* -------------------------------------------------------------------------- */

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

dxf_subscription_t create_subscription(dxf_connection_t connection, int event_id) {
	dxf_subscription_t subscription;
	const int event_type = event_info[event_id].event_type;

	if (!dxf_create_subscription(connection, event_type, &subscription)) {
		process_last_error();

		return NULL;
	};

	if (dxf_add_symbols(subscription, g_symbols, g_symbols_size) != DXF_SUCCESS) {
		return NULL;
	}

	if (!dxf_attach_event_listener(subscription, event_info[event_id].listener, NULL)) {
		process_last_error();

		return NULL;
	};

	return subscription;
}
/* -------------------------------------------------------------------------- */

void print_usage() {
	wprintf(
		L"Usage: QuoteTableTest [<host>] [-h|--help|-?]\n"
		L"  <host>       - dxFeed host (default: demo.dxfeed.com:7300)\n"
		L"  -h|--help|-? - print usage\n\n");
}

int main(int argc, char* argv[]) {
	dxf_connection_t connection;
	dxf_subscription_t subscriptions[EVENTS_COUNT];
	int loop_counter = 10000;
	int i;
	char* dxfeed_host = NULL;

	if (argc < 2) {
		dxfeed_host = dxfeed_host_default;
	} else {
		if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "-?") == 0 || strcmp(argv[1], "--help") == 0) {
			print_usage();

			return 0;
		}

		dxfeed_host = argv[1];
	}

	dxf_initialize_logger("quote-table-api.log", true, true, true);

	wprintf(L"Quote table test started.\n");
	wprintf(L"Connecting to host %hs...\n", dxfeed_host);

	if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, dxfeed_host, &connection)) {
		process_last_error();
		return 1;
	}

	wprintf(L"Connected\n");

	// create subscriptions
	for (i = 0; i < EVENTS_COUNT; ++i) {
		subscriptions[i] = create_subscription(connection, i);
		if (subscriptions[i] == NULL) {
			return 10;
		}
	}

	wprintf(L"Subscribed\n");

	dxs_mutex_create(&listener_thread_guard);
	while (!is_thread_terminate() && loop_counter--) {
		dxs_sleep(100);
	}
	dxs_mutex_destroy(&listener_thread_guard);

	wprintf(L"Disconnecting from host...\n");

	if (!dxf_close_connection(connection)) {
		process_last_error();

		return 2;
	}

	wprintf(L"Disconnected\nQuote table test completed\n");

	return 0;
}
