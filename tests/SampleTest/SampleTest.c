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

#ifdef _WIN32
#	ifndef _CRT_STDIO_ISO_WIDE_SPECIFIERS
#		define _CRT_STDIO_ISO_WIDE_SPECIFIERS 1
#	endif
#endif

#include <inttypes.h>
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

#define LS(s)  LS2(s)
#define LS2(s) L##s

#ifndef true

#	define true 1
#	define false 0
#endif

// const char dxfeed_host[] = "mddqa.in.devexperts.com:7400";
const char dxfeed_host[] = "demo.dxfeed.com:7300";

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
	dxf_int_t i = 0;

	wprintf(L"%ls{symbol=%ls, ", dx_event_type_to_string(event_type), symbol_name);

	if (event_type == DXF_ET_QUOTE) {
		dxf_quote_t* quotes = (dxf_quote_t*)data;

		for (; i < data_count; ++i) {
			wprintf(L"bidTime=");
			print_timestamp(quotes[i].bid_time);
			wprintf(L" bidExchangeCode=%c, bidPrice=%f, bidSize=%i, ", quotes[i].bid_exchange_code, quotes[i].bid_price,
					quotes[i].bid_size);
			wprintf(L"askTime=");
			print_timestamp(quotes[i].ask_time);
			wprintf(L" askExchangeCode=%c, askPrice=%f, askSize=%i, scope=%d}\n", quotes[i].ask_exchange_code,
					quotes[i].ask_price, quotes[i].ask_size, (int)quotes[i].scope);
		}
	}

	if (event_type == DXF_ET_ORDER) {
		dxf_order_t* orders = (dxf_order_t*)data;

		for (; i < data_count; ++i) {
			wprintf(L"index=0x%" LS(PRIX64) L", side=%i, scope=%i, time=", orders[i].index, orders[i].side,
					orders[i].scope);
			print_timestamp(orders[i].time);
			wprintf(L", exchange code=%c, market maker=%ls, price=%f, size=%i}\n", orders[i].exchange_code,
					orders[i].market_maker, orders[i].price, orders[i].size);
		}
	}

	if (event_type == DXF_ET_TRADE) {
		dxf_trade_t* trades = (dxf_trade_t*)data;

		for (; i < data_count; ++i) {
			print_timestamp(trades[i].time);
			wprintf(L", exchangeCode=%c, price=%f, size=%i, tick=%i, change=%f, day id=%d, day volume=%.0f, scope=%d}\n",
					trades[i].exchange_code, trades[i].price, trades[i].size, trades[i].tick, trades[i].change,
					trades[i].day_id, trades[i].day_volume, (int)trades[i].scope);
		}
	}

	if (event_type == DXF_ET_SUMMARY) {
		dxf_summary_t* s = (dxf_summary_t*)data;

		for (; i < data_count; ++i) {
			wprintf(
				L"day high price=%f, day low price=%f, day open price=%f, prev day close price=%f, open interest=%i}\n",
				s[i].day_high_price, s[i].day_low_price, s[i].day_open_price, s[i].prev_day_close_price,
				s[i].open_interest);
		}
	}

	if (event_type == DXF_ET_PROFILE) {
		dxf_profile_t* p = (dxf_profile_t*)data;

		for (; i < data_count; ++i) {
			wprintf(
				L"Beta=%f, eps=%f, div freq=%i, exd div amount=%f, exd div date=%i, 52 high price=%f, "
				L"52 low price=%f, shares=%f, Description=%ls, flags=%i, status_reason=%ls, halt start time=",
				p[i].beta, p[i].eps, p[i].div_freq, p[i].exd_div_amount, p[i].exd_div_date, p[i].high_52_week_price,
				p[i].low_52_week_price, p[i].shares, p[i].description, p[i].raw_flags, p[i].status_reason);
			print_timestamp(p[i].halt_start_time);
			wprintf(L", halt end time=");
			print_timestamp(p[i].halt_end_time);
			wprintf(L", high limit price=%f, low limit price=%f}\n", p[i].high_limit_price, p[i].low_limit_price);
		}
	}

	if (event_type == DXF_ET_TIME_AND_SALE) {
		dxf_time_and_sale_t* tns = (dxf_time_and_sale_t*)data;

		for (; i < data_count; ++i) {
			wprintf(L"event id=%"LS(PRId64)L", time=%"LS(PRId64)L", exchange code=%c, price=%f, size=%i, bid price=%f, ask price=%f, "
					L"exchange sale conditions=\'%ls\', is ETH trade=%ls, type=%i}\n",
					tns[i].index, tns[i].time, tns[i].exchange_code, tns[i].price, tns[i].size,
					tns[i].bid_price, tns[i].ask_price, tns[i].exchange_sale_conditions,
					tns[i].is_eth_trade ? L"True" : L"False", tns[i].type);
		}
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

/* -------------------------------------------------------------------------- */

int main(int argc, char* argv[]) {
	dxf_connection_t connection;
	dxf_subscription_t subscription;
	int loop_counter = 30;	// seconds

	dxf_initialize_logger("sample-test-api.log", true, true, true);
	dxs_mutex_create(&listener_thread_guard);

	wprintf(L"Sample test started.\n");
	wprintf(L"Connecting to host %hs...\n", dxfeed_host);

	if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, (void*)dxfeed_host,
							   &connection)) {
		process_last_error();
		return 1;
	}

	wprintf(L"Connected\n");

	if (!dxf_create_subscription(
			connection, DXF_ET_TRADE | DXF_ET_QUOTE | DXF_ET_ORDER | DXF_ET_SUMMARY | DXF_ET_PROFILE, &subscription)) {
		process_last_error();

		return 10;
	};

	if (!dxf_attach_event_listener(subscription, listener, NULL)) {
		process_last_error();

		return 11;
	};

	if (!dxf_add_symbol(subscription, L"IBM")) {
		process_last_error();

		return 12;
	};

	wprintf(L"Subscribed\n");

	while (!is_thread_terminate() && loop_counter--) {
		dxs_sleep(1000);
	}

	wprintf(L"Disconnecting from host...\n");

	if (!dxf_close_connection(connection)) {
		process_last_error();

		return 2;
	}

	wprintf(L"Disconnected\nSample test completed\n");
	dxs_mutex_destroy(&listener_thread_guard);

	return 0;
}
