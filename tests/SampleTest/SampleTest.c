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

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

		wprintf(L"index=0x%" LS(PRIX64) L", side=%i, scope=%i, time=", o->index, o->side, o->scope);
		print_timestamp(o->time);
		wprintf(L", exchange code=%c, market maker=%ls, price=%.15g, size=%.15g, count=%.15g}\n", o->exchange_code,
				o->market_maker, o->price, o->size, o->count);
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
			L"day high price=%.15g, day low price=%.15g, day open price=%.15g, prev day close price=%.15g, open "
			L"interest=%.15g}\n",
			s->day_high_price, s->day_low_price, s->day_open_price, s->prev_day_close_price, s->open_interest);
	}

	if (event_type == DXF_ET_PROFILE) {
		dxf_profile_t* p = (dxf_profile_t*)data;

		wprintf(
			L"Beta=%.15g, eps=%.15g, div freq=%.15g, exd div amount=%.15g, exd div date=%i, 52 high price=%.15g, "
			L"52 low price=%.15g, shares=%.15g, Description=%ls, flags=%i, status_reason=%ls, halt start time=",
			p->beta, p->eps, p->div_freq, p->exd_div_amount, p->exd_div_date, p->high_52_week_price,
			p->low_52_week_price, p->shares, p->description, p->raw_flags, p->status_reason);
		print_timestamp(p->halt_start_time);
		wprintf(L", halt end time=");
		print_timestamp(p->halt_end_time);
		wprintf(L", high limit price=%f, low limit price=%f}\n", p->high_limit_price, p->low_limit_price);
	}

	if (event_type == DXF_ET_TIME_AND_SALE) {
		dxf_time_and_sale_t* tns = (dxf_time_and_sale_t*)data;

		wprintf(L"event id=%"LS(PRId64)L", time=%"LS(PRId64)L", exchange code=%c, price=%.15g, size=%.15g, bid price=%.15g, ask price=%.15g, "
					L"exchange sale conditions=\'%ls\', is ETH trade=%ls, type=%i}\n",
					tns->index, tns->time, tns->exchange_code, tns->price, tns->size,
					tns->bid_price, tns->ask_price, tns->exchange_sale_conditions,
					tns->is_eth_trade ? L"True" : L"False", tns->type);
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
