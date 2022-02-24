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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "DXErrorCodes.h"
#include "DXFeed.h"
#include "Logger.h"

#ifndef _WIN32
typedef struct {
	int X;
	int Y;
} COORD;
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
HANDLE g_out_console;

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
// const char dxfeed_host[] = "localhost:5678";

#define SYMBOLS_COUNT 4
static const dxf_const_string_t g_symbols[] = {L"IBM", L"MSFT", L"YHOO", L"C"};
#define MAIN_LOOP_SLEEP_MILLIS 100

/* -------------------------------------------------------------------------- */
void trade_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					void* user_data);
void quote_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					void* user_data);
void summary_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					  void* user_data);
void profile_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					  void* user_data);
void order_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					void* user_data);
void time_and_sale_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data,
							int data_count, void* user_data);
void candle_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					 void* user_data);
void trade_eth_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
						void* user_data);
void spread_order_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
						   void* user_data);
void greeks_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					 void* user_data);
void theo_price_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
						 void* user_data);
void underlying_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
						 void* user_data);
void series_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					 void* user_data);
void configuration_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data,
							int data_count, void* user_data);
void snapshot_listener(dxf_snapshot_data_ptr_t snapshot_data, void* user_data);

struct event_info_t {
	dx_event_id_t id;
	int event_type;
	COORD coord;
	dxf_event_listener_t listener;
	unsigned int total_data_count[SYMBOLS_COUNT];
};

static struct event_info_t event_info[dx_eid_count] = {
	{dx_eid_trade, DXF_ET_TRADE, {0, 0}, trade_listener, {0}},
	{dx_eid_quote, DXF_ET_QUOTE, {22, 0}, quote_listener, {0}},
	{dx_eid_summary, DXF_ET_SUMMARY, {0, 6}, summary_listener, {0}},
	{dx_eid_profile, DXF_ET_PROFILE, {22, 6}, profile_listener, {0}},
	{dx_eid_order, DXF_ET_ORDER, {0, 12}, order_listener, {0}},
	{dx_eid_time_and_sale, DXF_ET_TIME_AND_SALE, {20, 12}, time_and_sale_listener, {0}},
	{dx_eid_candle, DXF_ET_CANDLE, {0, 18}, candle_listener, {0}},
	{dx_eid_trade_eth, DXF_ET_TRADE_ETH, {22, 18}, trade_eth_listener, {0}},
	{dx_eid_spread_order, DXF_ET_SPREAD_ORDER, {0, 24}, spread_order_listener, {0}},
	{dx_eid_greeks, DXF_ET_GREEKS, {22, 24}, greeks_listener, {0}},
	{dx_eid_theo_price, DXF_ET_THEO_PRICE, {0, 30}, theo_price_listener, {0}},
	{dx_eid_underlying, DXF_ET_UNDERLYING, {22, 30}, underlying_listener, {0}},
	{dx_eid_series, DXF_ET_SERIES, {0, 36}, series_listener, {0}},
	{dx_eid_configuration, DXF_ET_CONFIGURATION, {22, 36}, configuration_listener, {0}}};

struct snapshot_info_t {
	dx_event_id_t id;
	int event_type;
	const char source[100];
	COORD coord;
	dxf_snapshot_listener_t listener;
	size_t total_data_count[SYMBOLS_COUNT];
	dxf_snapshot_t obj[SYMBOLS_COUNT];
};

#define SNAPSHOT_COUNT 1
static struct snapshot_info_t snapshot_info[SNAPSHOT_COUNT] = {
	{dx_eid_order, DXF_ET_ORDER, "NTV", {0, 42}, snapshot_listener, {0}}};

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

#ifdef _WIN32
void print_at(COORD c, wchar_t* str) {
	DWORD dummy;
	SetConsoleCursorPosition(g_out_console, c);
	WriteConsoleW(g_out_console, str, (DWORD)wcslen(str), &dummy, NULL);
}
#else
void print_at(COORD c, wchar_t* str) {
	wprintf(L"\033[%d;%df", c.Y + 1, c.X + 1);
	wprintf(str);
}
#endif

void on_reader_thread_terminate(dxf_connection_t connection, void* user_data) {
	char* dxfeed_host = (char*)user_data;
	dxs_mutex_lock(&listener_thread_guard);
	is_listener_thread_terminated = true;
	dxs_mutex_unlock(&listener_thread_guard);

	wprintf(L"\nTerminating listener thread, host: %hs\n", dxfeed_host);
}

