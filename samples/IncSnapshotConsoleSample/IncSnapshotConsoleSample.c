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

#define STRINGIFY(a) STR(a)
#define STR(a)		 #a

#define LS(s)  LS2(s)
#define LS2(s) L##s

#ifndef true

#	define true 1
#	define false 0
#endif

// plus the name of the executable
#define STATIC_PARAMS_COUNT				4
#define DEFAULT_RECORDS_PRINT_LIMIT		7
#define RECORDS_PRINT_LIMIT_SHORT_PARAM "-l"
#define MAX_SOURCE_SIZE					42
#define TOKEN_PARAM_SHORT_TAG			"-T"
#define LOG_DATA_TRANSFER_TAG			"-p"
#define TIMEOUT_TAG						"-o"

// Prevents file names globbing (converting * to all files in the current dir)
#ifdef __MINGW64_VERSION_MAJOR
int _CRT_glob = 0;
#endif

/* -------------------------------------------------------------------------- */
#ifdef _WIN32
static int is_listener_thread_terminated = false;
CRITICAL_SECTION listener_thread_guard;

int is_thread_terminate() {
	int res;
	EnterCriticalSection(&listener_thread_guard);
	res = is_listener_thread_terminated;
	LeaveCriticalSection(&listener_thread_guard);

	return res;
}
#else
static volatile int is_listener_thread_terminated = false;
int is_thread_terminate() {
	int res;
	res = is_listener_thread_terminated;
	return res;
}
#endif

/* -------------------------------------------------------------------------- */

#ifdef _WIN32
void on_reader_thread_terminate(dxf_connection_t connection, void* user_data) {
	EnterCriticalSection(&listener_thread_guard);
	is_listener_thread_terminated = true;
	LeaveCriticalSection(&listener_thread_guard);

	wprintf(L"\nTerminating listener thread\n");
}
#else
void on_reader_thread_terminate(dxf_connection_t connection, void* user_data) {
	is_listener_thread_terminated = true;
	wprintf(L"\nTerminating listener thread\n");
}
#endif

