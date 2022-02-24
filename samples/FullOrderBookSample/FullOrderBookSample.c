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
#	pragma warning(push)
#	pragma warning(disable : 5105)
#	include <Windows.h>
#	pragma warning(pop)
#else

#	include <unistd.h>
#	include <string.h>
#	include <wctype.h>
#	include <stdlib.h>

#	define stricmp strcasecmp
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
#	define USE_WIN32_THREADS
typedef HANDLE dxs_thread_t;
typedef DWORD dxs_key_t;
typedef LPCRITICAL_SECTION dxs_mutex_t;
#endif /* !defined(_WIN32) || defined(USE_PTHREADS) */

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

#include "DXErrorCodes.h"
#include "DXFeed.h"

#define STRINGIFY(a) STR(a)
#define STR(a)		 #a

#define LS(s)  LS2(s)
#define LS2(s) L##s

#ifndef true
#	define true 1
#	define false 0
#endif

#ifdef _WIN32
// To fix problem with MS implementation of swprintf
#	define swprintf _snwprintf

void dxs_sleep(int milliseconds) { Sleep((DWORD)milliseconds); }

int dxs_mutex_create(dxs_mutex_t *mutex) {
	*mutex = calloc(1, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(*mutex);
	return true;
}

/* -------------------------------------------------------------------------- */

int dxs_mutex_destroy(dxs_mutex_t *mutex) {
	DeleteCriticalSection(*mutex);
	free(*mutex);
	return true;
}

/* -------------------------------------------------------------------------- */

int dxs_mutex_lock(dxs_mutex_t *mutex) {
	EnterCriticalSection(*mutex);
	return true;
}

/* -------------------------------------------------------------------------- */

int dxs_mutex_unlock(dxs_mutex_t *mutex) {
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

int dxs_mutex_create(dxs_mutex_t *mutex) {
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

int dxs_mutex_destroy(dxs_mutex_t *mutex) {
	if (pthread_mutex_destroy(&mutex->mutex) != 0) {
		return false;
	}

	if (pthread_mutexattr_destroy(&mutex->attr) != 0) {
		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

int dxs_mutex_lock(dxs_mutex_t *mutex) {
	if (pthread_mutex_lock(&mutex->mutex) != 0) {
		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

int dxs_mutex_unlock(dxs_mutex_t *mutex) {
	if (pthread_mutex_unlock(&mutex->mutex) != 0) {
		return false;
	}

	return true;
}

#endif	//_WIN32

// Prevents file names globbing (converting * to all files in the current dir)
#ifdef __MINGW64_VERSION_MAJOR
int _CRT_glob = 0;
#endif

// plus the name of the executable
#define STATIC_PARAMS_COUNT				3
#define DEFAULT_RECORDS_PRINT_LIMIT		7
#define RECORDS_PRINT_LIMIT_SHORT_PARAM "-l"
#define MAX_SOURCE_SIZE					42
#define TOKEN_PARAM_SHORT_TAG			"-T"
#define LOG_DATA_TRANSFER_TAG			"-p"
#define TIMEOUT_TAG						"-o"

static int is_listener_thread_terminated = false;
static dxs_mutex_t listener_thread_guard;

int is_thread_terminate() {
	int res;

	dxs_mutex_lock(&listener_thread_guard);
	res = is_listener_thread_terminated;
	dxs_mutex_unlock(&listener_thread_guard);

	return res;
}

void on_reader_thread_terminate(dxf_connection_t connection, void *user_data) {
	dxs_mutex_lock(&listener_thread_guard);
	is_listener_thread_terminated = true;
	dxs_mutex_unlock(&listener_thread_guard);

	wprintf(L"\nTerminating listener thread\n");
}

void print_timestamp(dxf_long_t timestamp) {
	wchar_t timefmt[80];

	struct tm *timeinfo;
	time_t tmpint = (time_t)(timestamp / 1000);
	timeinfo = localtime(&tmpint);
	wcsftime(timefmt, 80, L"%Y%m%d-%H%M%S", timeinfo);
	wprintf(L"%ls", timefmt);
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

dxf_string_t ansi_to_unicode(const char *ansi_str) {
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

/* -------------------------------------------------------------------------- */

void listener(const dxf_snapshot_data_ptr_t snapshot_data, void *user_data) {
	size_t i;
	size_t records_count = snapshot_data->records_count;
	int records_print_limit = DEFAULT_RECORDS_PRINT_LIMIT;

	if (user_data) {
		records_print_limit = *(int *)user_data;
	}

	wprintf(L"Snapshot %ls{symbol=%ls, records_count=%zu}\n", dx_event_type_to_string(snapshot_data->event_type),
			snapshot_data->symbol, records_count);

	dxf_order_t *order_records = (dxf_order_t *)snapshot_data->records;

	for (i = 0; i < records_count; ++i) {
		dxf_order_t order = order_records[i];

		if (records_print_limit > 0 && i >= records_print_limit) {
			wprintf(L"   { ... %zu records left ...}\n", records_count - i);
			break;
		}

		wprintf(L"   {index=0x%llX, side=%i, scope=%i, time=", order.index, order.side, order.scope);
		print_timestamp(order.time);
		wprintf(L", exchange code=%c, market maker=%ls, price=%.15g, size=%.15g, executed size=%.15g",
				order.exchange_code, order.market_maker, order.price, order.size, order.executed_size);
		if (wcslen(order.source) > 0) wprintf(L", source=%ls", order.source);
		wprintf(L", count=%.15g", order.count);

		if (order.action > dxf_oa_undefined && order.action <= dxf_oa_last) {
			wprintf(L", action=%ls, action time=%" LS(PRId64) "", dxf_get_order_action_wstring_name(order.action),
					order.action_time);

			switch (order.action) {
				case dxf_oa_new:
				case dxf_oa_delete:
					wprintf(L", order id=%" LS(PRId64) ", aux order id=%" LS(PRId64), order.order_id,
							order.aux_order_id);
					break;
				case dxf_oa_replace:
				case dxf_oa_modify:
					wprintf(L", order id=%" LS(PRId64) "", order.order_id);
					break;
				case dxf_oa_partial:
				case dxf_oa_execute:
					wprintf(L", order id=%" LS(PRId64) ", aux order id=%" LS(PRId64) ", trade id=%" LS(
								PRId64) ", trade size=%f, trade price = %f",
							order.order_id, order.aux_order_id, order.trade_id, order.trade_size, order.trade_price);
					break;
				case dxf_oa_trade:
				case dxf_oa_bust:
					wprintf(L"trade id=%" LS(PRId64) ", trade size=%f, trade price = %f", order.trade_id,
							order.trade_size, order.trade_price);
					break;
				case dxf_oa_undefined:
					break;
			}
		}
		wprintf(L"}\n");
	}
}

/* -------------------------------------------------------------------------- */

int atoi2(char *str, int *result) {
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

/* -------------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
	if (argc < STATIC_PARAMS_COUNT) {
		printf("DXFeed command line sample.\n"
			"Usage: FullOrderBookSample <server address> <symbol> "
			"[" RECORDS_PRINT_LIMIT_SHORT_PARAM " <records_print_limit>] [" TOKEN_PARAM_SHORT_TAG " <token>] "
			"[" LOG_DATA_TRANSFER_TAG "] [" TIMEOUT_TAG " <timeout>]\n"
			"  <server address> - The DXFeed server address, e.g. demo.dxfeed.com:7300\n"
			"  <symbol>         - The trade symbol, e.g. C, MSFT, YHOO, IBM\n"
			"  " RECORDS_PRINT_LIMIT_SHORT_PARAM " <limit>       - The number of displayed records "
			"(0 - unlimited, default: " STRINGIFY(DEFAULT_RECORDS_PRINT_LIMIT) ")\n"
			"  " TOKEN_PARAM_SHORT_TAG " <token>       - The authorization token\n"
			"  " LOG_DATA_TRANSFER_TAG "               - Enables the data transfer logging\n"
			"  " TIMEOUT_TAG " <timeout>     - Sets the program timeout in seconds (default = 604800, i.e a week)\n"
			"Example: FullOrderBookSample demo.dxfeed.com:7300 IBM"
		);

		return 0;
	}

	char *host = argv[1];
	dxf_string_t symbol = ansi_to_unicode(argv[2]);

	if (symbol == NULL) {
		return 2;
	}

	int records_print_limit = DEFAULT_RECORDS_PRINT_LIMIT;
	char *token = NULL;
	int log_data_transfer_flag = false;
	int program_timeout = 604800;  // a week

	if (argc > STATIC_PARAMS_COUNT) {
		int records_print_limit_is_set = false;
		int token_is_set = false;
		int program_timeout_is_set = false;

		for (int i = STATIC_PARAMS_COUNT; i < argc; i++) {
			if (records_print_limit_is_set == false && strcmp(argv[i], RECORDS_PRINT_LIMIT_SHORT_PARAM) == 0) {
				if (i + 1 == argc) {
					wprintf(L"The records print limit argument error\n");

					return 3;
				}

				int new_records_print_limit = -1;

				if (!atoi2(argv[++i], &new_records_print_limit)) {
					wprintf(L"The records print limit argument parsing error\n");

					return 4;
				}

				records_print_limit = new_records_print_limit;
				records_print_limit_is_set = true;
			} else if (token_is_set == false && strcmp(argv[i], TOKEN_PARAM_SHORT_TAG) == 0) {
				if (i + 1 == argc) {
					wprintf(L"The token argument error\n");

					return 5;
				}

				token = argv[++i];
				token_is_set = true;
			} else if (log_data_transfer_flag == false && strcmp(argv[i], LOG_DATA_TRANSFER_TAG) == 0) {
				log_data_transfer_flag = true;
			} else if (program_timeout_is_set == false && strcmp(argv[i], TIMEOUT_TAG) == 0) {
				if (i + 1 == argc) {
					wprintf(L"The program timeout argument error\n");

					return 6;
				}

				int new_program_timeout = -1;

				if (!atoi2(argv[++i], &new_program_timeout)) {
					wprintf(L"The program timeout argument parsing error\n");

					return 7;
				}

				program_timeout = new_program_timeout;
				program_timeout_is_set = true;
			}
		}
	}

	dxf_initialize_logger_v2("full-order-book-sample.log", true, true, true, log_data_transfer_flag);
	wprintf(L"Full order book (FOB) sample started.\n");
	wprintf(L"Connecting to host %hs...\n", host);

	dxf_connection_t connection;
	dxf_snapshot_t snapshot;

	dxs_mutex_create(&listener_thread_guard);

	ERRORCODE connection_result;
	if (token != NULL && token[0] != '\0') {
		connection_result = dxf_create_connection_auth_bearer(host, token, on_reader_thread_terminate, NULL, NULL, NULL,
															  NULL, &connection);
	} else {
		connection_result =
			dxf_create_connection(host, on_reader_thread_terminate, NULL, NULL, NULL, NULL, &connection);
	}

	if (connection_result == DXF_FAILURE) {
		free(symbol);
		process_last_error();

		return 10;
	}

	wprintf(L"Connected\n");

	if (!dxf_create_order_snapshot(connection, symbol, "NTV", 0, &snapshot)) {
		free(symbol);
		process_last_error();
		dxf_close_connection(connection);

		return 22;
	}

	free(symbol);

	if (!dxf_attach_snapshot_listener(snapshot, listener, (void *)&records_print_limit)) {
		process_last_error();
		dxf_close_snapshot(snapshot);
		dxf_close_connection(connection);

		return 24;
	};

	wprintf(L"Subscribed\n");

	while (!is_thread_terminate() && program_timeout--) {
		dxs_sleep(1000);
	}

	if (!dxf_close_snapshot(snapshot)) {
		process_last_error();
		dxf_close_connection(connection);

		return 25;
	}

	wprintf(L"Disconnecting from host...\n");

	if (!dxf_close_connection(connection)) {
		process_last_error();

		return 11;
	}

	wprintf(L"Disconnected\n");

	dxs_mutex_destroy(&listener_thread_guard);

	return 0;
}