/* -------------------------------------------------------------------------- */

int get_symbol_index(dxf_const_string_t symbol_name) {
	int i = 0;
	for (; i < SYMBOLS_COUNT; ++i) {
		if (wcscmp(symbol_name, g_symbols[i]) == 0) {
			return i;
		}
	}

	return -1;
}

/* -------------------------------------------------------------------------- */

#define DXT_BUF_LEN 256

void trade_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					void* user_data) {
	wchar_t buf[DXT_BUF_LEN];
	COORD coord = event_info[dx_eid_trade].coord;

	if (event_type != DXF_ET_TRADE) {
		swprintf(buf, DXT_BUF_LEN, L"Error: event: %ls Symbol: %ls, expected event: Trade\n",
				 dx_event_type_to_string(event_type), symbol_name);
		print_at(coord, buf);
		return;
	}

	print_at(coord, L"Trade:        ");
	int ind = get_symbol_index(symbol_name);
	if (ind == -1) {
		return;
	}

	event_info[dx_eid_trade].total_data_count[ind] += data_count;

	coord.Y += ind + 1;
	swprintf(buf, DXT_BUF_LEN, L"%5ls: %6d/%6d", symbol_name, data_count,
			 event_info[dx_eid_trade].total_data_count[ind]);
	print_at(coord, buf);
}

/* -------------------------------------------------------------------------- */

void quote_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					void* user_data) {
	wchar_t buf[DXT_BUF_LEN];
	COORD coord = event_info[dx_eid_quote].coord;

	if (event_type != DXF_ET_QUOTE) {
		swprintf(buf, DXT_BUF_LEN, L"Error: event: %ls Symbol: %ls, expected event: Quote\n",
				 dx_event_type_to_string(event_type), symbol_name);
		print_at(coord, buf);
		return;
	}

	print_at(coord, L"Quote:        ");
	int ind = get_symbol_index(symbol_name);
	if (ind == -1) {
		return;
	}

	event_info[dx_eid_quote].total_data_count[ind] += data_count;

	coord.Y += ind + 1;
	swprintf(buf, DXT_BUF_LEN, L"%5ls: %6d/%6d", symbol_name, data_count,
			 event_info[dx_eid_quote].total_data_count[ind]);
	print_at(coord, buf);
}

/* -------------------------------------------------------------------------- */

void summary_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					  void* user_data) {
	wchar_t buf[DXT_BUF_LEN];
	COORD coord = event_info[dx_eid_summary].coord;

	if (event_type != DXF_ET_SUMMARY) {
		swprintf(buf, DXT_BUF_LEN, L"Error: event: %ls Symbol: %ls, expected event: Summary\n",
				 dx_event_type_to_string(event_type), symbol_name);
		print_at(coord, buf);
		return;
	}

	print_at(coord, L"Summary:        ");
	int ind = get_symbol_index(symbol_name);
	if (ind == -1) {
		return;
	}

	event_info[dx_eid_summary].total_data_count[ind] += data_count;

	coord.Y += ind + 1;
	swprintf(buf, DXT_BUF_LEN, L"%5ls: %6d/%6d", symbol_name, data_count,
			 event_info[dx_eid_summary].total_data_count[ind]);
	print_at(coord, buf);
}

/* -------------------------------------------------------------------------- */

void profile_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					  void* user_data) {
	wchar_t buf[DXT_BUF_LEN];
	COORD coord = event_info[dx_eid_profile].coord;

	if (event_type != DXF_ET_PROFILE) {
		swprintf(buf, DXT_BUF_LEN, L"Error: event: %ls Symbol: %ls, expected event: Profile\n",
				 dx_event_type_to_string(event_type), symbol_name);
		print_at(coord, buf);
		return;
	}

	print_at(coord, L"Profile:        ");
	int ind = get_symbol_index(symbol_name);
	if (ind == -1) {
		return;
	}

	event_info[dx_eid_profile].total_data_count[ind] += data_count;

	coord.Y += ind + 1;
	swprintf(buf, DXT_BUF_LEN, L"%5ls: %6d/%6d", symbol_name, data_count,
			 event_info[dx_eid_profile].total_data_count[ind]);
	print_at(coord, buf);
}

/* -------------------------------------------------------------------------- */

