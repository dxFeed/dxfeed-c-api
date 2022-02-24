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

int dxs_mutex_create(dxs_mutex_t *mutex) {
	*mutex = calloc(1, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(*mutex);
	return true;
}

int dxs_mutex_destroy(dxs_mutex_t *mutex) {
	DeleteCriticalSection(*mutex);
	free(*mutex);
	return true;
}

int dxs_mutex_lock(dxs_mutex_t *mutex) {
	EnterCriticalSection(*mutex);
	return true;
}

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

int dxs_mutex_destroy(dxs_mutex_t *mutex) {
	if (pthread_mutex_destroy(&mutex->mutex) != 0) {
		return false;
	}

	if (pthread_mutexattr_destroy(&mutex->attr) != 0) {
		return false;
	}

	return true;
}

int dxs_mutex_lock(dxs_mutex_t *mutex) {
	if (pthread_mutex_lock(&mutex->mutex) != 0) {
		return false;
	}

	return true;
}

int dxs_mutex_unlock(dxs_mutex_t *mutex) {
	if (pthread_mutex_unlock(&mutex->mutex) != 0) {
		return false;
	}

	return true;
}

#	define stricmp strcasecmp

#endif	//_WIN32

#define STRINGIFY(a) STR(a)
#define STR(a)		 #a

#define LS(s)  LS2(s)
#define LS2(s) L##s

// Prevents file names globbing (converting * to all files in the current dir)
#ifdef __MINGW64_VERSION_MAJOR
int _CRT_glob = 0;
#endif

// plus the name of the executable
#define STATIC_PARAMS_COUNT				4
#define DEFAULT_RECORDS_PRINT_LIMIT		7
#define RECORDS_PRINT_LIMIT_SHORT_PARAM "-l"
#define MAX_SOURCE_SIZE					42
#define TOKEN_PARAM_SHORT_TAG			"-T"
#define LOG_DATA_TRANSFER_TAG			"-p"
#define TIMEOUT_TAG						"-o"
#define TIME_PARAM_SHORT_TAG			"-t"

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
	(void)connection;
	char *host = (char *)user_data;
	dxs_mutex_lock(&listener_thread_guard);
	is_listener_thread_terminated = true;
	dxs_mutex_unlock(&listener_thread_guard);

	wprintf(L"\nTerminating listener thread, host: %hs\n", host);
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

	if (snapshot_data->event_type == DXF_ET_ORDER) {
		dxf_order_t *order_records = (dxf_order_t *)snapshot_data->records;
		for (i = 0; i < records_count; ++i) {
			dxf_order_t order = order_records[i];

			if (records_print_limit > 0 && i >= records_print_limit) {
				wprintf(L"   { ... %zu records left ...}\n", records_count - i);
				break;
			}

			wprintf(L"   {index=0x%llX, side=%i, scope=%i, time=", order.index, order.side, order.scope);
			print_timestamp(order.time);
			wprintf(L", exchange code=%c, market maker=%ls, price=%.15g, size=%.15g", order.exchange_code,
					order.market_maker, order.price, order.size);
			if (wcslen(order.source) > 0) wprintf(L", source=%ls", order.source);
			wprintf(L", count=%.15g, flags=0x%X}\n", order.count, order.event_flags);
		}
	} else if (snapshot_data->event_type == DXF_ET_CANDLE) {
		dxf_candle_t *candle_records = (dxf_candle_t *)snapshot_data->records;
		for (i = 0; i < snapshot_data->records_count; ++i) {
			dxf_candle_t candle = candle_records[i];

			if (records_print_limit > 0 && i >= records_print_limit) {
				wprintf(L"   { ... %zu records left ...}\n", records_count - i);
				break;
			}

			wprintf(L"   {time=");
			print_timestamp(candle.time);
			wprintf(
				L", sequence=%d, count=%.15g, open=%.15g, high=%.15g, low=%.15g, close=%.15g, volume=%.15g, "
				L"VWAP=%.15g, bidVolume=%.15g, askVolume=%.15g, flags=0x%X}\n",
				candle.sequence, candle.count, candle.open, candle.high, candle.low, candle.close, candle.volume,
				candle.vwap, candle.bid_volume, candle.ask_volume, candle.event_flags);
		}
	} else if (snapshot_data->event_type == DXF_ET_SPREAD_ORDER) {
		dxf_order_t *order_records = (dxf_order_t *)snapshot_data->records;
		for (i = 0; i < records_count; ++i) {
			dxf_order_t order = order_records[i];

			if (records_print_limit > 0 && i >= records_print_limit) {
				wprintf(L"   { ... %zu records left ...}\n", records_count - i);
				break;
			}

			wprintf(L"   {index=0x%llX, side=%i, scope=%i, time=", order.index, order.side, order.scope);
			print_timestamp(order.time);
			wprintf(
				L", sequence=%i, exchange code=%c, price=%.15g, size=%.15g, source=%ls, "
				L"count=%.15g, flags=0x%X, spread symbol=%ls}\n",
				order.sequence, order.exchange_code, order.price, order.size,
				wcslen(order.source) > 0 ? order.source : L"", order.count, order.event_flags,
				wcslen(order.spread_symbol) > 0 ? order.spread_symbol : L"");
		}
	} else if (snapshot_data->event_type == DXF_ET_TIME_AND_SALE) {
		dxf_time_and_sale_t *time_and_sale_records = (dxf_time_and_sale_t *)snapshot_data->records;
		for (i = 0; i < snapshot_data->records_count; ++i) {
			dxf_time_and_sale_t tns = time_and_sale_records[i];

			if (records_print_limit > 0 && i >= records_print_limit) {
				wprintf(L"   { ... %zu records left ...}\n", records_count - i);
				break;
			}

			wprintf(L"   {event id=%"LS(PRId64)L", time=%"LS(
					        PRId64)L", exchange code=%c, price=%.15g, size=%.15g, bid price=%.15g, ask price=%.15g, "
			        L"exchange sale conditions=\'%ls\', is ETH trade=%ls, type=%i, flags=0x%X}\n",
			        tns.index, tns.time, tns.exchange_code, tns.price, tns.size,
			        tns.bid_price, tns.ask_price, tns.exchange_sale_conditions,
			        tns.is_eth_trade ? L"True" : L"False", tns.type, tns.event_flags);
		}
	} else if (snapshot_data->event_type == DXF_ET_GREEKS) {
		dxf_greeks_t *greeks_records = (dxf_greeks_t *)snapshot_data->records;
		for (i = 0; i < snapshot_data->records_count; ++i) {
			dxf_greeks_t grks = greeks_records[i];

			if (records_print_limit > 0 && i >= records_print_limit) {
				wprintf(L"   { ... %zu records left ...}\n", records_count - i);
				break;
			}

			wprintf(L"   {time=");
			print_timestamp(grks.time);
			wprintf(L", index=0x%"LS(PRIX64)L", greeks price=%.15g, volatility=%.15g, "
			        L"delta=%.15g, gamma=%.15g, theta=%.15g, rho=%.15g, vega=%.15g, flags=0x%X}\n",
			        grks.index, grks.price, grks.volatility, grks.delta,
			        grks.gamma, grks.theta, grks.rho, grks.vega, grks.event_flags);
		}
	} else if (snapshot_data->event_type == DXF_ET_SERIES) {
		dxf_series_t *series_records = (dxf_series_t *)snapshot_data->records;
		for (i = 0; i < snapshot_data->records_count; ++i) {
			dxf_series_t srs = series_records[i];

			if (records_print_limit > 0 && i >= records_print_limit) {
				wprintf(L"   { ... %zu records left ...}\n", records_count - i);
				break;
			}
			wprintf(L"   {index=%" LS(PRId64) L", time=", srs.index);
			print_timestamp(srs.time);
			wprintf(
				L", sequence=%i, expiration=%d, volatility=%.15g, call volume=%.15g, put volume=%.15g, "
				L"option volume=%.15g, put call ratio=%.15g, forward_price=%.15g, dividend=%.15g, interest=%.15g, "
				L"index=0x%" LS(PRIX64) L", flags=0x%X}\n",
				srs.sequence, srs.expiration, srs.volatility, srs.call_volume, srs.put_volume, srs.option_volume,
				srs.put_call_ratio, srs.forward_price, srs.dividend, srs.interest, srs.index, srs.event_flags);
		}
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

#define DATE_TIME_BUF_SIZE 4
/*
 * Parse date string in format 'DD-MM-YYYY'
 */
int parse_date(const char *date_str, struct tm *time_struct) {
	size_t i;
	size_t date_string_len = strlen(date_str);
	int separator_count = 0;
	char buf[DATE_TIME_BUF_SIZE + 1] = {0};
	int mday = 0;
	int month = 0;
	int year = 0;

	for (i = 0; i < date_string_len; i++) {
		if (date_str[i] == '-') {
			if (separator_count == 0) {
				if (!atoi2(buf, &mday)) return false;
			} else if (separator_count == 1) {
				if (!atoi2(buf, &month)) return false;
			} else
				return false;
			separator_count++;
			memset(buf, 0, DATE_TIME_BUF_SIZE);
			continue;
		}

		size_t buf_len = strlen(buf);
		if (buf_len >= DATE_TIME_BUF_SIZE) return false;
		buf[buf_len] = date_str[i];
	}

	if (!atoi2(buf, &year)) return false;

	if (mday == 0 || month == 0 || year == 0) return false;

	time_struct->tm_mday = mday;
	time_struct->tm_mon = month - 1;
	time_struct->tm_year = year - 1900;

	return true;
}

int main(int argc, char *argv[]) {
	dxf_connection_t connection;
	dxf_snapshot_t snapshot;
	dxf_candle_attributes_t candle_attributes = NULL;
	char *event_type_name = NULL;
	dx_event_id_t event_id;
	dxf_string_t base_symbol = NULL;
	char *dxfeed_host = NULL;
	time_t time_value = time(NULL);
	struct tm time_struct;
	struct tm *local_time = localtime(&time_value);

	time_struct.tm_sec = 0;
	time_struct.tm_min = 0;
	time_struct.tm_hour = 0;
	time_struct.tm_mday = local_time->tm_mday;
	time_struct.tm_mon = local_time->tm_mon;
	time_struct.tm_year = local_time->tm_year;
	time_value = mktime(&time_struct);

	if (argc < STATIC_PARAMS_COUNT) {
		printf(
			"dxFeed snapshot command line sample\n"
			"===================================\n"
			"Demonstrates how to subscribe to historical data, get snapshots (time series)\n"
			"of events and a full order book.\n"
			"-------------------------------------------------------------------------------\n"
			"Usage: SnapshotConsoleSample <server address> <event type> <symbol>\n"
			"       [order_source] [" TIME_PARAM_SHORT_TAG " <DD-MM-YYYY>] "
			"[" RECORDS_PRINT_LIMIT_SHORT_PARAM " <records_print_limit>] [" TOKEN_PARAM_SHORT_TAG " <token>]\n"
			"       [" LOG_DATA_TRANSFER_TAG "] [" TIMEOUT_TAG " <timeout>]\n\n"
			"  <server address> - The dxFeed server address, e.g. demo.dxfeed.com:7300\n"
			"  <event type>     - The event type, one of the following: ORDER, CANDLE,\n"
			"                     SPREAD_ORDER, TIME_AND_SALE, GREEKS, SERIES\n"
			"  <symbol>         - The trade symbol, e.g. C, MSFT, YHOO, IBM\n"
			"  [order_source]   - a) source for Order (also can be empty), e.g. NTV, BYX,\n"
			"                        BZX, DEA, ISE, DEX, IST, ...\n"
			"                     b) source for MarketMaker, one of following: AGGREGATE_BID\n"
			"                        or AGGREGATE_ASK\n"
			"  " TIME_PARAM_SHORT_TAG " <DD-MM-YYYY>  - Time from which to receive data (default: current date and\n"
			"                     time)\n"
			"  " RECORDS_PRINT_LIMIT_SHORT_PARAM " <limit>       - The number of displayed records "
			"(0 - unlimited, default: " STRINGIFY(DEFAULT_RECORDS_PRINT_LIMIT)")\n"
			"  " TOKEN_PARAM_SHORT_TAG " <token>       - The authorization token\n"
			"  " LOG_DATA_TRANSFER_TAG "               - Enables the data transfer logging\n"
			"  " TIMEOUT_TAG " <timeout>     - Sets the program timeout in seconds (default = 604800,\n"
			"                     i.e a week)\n"
			"Example: demo.dxfeed.com:7300 ORDER IBM NTV -t 01-01-1970 -o 30\n\n"
		);

		return 0;
	}

	dxfeed_host = argv[1];
	event_type_name = argv[2];

	if (stricmp(event_type_name, "ORDER") == 0) {
		event_id = dx_eid_order;
	} else if (stricmp(event_type_name, "CANDLE") == 0) {
		event_id = dx_eid_candle;
	} else if (stricmp(event_type_name, "SPREAD_ORDER") == 0) {
		event_id = dx_eid_spread_order;
	} else if (stricmp(event_type_name, "TIME_AND_SALE") == 0) {
		event_id = dx_eid_time_and_sale;
	} else if (stricmp(event_type_name, "GREEKS") == 0) {
		event_id = dx_eid_greeks;
	} else if (stricmp(event_type_name, "SERIES") == 0) {
		event_id = dx_eid_series;
	} else {
		wprintf(L"Unknown event type.\n");

		return 1;
	}

	base_symbol = ansi_to_unicode(argv[3]);
	if (base_symbol == NULL) {
		return 2;
	}

	char *order_source_ptr = NULL;
	int records_print_limit = DEFAULT_RECORDS_PRINT_LIMIT;
	char *token = NULL;
	int log_data_transfer_flag = false;
	int program_timeout = 604800;  // a week
	int time_is_set = false;

	if (argc > STATIC_PARAMS_COUNT) {
		int records_print_limit_is_set = false;
		int token_is_set = false;
		int order_source_is_set = false;
		int program_timeout_is_set = false;

		for (int i = STATIC_PARAMS_COUNT; i < argc; i++) {
			if (time_is_set == false && strcmp(argv[i], TIME_PARAM_SHORT_TAG) == 0) {
				if (i + 1 == argc) {
					wprintf(L"Date argument error\n");
					return 3;
				}

				i += 1;

				if (!parse_date(argv[i], &time_struct)) {
					wprintf(L"Date format error\n");
					return 4;
				}

				time_value = mktime(&time_struct);
				time_is_set = true;
			} else if (records_print_limit_is_set == false && strcmp(argv[i], RECORDS_PRINT_LIMIT_SHORT_PARAM) == 0) {
				if (i + 1 == argc) {
					wprintf(L"The records print limit argument error\n");

					return 5;
				}

				int new_records_print_limit = -1;

				if (!atoi2(argv[++i], &new_records_print_limit)) {
					wprintf(L"The records print limit argument parsing error\n");

					return 6;
				}

				records_print_limit = new_records_print_limit;
				records_print_limit_is_set = true;
			} else if (token_is_set == false && strcmp(argv[i], TOKEN_PARAM_SHORT_TAG) == 0) {
				if (i + 1 == argc) {
					wprintf(L"The token argument error\n");

					return 7;
				}

				token = argv[++i];
				token_is_set = true;
			} else if (log_data_transfer_flag == false && strcmp(argv[i], LOG_DATA_TRANSFER_TAG) == 0) {
				log_data_transfer_flag = true;
			} else if (program_timeout_is_set == false && strcmp(argv[i], TIMEOUT_TAG) == 0) {
				if (i + 1 == argc) {
					wprintf(L"The program timeout argument error\n");

					return 8;
				}

				int new_program_timeout = -1;

				if (!atoi2(argv[++i], &new_program_timeout)) {
					wprintf(L"The program timeout argument parsing error\n");

					return 9;
				}

				program_timeout = new_program_timeout;
				program_timeout_is_set = true;
			} else if (order_source_is_set == false) {
				size_t string_len = 0;
				string_len = strlen(argv[i]);

				if (string_len > MAX_SOURCE_SIZE) {
					wprintf(L"Invalid order source param!\n");

					return 10;
				}

				char order_source[MAX_SOURCE_SIZE + 1] = {0};

				strcpy(order_source, argv[i]);
				order_source_ptr = &(order_source[0]);
				order_source_is_set = true;
			}
		}
	}

	dxf_initialize_logger_v2("snapshot-console-api.log", true, true, true, log_data_transfer_flag);
	dxs_mutex_create(&listener_thread_guard);

	wprintf(L"Snapshot console sample started.\n");
	wprintf(L"Connecting to host %hs...\n", dxfeed_host);

	ERRORCODE connection_result;
	if (token != NULL && token[0] != '\0') {
		connection_result = dxf_create_connection_auth_bearer(dxfeed_host, token, on_reader_thread_terminate, NULL,
															  NULL, NULL, NULL, &connection);
	} else {
		connection_result =
			dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, NULL, &connection);
	}

	if (connection_result == DXF_FAILURE) {
		free(base_symbol);
		process_last_error();

		return 20;
	}

	wprintf(L"Connected\n");

	if (event_id == dx_eid_candle) {
		if (!dxf_create_candle_symbol_attributes(base_symbol, DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT,
												 DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT, dxf_ctpa_default,
												 dxf_cpa_default, dxf_csa_default, dxf_caa_default,
												 DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, &candle_attributes)) {
			free(base_symbol);
			process_last_error();
			dxf_close_connection(connection);

			return 30;
		}

		if (!dxf_create_candle_snapshot(connection, candle_attributes, time_is_set ? time_value * 1000 : 0, &snapshot)) {
			free(base_symbol);
			process_last_error();
			dxf_delete_candle_symbol_attributes(candle_attributes);
			dxf_close_connection(connection);

			return 31;
		}
	} else if (event_id == dx_eid_order) {
		if (!dxf_create_order_snapshot(connection, base_symbol, order_source_ptr, 0, &snapshot)) {
			free(base_symbol);
			process_last_error();
			dxf_close_connection(connection);

			return 32;
		}
	} else {
		if (!dxf_create_snapshot(connection, event_id, base_symbol, NULL, time_is_set ? time_value * 1000 : 0, &snapshot)) {
			free(base_symbol);
			process_last_error();
			dxf_close_connection(connection);

			return 33;
		}
	}

	free(base_symbol);

	if (!dxf_attach_snapshot_listener(snapshot, listener, (void *)&records_print_limit)) {
		process_last_error();
		dxf_close_snapshot(snapshot);
		if (candle_attributes != NULL) dxf_delete_candle_symbol_attributes(candle_attributes);
		dxf_close_connection(connection);

		return 34;
	};

	wprintf(L"Subscribed\n");

	while (!is_thread_terminate() && program_timeout--) {
		dxs_sleep(1000);
	}

	if (!dxf_close_snapshot(snapshot)) {
		process_last_error();
		if (candle_attributes != NULL) dxf_delete_candle_symbol_attributes(candle_attributes);
		dxf_close_connection(connection);

		return 35;
	}

	if (candle_attributes != NULL) {
		if (!dxf_delete_candle_symbol_attributes(candle_attributes)) {
			process_last_error();
			dxf_close_connection(connection);

			return 36;
		}
	}

	wprintf(L"Disconnecting from host...\n");

	if (!dxf_close_connection(connection)) {
		process_last_error();

		return 21;
	}

	wprintf(L"Disconnected\n");

	dxs_mutex_destroy(&listener_thread_guard);

	return 0;
}
