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
#include <string.h>
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

const char dxfeed_host[] = "(demo.dxfeed.com:7300)(mddqa.in.devexperts.com:7400)";

#define TIMEOUT_TAG "-o"

static int is_listener_thread_terminated = false;
static dxs_mutex_t listener_thread_guard;

int is_thread_terminate() {
	int res;
	dxs_mutex_lock(&listener_thread_guard);
	res = is_listener_thread_terminated;
	dxs_mutex_unlock(&listener_thread_guard);

	return res;
}

void on_reader_thread_terminate(dxf_connection_t connection, void* user_data) {
	(void)connection;
	char* host = (char*)user_data;
	dxs_mutex_lock(&listener_thread_guard);
	is_listener_thread_terminated = true;
	dxs_mutex_unlock(&listener_thread_guard);

	wprintf(L"\nTerminating listener thread, host: %hs\n", host);
}

/* -------------------------------------------------------------------------- */
int events_counter = 0;
int server_lags_counter = 0;
dxf_long_t server_lags_sum = 0;
int doPrint = false;

void print_timestamp(dxf_long_t timestamp) {
	wchar_t timefmt[80];

	struct tm* timeinfo;
	time_t tmpint = (time_t)(timestamp / 1000);
	timeinfo = localtime(&tmpint);
	wcsftime(timefmt, 80, L"%Y%m%d-%H%M%S", timeinfo);
	wprintf(L"%ls", timefmt);
}

void listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
			  void* user_data) {
	(void)user_data;

	++events_counter;
	if (!doPrint) return;
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
		wprintf(L", high limit price=%.15g, low limit price=%.15g}\n", p->high_limit_price, p->low_limit_price);
	}

	if (event_type == DXF_ET_TIME_AND_SALE) {
		dxf_time_and_sale_t* tns = (dxf_time_and_sale_t*)data;

		wprintf(L"event id=%"LS(PRId64)L", time=%"LS(PRId64)L", exchange code=%c, price=%.15g, size=%.15g, bid price=%.15g, ask price=%.15g, "
				L"exchange sale conditions=%ls, is ETH trade=%ls, type=%i}\n",
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
dxf_string_t ansi_to_unicode(const char* ansi_str) {
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
#else  /* _WIN32 */
	dxf_string_t wide_str = NULL;
	size_t wide_size = mbstowcs(NULL, ansi_str, 0);	 // 0 is ignored

	if (wide_size > 0 && wide_size != (size_t)-1) {
		wide_str = calloc(wide_size + 1, sizeof(dxf_char_t));
		mbstowcs(wide_str, ansi_str, wide_size + 1);
	}

	return wide_str;
#endif /* _WIN32 */
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
	server_lags_counter++;
	server_lags_sum += server_lag_mark;
	fwprintf(stderr, L"\n##### Server time (UTC) = %" LS(PRId64) L" ms, Server lag = %d us, RTT = %d us #####\n",
			 server_millis, server_lag_mark, connection_rtt);
}

dxf_string_t* ansi_to_unicode_symbols(char** symbols, size_t count) {
	dxf_string_t* s = malloc(count * sizeof(dxf_string_t));

	for (size_t i = 0; i < count; ++i) {
		s[i] = ansi_to_unicode(symbols[i]);
	}

	return s;
}

void destroy_symbols(char** symbols, size_t count) {
	for (size_t i = 0; i < count; ++i) {
		free(symbols[i]);
	}

	free(symbols);
}

void destroy_unicode_symbols(dxf_string_t* symbols, size_t count) {
	for (size_t i = 0; i < count; ++i) {
		free(symbols[i]);
	}

	free(symbols);
}

/* -------------------------------------------------------------------------- */

void print_usage() {
	wprintf(L"Usage: PerformanceTest [print] [" LS(TIMEOUT_TAG)
		   L" <timeout>] [<IPF file> ... <IPF file>] [-h|--help|-?]\n"
		   L"  print        - Prints events\n"
		   L"  " LS(TIMEOUT_TAG)
		   L" <timeout> - Sets the program timeout in seconds (default = 3600, i.e a hour)\n"
		   L"  <IPF file>   - IPF file with symbols\n"
		   L"  -h|--help|-? - Prints usage\n\n");
}

#define LINE_SIZE	4096
#define SYMBOLS_MAX 10000000

int main(int argc, char* argv[]) {
	dxf_connection_t connection;
	dxf_subscription_t subscription;
	int program_timeout = 3600;	 // a hour
	int arg = 1;
	char line[LINE_SIZE];
	int symbols_pos = 0;
	time_t start, end;
	int diff_time;

	char** symbols = (char**)malloc(SYMBOLS_MAX * sizeof(char*));
	if (argc > 1) {	 // we have params
		if (strcmp(argv[arg], "print") == 0) {
			doPrint = true;
			++arg;
		} else if (strcmp(argv[arg], "-h") == 0 || strcmp(argv[arg], "-?") == 0 || strcmp(argv[arg], "--help") == 0) {
			print_usage();

			return 0;
		}

		if (argc > 3) {
			if (strcmp(argv[arg], TIMEOUT_TAG) == 0) {
				++arg;
				int new_program_timeout = -1;

				if (!atoi2(argv[arg], &new_program_timeout)) {
					wprintf(L"The program timeout argument parsing error: \"%s %s\"\n\n", argv[arg - 1], argv[arg]);
					print_usage();

					return 1;
				}

				program_timeout = new_program_timeout;
				++arg;
			}
		}

		for (size_t i = arg; i < argc; ++i) {
			FILE* file;
			file = fopen(argv[i], "rt");
			if (file == NULL) {
				wprintf(L"Couldn't open input file.");
				return 2;
			}

			while (fgets(line, LINE_SIZE, file) != NULL) {	// read symbols from IPF file
				if (line[0] == '#' || line[0] == '\r' || line[0] == '\n' || line[0] == '\0') continue;

				char* pch = strtok(line, ",");

				pch = strtok(NULL, ",");  // we need a second token (SYMBOL)

				if (symbols_pos < SYMBOLS_MAX) {
					symbols[symbols_pos] = strdup(pch);
					++symbols_pos;
				}
			}
			fclose(file);
		}
	} else {
		print_usage();

		return 0;
	}

	dxf_initialize_logger("performance-test-api.log", true, true, true);
	dxs_mutex_create(&listener_thread_guard);

	wprintf(L"Performance test started.\n");
	wprintf(L"Connecting to host %hs...\n", dxfeed_host);

	if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, (void*)dxfeed_host,
							   &connection)) {
		process_last_error();
		destroy_symbols(symbols, symbols_pos);

		return 10;
	}
	dxf_set_on_server_heartbeat_notifier(connection, on_server_heartbeat_notifier, NULL);

	wprintf(L"Connected\n");
	time(&start);

	int event_types = DXF_ET_TRADE | DXF_ET_QUOTE | DXF_ET_ORDER | DXF_ET_SUMMARY | DXF_ET_PROFILE;

	if (!dxf_create_subscription(connection, event_types, &subscription)) {
		process_last_error();
		dxf_close_connection(connection);
		destroy_symbols(symbols, symbols_pos);

		return 20;
	}

	if (!dxf_attach_event_listener(subscription, listener, NULL)) {
		process_last_error();
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);
		destroy_symbols(symbols, symbols_pos);

		return 21;
	}

	if (symbols_pos > 0) {
		dxf_string_t* s = ansi_to_unicode_symbols(symbols, symbols_pos);

		destroy_symbols(symbols, symbols_pos);

		if (!dxf_add_symbols(subscription, (dxf_const_string_t*)s, symbols_pos)) {
			process_last_error();
			dxf_close_subscription(subscription);
			dxf_close_connection(connection);

			destroy_unicode_symbols(s, symbols_pos);

			return 22;
		}

		destroy_unicode_symbols(s, symbols_pos);
	}

	wprintf(L"Subscribed\n");

	while (!is_thread_terminate() && program_timeout--) {
		dxs_sleep(1000);
	}

	wprintf(L"Disconnecting from host...\n");

	if (!dxf_close_connection(connection)) {
		process_last_error();

		return 11;
	}

	time(&end);
	diff_time = (int)difftime(end, start);

	wprintf(L"Disconnected\nConnection test completed\n");

	double events_speed = (double)events_counter / diff_time;
	double average_server_lag = (double)server_lags_sum / server_lags_counter;

	wprintf(L"Received %i events in %i sec. %0.2f events in 1 sec\n", events_counter, diff_time, events_speed);
	wprintf(L"Average server lag (us) = %0.2f\n", average_server_lag);
	wprintf(L"Average server lag by event (us/event) = %0.2f\n", average_server_lag / events_counter);
	dxs_mutex_destroy(&listener_thread_guard);

	return 0;
}