void order_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					void* user_data) {
	wchar_t buf[DXT_BUF_LEN];
	COORD coord = event_info[dx_eid_order].coord;

	if (event_type != DXF_ET_ORDER) {
		swprintf(buf, DXT_BUF_LEN, L"Error: event: %ls Symbol: %ls, expected event: Order\n",
				 dx_event_type_to_string(event_type), symbol_name);
		print_at(coord, buf);
		return;
	}

	print_at(coord, L"Order:        ");
	int ind = get_symbol_index(symbol_name);
	if (ind == -1) {
		return;
	}

	event_info[dx_eid_order].total_data_count[ind] += data_count;

	coord.Y += ind + 1;
	swprintf(buf, DXT_BUF_LEN, L"%5ls: %6d/%6d", symbol_name, data_count,
			 event_info[dx_eid_order].total_data_count[ind]);
	print_at(coord, buf);
}

/* -------------------------------------------------------------------------- */

void time_and_sale_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data,
							int data_count, void* user_data) {
	wchar_t buf[DXT_BUF_LEN];
	COORD coord = event_info[dx_eid_time_and_sale].coord;

	if (event_type != DXF_ET_TIME_AND_SALE) {
		swprintf(buf, DXT_BUF_LEN, L"Error: event: %ls Symbol: %ls, expected event: Time&Sale\n",
				 dx_event_type_to_string(event_type), symbol_name);
		print_at(coord, buf);
		return;
	}

	print_at(coord, L"Time&Sale:        ");
	int ind = get_symbol_index(symbol_name);
	if (ind == -1) {
		return;
	}

	event_info[dx_eid_time_and_sale].total_data_count[ind] += data_count;

	coord.Y += ind + 1;
	swprintf(buf, DXT_BUF_LEN, L"%5ls: %6d/%6d", symbol_name, data_count,
			 event_info[dx_eid_time_and_sale].total_data_count[ind]);
	print_at(coord, buf);
}

/* -------------------------------------------------------------------------- */

void candle_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					 void* user_data) {
	wchar_t buf[DXT_BUF_LEN];
	COORD coord = event_info[dx_eid_candle].coord;

	if (event_type != DXF_ET_CANDLE) {
		swprintf(buf, DXT_BUF_LEN, L"Error: event: %ls Symbol: %ls, expected event: Candle\n",
				 dx_event_type_to_string(event_type), symbol_name);
		print_at(coord, buf);
		return;
	}

	print_at(coord, L"Candle:        ");
	int ind = get_symbol_index(symbol_name);
	if (ind == -1) {
		return;
	}

	event_info[dx_eid_candle].total_data_count[ind] += data_count;

	coord.Y += ind + 1;
	swprintf(buf, DXT_BUF_LEN, L"%5ls: %6d/%6d", symbol_name, data_count,
			 event_info[dx_eid_candle].total_data_count[ind]);
	print_at(coord, buf);
}

/* -------------------------------------------------------------------------- */

void trade_eth_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
						void* user_data) {
	wchar_t buf[DXT_BUF_LEN];
	COORD coord = event_info[dx_eid_trade_eth].coord;

	if (event_type != DXF_ET_TRADE_ETH) {
		swprintf(buf, DXT_BUF_LEN, L"Error: event: %ls Symbol: %ls, expected event: TradeETH\n",
				 dx_event_type_to_string(event_type), symbol_name);
		print_at(coord, buf);
		return;
	}

	print_at(coord, L"TradeETH:        ");
	int ind = get_symbol_index(symbol_name);
	if (ind == -1) {
		return;
	}

	event_info[dx_eid_trade_eth].total_data_count[ind] += data_count;

	coord.Y += ind + 1;
	swprintf(buf, DXT_BUF_LEN, L"%5ls: %6d/%6d", symbol_name, data_count,
			 event_info[dx_eid_trade_eth].total_data_count[ind]);
	print_at(coord, buf);
}

/* -------------------------------------------------------------------------- */

void spread_order_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
						   void* user_data) {
	wchar_t buf[DXT_BUF_LEN];
	COORD coord = event_info[dx_eid_spread_order].coord;

	if (event_type != DXF_ET_SPREAD_ORDER) {
		swprintf(buf, DXT_BUF_LEN, L"Error: event: %ls Symbol: %ls, expected event: SpreadOrder\n",
				 dx_event_type_to_string(event_type), symbol_name);
		print_at(coord, buf);
		return;
	}

	print_at(coord, L"SpreadOrder:        ");
	int ind = get_symbol_index(symbol_name);
	if (ind == -1) {
		return;
	}

	event_info[dx_eid_spread_order].total_data_count[ind] += data_count;

	coord.Y += ind + 1;
	swprintf(buf, DXT_BUF_LEN, L"%5ls: %6d/%6d", symbol_name, data_count,
			 event_info[dx_eid_spread_order].total_data_count[ind]);
	print_at(coord, buf);
}