void print_timestamp(dxf_long_t timestamp) {
	wchar_t timefmt[80];

	struct tm* timeinfo;
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
			wprintf(L"no error information is stored");
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

/* -------------------------------------------------------------------------- */

void listener(const dxf_snapshot_data_ptr_t snapshot_data, int new_snapshot, void* user_data) {
	size_t i;
	size_t records_count = snapshot_data->records_count;
	int records_print_limit = DEFAULT_RECORDS_PRINT_LIMIT;

	if (user_data) {
		records_print_limit = *(int*)user_data;
	}

	wprintf(L"Snapshot %ls{symbol=%ls, records_count=%zu, type=%ls}\n",
			dx_event_type_to_string(snapshot_data->event_type), snapshot_data->symbol, records_count,
			new_snapshot ? L"full" : L"update");

	if (snapshot_data->event_type == DXF_ET_ORDER) {
		dxf_order_t* order_records = (dxf_order_t*)snapshot_data->records;
		for (i = 0; i < records_count; ++i) {
			dxf_order_t order = order_records[i];

			if (records_print_limit > 0 && i >= records_print_limit) {
				wprintf(L"   { ... %zu records left ...}\n", records_count - i);
				break;
			}

			if (!DXF_IS_ORDER_REMOVAL(&order)) {
				wprintf(L"   {index=0x%llX, side=%i, side=%i, time=", order.index, order.side, order.side);
				print_timestamp(order.time);
				wprintf(L", exchange code=%c, market maker=%ls, price=%.15g, size=%.15g", order.exchange_code,
						order.market_maker, order.price, order.size);
				if (wcslen(order.source) > 0) wprintf(L", source=%ls", order.source);
				wprintf(L", count=%.15g, flags=0x%X}\n", order.count, order.event_flags);
			} else {
				wprintf(L"   {index=0x%llX, REMOVAL}\n", order.index);
			}
		}
	} else if (snapshot_data->event_type == DXF_ET_CANDLE) {
		dxf_candle_t* candle_records = (dxf_candle_t*)snapshot_data->records;
		for (i = 0; i < snapshot_data->records_count; ++i) {
			dxf_candle_t candle = candle_records[i];

			if (records_print_limit > 0 && i >= records_print_limit) {
				wprintf(L"   { ... %zu records left ...}\n", records_count - i);
				break;
			}

			if (!DXF_IS_CANDLE_REMOVAL(&candle)) {
				wprintf(L"    {time=");
				print_timestamp(candle.time);
				wprintf(
					L", sequence=%d, count=%.15g, open=%.15g, high=%.15g, low=%.15g, close=%.15g, volume=%.15g, "
					L"VWAP=%.15g, bidVolume=%.15g, askVolume=%.15g, flags=0x%X}\n",
					candle.sequence, candle.count, candle.open, candle.high, candle.low, candle.close, candle.volume,
					candle.vwap, candle.bid_volume, candle.ask_volume, candle.event_flags);
			} else {
				wprintf(L"    {time=");
				print_timestamp(candle.time);
				wprintf(L", sequence=%d, REMOVAL}\n", candle.sequence);
			}
		}
	} else if (snapshot_data->event_type == DXF_ET_SPREAD_ORDER) {
		dxf_order_t* order_records = (dxf_order_t*)snapshot_data->records;
		for (i = 0; i < records_count; ++i) {
			dxf_order_t order = order_records[i];

			if (records_print_limit > 0 && i >= records_print_limit) {
				wprintf(L"   { ... %zu records left ...}\n", records_count - i);
				break;
			}

			if (!DXF_IS_SPREAD_ORDER_REMOVAL(&order)) {
				wprintf(L"   {index=0x%llX, side=%i, scope=%i, time=", order.index, order.side, order.scope);
				print_timestamp(order.time);
				wprintf(
					L", sequence=%i, exchange code=%c, price=%.15g, size=%.15g, source=%ls, "
					L"count=%.15g, flags=0x%X, spread symbol=%ls}\n",
					order.sequence, order.exchange_code, order.price, order.size,
					wcslen(order.source) > 0 ? order.source : L"", order.count, order.event_flags,
					wcslen(order.spread_symbol) > 0 ? order.spread_symbol : L"");
			} else {
				wprintf(L"   {index=0x%llX, REMOVAL}\n", order.index);
			}
		}
	} else if (snapshot_data->event_type == DXF_ET_TIME_AND_SALE) {
		dxf_time_and_sale_t* time_and_sale_records = (dxf_time_and_sale_t*)snapshot_data->records;
		for (i = 0; i < snapshot_data->records_count; ++i) {
			dxf_time_and_sale_t tns = time_and_sale_records[i];

			if (records_print_limit > 0 && i >= records_print_limit) {
				wprintf(L"   { ... %zu records left ...}\n", records_count - i);
				break;
			}

			if (!DXF_IS_TIME_AND_SALE_REMOVAL(&tns)) {
				wprintf(L"    {time=");
				print_timestamp(tns.time);
				wprintf(L", event id=%"LS(PRId64)L", exchange code=%c, price=%.15g, size=%.15g, bid price=%.15g, ask price=%.15g, "
					L"exchange sale conditions=\'%ls\', is ETH trade=%ls, type=%i, flags=0x%X}\n",
					tns.index, tns.exchange_code, tns.price, tns.size,
					tns.bid_price, tns.ask_price, tns.exchange_sale_conditions,
					tns.is_eth_trade ? L"True" : L"False", tns.type, tns.event_flags);
			} else {
				wprintf(L"    {time=");
				print_timestamp(tns.time);
				wprintf(L", event id=%" LS(PRId64) L", REMOVAL}\n", tns.index);
			}
		}
	} else if (snapshot_data->event_type == DXF_ET_GREEKS) {
		dxf_greeks_t* greeks_records = (dxf_greeks_t*)snapshot_data->records;
		for (i = 0; i < snapshot_data->records_count; ++i) {
			dxf_greeks_t grks = greeks_records[i];

			if (records_print_limit > 0 && i >= records_print_limit) {
				wprintf(L"   { ... %zu records left ...}\n", records_count - i);
				break;
			}

			if (!DXF_IS_GREEKS_REMOVAL(&grks)) {
				wprintf(L"    {time=");
				print_timestamp(grks.time);
				wprintf(L", index=0x%"LS(PRIX64)L", greeks price=%.15g, volatility=%.15g, "
					L"delta=%.15g, gamma=%.15g, theta=%.15g, rho=%.15g, vega=%.15g, flags=0x%X}\n",
					grks.index, grks.price, grks.volatility, grks.delta,
					grks.gamma, grks.theta, grks.rho, grks.vega, grks.event_flags);
			} else {
				wprintf(L"    {time=");
				print_timestamp(grks.time);
				wprintf(L", index=0x%" LS(PRIX64) L", REMOVAL}\n", grks.index);
			}
		}
	} else if (snapshot_data->event_type == DXF_ET_SERIES) {
		dxf_series_t* series_records = (dxf_series_t*)snapshot_data->records;
		for (i = 0; i < snapshot_data->records_count; ++i) {
			dxf_series_t srs = series_records[i];

			if (records_print_limit > 0 && i >= records_print_limit) {
				wprintf(L"   { ... %zu records left ...}\n", records_count - i);
				break;
			}
			if (!DXF_IS_SERIES_REMOVAL(&srs)) {
				wprintf(L"expiration=%d, index=0x%"LS(PRIX64)L", volatility=%.15g, call volume=%.15g, put volume=%.15g, "
					L"option volume=%.15g, put call ratio=%.15g, forward_price=%.15g, dividend=%.15g, interest=%.15g, flags=0x%X}\n",
					srs.expiration, srs.index, srs.volatility, srs.call_volume, srs.put_volume, srs.option_volume,
					srs.put_call_ratio, srs.forward_price, srs.dividend, srs.interest, srs.event_flags);
			} else {
				wprintf(L"    {expiration=%d, index=0x%" LS(PRIX64) L", REMOVAL}\n", srs.expiration, srs.index);
			}
		}
	}
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

/* -------------------------------------------------------------------------- */

int main(int argc, char* argv[]) {
	dxf_connection_t connection;
	dxf_snapshot_t snapshot;
	dxf_candle_attributes_t candle_attributes = NULL;
	char* event_type_name = NULL;
	dx_event_id_t event_id;
	dxf_string_t base_symbol = NULL;
	char* dxfeed_host = NULL;

	if (argc < STATIC_PARAMS_COUNT) {
		printf("DXFeed command line sample.\n"
			"Usage: IncSnapshotConsoleSample <server address> <event type> <symbol> [order_source] "
			"[" RECORDS_PRINT_LIMIT_SHORT_PARAM " <records_print_limit>] [" TOKEN_PARAM_SHORT_TAG " <token>] "
			"[" LOG_DATA_TRANSFER_TAG "] [" TIMEOUT_TAG " <timeout>]\n"
			"  <server address> - The DXFeed server address, e.g. demo.dxfeed.com:7300\n"
			"  <event type> - The event type, one of the following: ORDER, CANDLE, SPREAD_ORDER,\n"
			"                 TIME_AND_SALE, GREEKS, SERIES\n"
			"  <symbol> - The trade symbol, e.g. C, MSFT, YHOO, IBM\n"
			"  [order_source] - a) source for Order (also can be empty), e.g. NTV, BYX, BZX, DEA,\n"
			"                      ISE, DEX, IST, ...\n"
			"                   b) source for MarketMaker, one of following: AGGREGATE_BID or \n"
			"                      AGGREGATE_ASK\n"
			"  " RECORDS_PRINT_LIMIT_SHORT_PARAM " <records_print_limit> - The number of displayed records (0 - unlimited, default: " STRINGIFY(DEFAULT_RECORDS_PRINT_LIMIT) ")\n"
			"  " TOKEN_PARAM_SHORT_TAG " <token>               - The authorization token\n"
			"  " LOG_DATA_TRANSFER_TAG "                       - Enables the data transfer logging\n"
			"  " TIMEOUT_TAG " <timeout>             - Sets the program timeout in seconds (default = 604800, i.e a week)\n"
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
		return -1;
	}

	base_symbol = ansi_to_unicode(argv[3]);
	if (base_symbol == NULL) {
		return 1;
	}

	char* order_source_ptr = NULL;
	int records_print_limit = DEFAULT_RECORDS_PRINT_LIMIT;
	char* token = NULL;
	int log_data_transfer_flag = false;
	int program_timeout = 604800;  // a week

	if (argc > STATIC_PARAMS_COUNT) {
		int records_print_limit_is_set = false;
		int token_is_set = false;
		int order_source_is_set = false;
		int program_timeout_is_set = false;

		for (int i = STATIC_PARAMS_COUNT; i < argc; i++) {
			if (records_print_limit_is_set == false && strcmp(argv[i], RECORDS_PRINT_LIMIT_SHORT_PARAM) == 0) {
				if (i + 1 == argc) {
					wprintf(L"The records print limit argument error\n");

					return 2;
				}

				int new_records_print_limit = -1;

				if (!atoi2(argv[++i], &new_records_print_limit)) {
					wprintf(L"The records print limit argument parsing error\n");

					return 3;
				}

				records_print_limit = new_records_print_limit;
				records_print_limit_is_set = true;
			} else if (token_is_set == false && strcmp(argv[i], TOKEN_PARAM_SHORT_TAG) == 0) {
				if (i + 1 == argc) {
					wprintf(L"The token argument error\n");

					return 4;
				}

				token = argv[++i];
				token_is_set = true;
			} else if (log_data_transfer_flag == false && strcmp(argv[i], LOG_DATA_TRANSFER_TAG) == 0) {
				log_data_transfer_flag = true;
			} else if (program_timeout_is_set == false && strcmp(argv[i], TIMEOUT_TAG) == 0) {
				if (i + 1 == argc) {
					wprintf(L"The program timeout argument error\n");

					return 5;
				}

				int new_program_timeout = -1;

				if (!atoi2(argv[++i], &new_program_timeout)) {
					wprintf(L"The program timeout argument parsing error\n");

					return 6;
				}

				program_timeout = new_program_timeout;
				program_timeout_is_set = true;
			} else if (order_source_is_set == false) {	// this parsing block should be last
				size_t string_len = strlen(argv[i]);

				if (string_len > MAX_SOURCE_SIZE) {
					wprintf(L"Invalid order source param!\n");

					return 7;
				}

				char order_source[MAX_SOURCE_SIZE + 1] = {0};

				strcpy(order_source, argv[i]);
				order_source_ptr = &(order_source[0]);
				order_source_is_set = true;
			}
		}
	}

	dxf_initialize_logger_v2("inc-snapshot-console-api.log", true, true, true, log_data_transfer_flag);
	wprintf(L"Incremental snapshot sample started.\n");
	wprintf(L"Connecting to host %hs...\n", dxfeed_host);

#ifdef _WIN32
	InitializeCriticalSection(&listener_thread_guard);
#endif

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

		return 10;
	}

	wprintf(L"Connected\n");

	if (event_id == dx_eid_candle) {
		if (!dxf_create_candle_symbol_attributes(base_symbol, DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT,
												 DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT, dxf_ctpa_day,
												 dxf_cpa_default, dxf_csa_default, dxf_caa_default,
												 DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, &candle_attributes)) {
			free(base_symbol);
			process_last_error();
			dxf_close_connection(connection);

			return 20;
		}

		if (!dxf_create_candle_snapshot(connection, candle_attributes, 0, &snapshot)) {
			free(base_symbol);
			process_last_error();
			dxf_delete_candle_symbol_attributes(candle_attributes);
			dxf_close_connection(connection);

			return 21;
		}
	} else if (event_id == dx_eid_order) {
		if (!dxf_create_order_snapshot(connection, base_symbol, order_source_ptr, 0, &snapshot)) {
			free(base_symbol);
			process_last_error();
			dxf_close_connection(connection);

			return 22;
		}
	} else {
		if (!dxf_create_snapshot(connection, event_id, base_symbol, NULL, 0, &snapshot)) {
			free(base_symbol);
			process_last_error();
			dxf_close_connection(connection);

			return 23;
		}
	}

	free(base_symbol);

	if (!dxf_attach_snapshot_inc_listener(snapshot, listener, (void*)&records_print_limit)) {
		process_last_error();
		dxf_close_snapshot(snapshot);
		if (candle_attributes != NULL) dxf_delete_candle_symbol_attributes(candle_attributes);
		dxf_close_connection(connection);

		return 24;
	}

	wprintf(L"Subscribed\n");

	while (!is_thread_terminate() && program_timeout--) {
#ifdef _WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
	}

	if (!dxf_close_snapshot(snapshot)) {
		process_last_error();
		if (candle_attributes != NULL) dxf_delete_candle_symbol_attributes(candle_attributes);
		dxf_close_connection(connection);

		return 25;
	}

	if (candle_attributes != NULL) {
		if (!dxf_delete_candle_symbol_attributes(candle_attributes)) {
			process_last_error();
			dxf_close_connection(connection);

			return 26;
		}
	}

	wprintf(L"Disconnecting from host...\n");

	if (!dxf_close_connection(connection)) {
		process_last_error();

		return 11;
	}

	wprintf(L"Disconnected\n");

#ifdef _WIN32
	DeleteCriticalSection(&listener_thread_guard);
#endif

	return 0;
}