/* -------------------------------------------------------------------------- */

void greeks_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					 void* user_data) {
	wchar_t buf[DXT_BUF_LEN];
	COORD coord = event_info[dx_eid_greeks].coord;

	if (event_type != DXF_ET_GREEKS) {
		swprintf(buf, DXT_BUF_LEN, L"Error: event: %ls Symbol: %ls, expected event: Greeks\n",
				 dx_event_type_to_string(event_type), symbol_name);
		print_at(coord, buf);
		return;
	}

	print_at(coord, L"Greeks:        ");
	int ind = get_symbol_index(symbol_name);
	if (ind == -1) {
		return;
	}

	event_info[dx_eid_greeks].total_data_count[ind] += data_count;

	coord.Y += ind + 1;
	swprintf(buf, DXT_BUF_LEN, L"%5ls: %6d/%6d", symbol_name, data_count,
			 event_info[dx_eid_greeks].total_data_count[ind]);
	print_at(coord, buf);
}

/* -------------------------------------------------------------------------- */

void theo_price_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
						 void* user_data) {
	wchar_t buf[DXT_BUF_LEN];
	COORD coord = event_info[dx_eid_theo_price].coord;

	if (event_type != DXF_ET_THEO_PRICE) {
		swprintf(buf, DXT_BUF_LEN, L"Error: event: %ls Symbol: %ls, expected event: TheoPrice\n",
				 dx_event_type_to_string(event_type), symbol_name);
		print_at(coord, buf);
		return;
	}

	print_at(coord, L"TheoPrice:        ");
	int ind = get_symbol_index(symbol_name);
	if (ind == -1) {
		return;
	}

	event_info[dx_eid_theo_price].total_data_count[ind] += data_count;

	coord.Y += ind + 1;
	swprintf(buf, DXT_BUF_LEN, L"%5ls: %6d/%6d", symbol_name, data_count,
			 event_info[dx_eid_theo_price].total_data_count[ind]);
	print_at(coord, buf);
}

/* -------------------------------------------------------------------------- */

void underlying_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
						 void* user_data) {
	wchar_t buf[DXT_BUF_LEN];
	COORD coord = event_info[dx_eid_underlying].coord;

	if (event_type != DXF_ET_UNDERLYING) {
		swprintf(buf, DXT_BUF_LEN, L"Error: event: %ls Symbol: %ls, expected event: Underlying\n",
				 dx_event_type_to_string(event_type), symbol_name);
		print_at(coord, buf);
		return;
	}

	print_at(coord, L"Underlying:        ");
	int ind = get_symbol_index(symbol_name);
	if (ind == -1) {
		return;
	}

	event_info[dx_eid_underlying].total_data_count[ind] += data_count;

	coord.Y += ind + 1;
	swprintf(buf, DXT_BUF_LEN, L"%5ls: %6d/%6d", symbol_name, data_count,
			 event_info[dx_eid_underlying].total_data_count[ind]);
	print_at(coord, buf);
}

/* -------------------------------------------------------------------------- */

void series_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					 void* user_data) {
	wchar_t buf[DXT_BUF_LEN];
	COORD coord = event_info[dx_eid_series].coord;

	if (event_type != DXF_ET_SERIES) {
		swprintf(buf, DXT_BUF_LEN, L"Error: event: %ls Symbol: %ls, expected event: Series\n",
				 dx_event_type_to_string(event_type), symbol_name);
		print_at(coord, buf);
		return;
	}

	print_at(coord, L"Series:        ");
	int ind = get_symbol_index(symbol_name);
	if (ind == -1) {
		return;
	}

	event_info[dx_eid_series].total_data_count[ind] += data_count;

	coord.Y += ind + 1;
	swprintf(buf, DXT_BUF_LEN, L"%5ls: %6d/%6d", symbol_name, data_count,
			 event_info[dx_eid_series].total_data_count[ind]);
	print_at(coord, buf);
}

/* -------------------------------------------------------------------------- */

void configuration_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data,
							int data_count, void* user_data) {
	wchar_t buf[DXT_BUF_LEN];
	COORD coord = event_info[dx_eid_configuration].coord;

	if (event_type != DXF_ET_CONFIGURATION) {
		swprintf(buf, DXT_BUF_LEN, L"Error: event: %ls Symbol: %ls, expected event: Configuration\n",
				 dx_event_type_to_string(event_type), symbol_name);
		print_at(coord, buf);
		return;
	}

	print_at(coord, L"Configuration:        ");
	int ind = get_symbol_index(symbol_name);
	if (ind == -1) {
		return;
	}

	event_info[dx_eid_configuration].total_data_count[ind] += data_count;

	coord.Y += ind + 1;
	swprintf(buf, DXT_BUF_LEN, L"%5ls: %6d/%6d", symbol_name, data_count,
			 event_info[dx_eid_configuration].total_data_count[ind]);
	print_at(coord, buf);
}

/* -------------------------------------------------------------------------- */

void snapshot_listener(const dxf_snapshot_data_ptr_t snapshot_data, void* user_data) {
	wchar_t buf[DXT_BUF_LEN];
	struct snapshot_info_t* info = user_data;
	COORD coord = info->coord;

	if (snapshot_data->event_type != info->event_type) {
		swprintf(buf, DXT_BUF_LEN, L"Error: event: %ls Symbol: %ls, expected event: %ls\n",
				 dx_event_type_to_string(snapshot_data->event_type), snapshot_data->symbol,
				 dx_event_type_to_string(info->event_type));
		print_at(coord, buf);
		return;
	}

	swprintf(buf, DXT_BUF_LEN, L"Snapshot %ls#%hs:        ", dx_event_type_to_string(info->event_type), info->source);
	print_at(coord, buf);
	int ind = get_symbol_index(snapshot_data->symbol);
	if (ind == -1) {
		return;
	}

	info->total_data_count[ind] += snapshot_data->records_count;

	coord.Y += ind + 1;
	swprintf(buf, DXT_BUF_LEN, L"%5ls: %6zu/%10zu", snapshot_data->symbol, snapshot_data->records_count,
			 info->total_data_count[ind]);
	print_at(coord, buf);
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
	wchar_t buf[DXT_BUF_LEN];
	const int event_type = event_info[event_id].event_type;
	COORD coord = event_info[event_id].coord;

	swprintf(buf, DXT_BUF_LEN, L"%5ls: no data", dx_event_type_to_string(event_type));
	print_at(coord, buf);

	if (!dxf_create_subscription(connection, event_type, &subscription)) {
		process_last_error();

		return NULL;
	};

	for (int i = 0; i < SYMBOLS_COUNT; ++i) {
		COORD symbol_coord = coord;
		if (!dxf_add_symbol(subscription, g_symbols[i])) {
			process_last_error();

			return NULL;
		};

		swprintf(buf, DXT_BUF_LEN, L"%5ls: no data", g_symbols[i]);
		int ind = get_symbol_index(g_symbols[i]);
		if (ind == -1) {
			return NULL;
		}
		symbol_coord.Y += ind + 1;
		print_at(symbol_coord, buf);
	}

	if (!dxf_attach_event_listener(subscription, event_info[event_id].listener, NULL)) {
		process_last_error();

		return NULL;
	};

	return subscription;
}

void* create_snapshot_subscription(dxf_connection_t connection, struct snapshot_info_t* info) {
	wchar_t buf[DXT_BUF_LEN];
	COORD coord = info->coord;

	swprintf(buf, DXT_BUF_LEN, L"%ls snapshot: no data", dx_event_type_to_string(info->event_type));
	print_at(coord, buf);

	for (int i = 0; i < SYMBOLS_COUNT; ++i) {
		COORD symbol_coord = coord;
		dxf_candle_attributes_t candle_attributes = NULL;

		if (info->id == dx_eid_candle) {
			if (!dxf_create_candle_symbol_attributes(g_symbols[i], DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT,
													 DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT, dxf_ctpa_day,
													 dxf_cpa_default, dxf_csa_default, dxf_caa_default,
													 DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, &candle_attributes)) {
				process_last_error();
				return NULL;
			}

			if (!dxf_create_candle_snapshot(connection, candle_attributes, 0, &(info->obj[i]))) {
				process_last_error();
				dxf_delete_candle_symbol_attributes(candle_attributes);
				return NULL;
			};
		} else if (info->id == dx_eid_order) {
			if (!dxf_create_order_snapshot(connection, g_symbols[i], info->source, 0, &(info->obj[i]))) {
				process_last_error();
				return NULL;
			};
		} else {
			return NULL;
		}

		swprintf(buf, DXT_BUF_LEN, L"%5ls: no data", g_symbols[i]);
		int ind = get_symbol_index(g_symbols[i]);
		if (ind == -1) {
			return NULL;
		}
		symbol_coord.X += 1;
		symbol_coord.Y += ind + 1;
		print_at(symbol_coord, buf);

		if (!dxf_attach_snapshot_listener(info->obj[i], info->listener, info)) {
			process_last_error();

			return NULL;
		};
	}

	return info;
}
/* -------------------------------------------------------------------------- */
#ifdef _WIN32
int initialize_console() {
	CONSOLE_SCREEN_BUFFER_INFO info;
	COORD c = {80, 49};
	SMALL_RECT rect = {0, 0, 79, 48};
	DWORD buffer_size, dummy;

	g_out_console = GetStdHandle(STD_OUTPUT_HANDLE);
	if (g_out_console == NULL) {
		return false;
	}

	if (!GetConsoleScreenBufferInfo(g_out_console, &info)) {
		DWORD err = GetLastError();
		return false;
	}

	if (info.dwSize.X < 80 || info.dwSize.Y < 49) {
		if (!SetConsoleScreenBufferSize(g_out_console, c)) {
			return false;
		}
	}

	if (!SetConsoleWindowInfo(g_out_console, TRUE, &rect)) {
		return false;
	}

	buffer_size = info.dwSize.X * info.dwSize.Y;

	c.X = 0;
	c.Y = 0;
	if (!FillConsoleOutputCharacter(g_out_console, (TCHAR)' ', buffer_size, c, &dummy)) {
		return false;
	}

	///* get the current text attribute */
	// bSuccess = GetConsoleScreenBufferInfo( hConsole, &csbi );

	///* now set the buffer's attributes accordingly */

	// bSuccess = FillConsoleOutputAttribute( hConsole, csbi.wAttributes,
	//    dwConSize, coordScreen, &cCharsWritten );

	// put the cursor at (0, 0)
	if (!SetConsoleCursorPosition(g_out_console, c)) {
		return false;
	}

	return true;
}
#else
int initialize_console() {
	wprintf(L"\033[8;49;80t");	// set console size = 80x49
	wprintf(L"\033[2J");		// clear

	return true;
}
#endif

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

void print_usage() {
	wprintf(
		L"Usage: FullTest [<host>] [<timeout>] [-h|--help|-?]\n"
		L"  <host>       - dxfeed host (default: demo.dxfeed.com:7300)\n"
		L"  <timeout>    - timeout in seconds (default: 10000)\n"
		L"  -h|--help|-? - print usage\n\n");
}

int main(int argc, char* argv[]) {
	dxf_connection_t connection;
	dxf_subscription_t subscriptions[dx_eid_count];
	int loop_counter = 10000;
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

	if (argc >= 3) {
		// expect the second param in seconds
		// convert seconds time interval to loop number
		int timeout_seconds = 0;

		if (atoi2(argv[2], &timeout_seconds)) {
			loop_counter = timeout_seconds * 1000 / MAIN_LOOP_SLEEP_MILLIS;
		}
	}

	dxs_mutex_create(&listener_thread_guard);

	dxf_initialize_logger("full-test-api.log", true, true, true);

	if (!initialize_console()) {
		return 1;
	}

	if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, (void*)dxfeed_host,
							   &connection)) {
		process_last_error();

		return 10;
	}

	// create subscriptions
	for (int i = dx_eid_begin; i < dx_eid_count; ++i) {
		subscriptions[i] = create_subscription(connection, i);
		if (subscriptions[i] == NULL) {
			return 20;
		}
	}
	// create snapshots
	for (int i = 0; i < SNAPSHOT_COUNT; ++i) {
		if (create_snapshot_subscription(connection, &(snapshot_info[i])) == NULL) return 21;
	}

	while (!is_thread_terminate() && loop_counter--) {
		dxs_sleep(MAIN_LOOP_SLEEP_MILLIS);
	}

	if (!dxf_close_connection(connection)) {
		process_last_error();

		return 11;
	}

	wprintf(L"Disconnected\n");

	dxs_mutex_destroy(&listener_thread_guard);

	return 0;
}
