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

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define true 1
#define false 0

// plus the name of the executable
#define STATIC_PARAMS_COUNT 3
#define TOKEN_PARAM_SHORT_TAG "-T"
#define LOG_DATA_TRANSFER_TAG "-p"
#define TIMEOUT_TAG "-o"
#define MAX_SOURCE_SIZE 42
#define MAX_SOURCES 10

//Prevents file names globbing (converting * to all files in the current dir)
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
			wprintf(L"No error information is stored");
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
#else /* _WIN32 */
	dxf_string_t wide_str = NULL;
	size_t wide_size = mbstowcs(NULL, ansi_str, 0); // 0 is ignored

	if (wide_size > 0 && wide_size != (size_t)-1) {
		wide_str = calloc(wide_size + 1, sizeof(dxf_char_t));
		mbstowcs(wide_str, ansi_str, wide_size + 1);
	}

	return wide_str;
#endif /* _WIN32 */
}

/* -------------------------------------------------------------------------- */

void listener(const dxf_price_level_book_data_ptr_t book_data, void* user_data) {
	size_t i = 0;
	wprintf(L"New Price Level Order Book for %ls:\n", book_data->symbol);
	/* Time is 4 + 2 + 2 + 1 + 2 + 2 + 2 = 15 */
	wprintf(L" %-7ls %-8ls %-15ls |  %-7ls %-8ls %-15ls\n", L"Ask", L"Size", L"Time", L"Bid", L"Size", L"Time");
	for (; i < MAX(book_data->asks_count, book_data->bids_count); i++) {
		if (i < book_data->asks_count) {
			wprintf(L"$%-7.2f %-8lld ", book_data->asks[i].price, book_data->asks[i].size);
			print_timestamp(book_data->asks[i].time);
		} else {
			wprintf(L" %-7ls %-8ls %-15ls", L"", L"", L"");
		}
		wprintf(L" | ");
		if (i < book_data->bids_count) {
			wprintf(L"$%-7.2f %-8lld ", book_data->bids[i].price, book_data->bids[i].size);
			print_timestamp(book_data->bids[i].time);
		}
		wprintf(L"\n");
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

static const char *default_sources[] = { "BZX", "DEX", NULL };

/* -------------------------------------------------------------------------- */

int main(int argc, char* argv[]) {
	dxf_connection_t connection;
	dxf_price_level_book_t book;
	dxf_string_t base_symbol = NULL;
	char* dxfeed_host = NULL;

	if (argc < STATIC_PARAMS_COUNT) {
		printf("DXFeed Price Level Book command line sample.\n"
			"Usage: PriceLevelBookSample <server address> <symbol> [order_source] [" TOKEN_PARAM_SHORT_TAG " <token>] "
			"[" LOG_DATA_TRANSFER_TAG "] [" TIMEOUT_TAG " <timeout>]\n"
			"  <server address> - The DXFeed server address, e.g. demo.dxfeed.com:7300\n"
			"  <symbol>         - The trade symbol, e.g. C, MSFT, YHOO, IBM\n"
			"  [order_source]   - One or more order sources, e.g.. NTV, BYX, BZX, DEA,\n"
			"                     ISE, DEX, IST. Default is BZX DEX\n"
			"  " TOKEN_PARAM_SHORT_TAG " <token>       - The authorization token\n"
			"  " LOG_DATA_TRANSFER_TAG "               - Enables the data transfer logging\n"
			"  " TIMEOUT_TAG " <timeout>     - Sets the program timeout in seconds (default = 604800, i.e a week)\n"
			);
		return 0;
	}

	dxfeed_host = argv[1];
	base_symbol = ansi_to_unicode(argv[2]);
	if (base_symbol == NULL) {
		return 1;
	}

	const char **order_sources_ptr = NULL;
	char order_source[MAX_SOURCE_SIZE + 1] = { 0 };
	const char* order_sources[MAX_SOURCES + 1] = {NULL};
	char* token = NULL;
	int order_source_is_set = false;
	int log_data_transfer_flag = false;
	int program_timeout = 604800; // a week

	if (argc > STATIC_PARAMS_COUNT) {
		int token_is_set = false;
		int program_timeout_is_set = false;

		for (int i = STATIC_PARAMS_COUNT; i < argc; i++) {
			if (token_is_set == false && strcmp(argv[i], TOKEN_PARAM_SHORT_TAG) == 0) {
				if (i + 1 == argc) {
					wprintf(L"Token argument error\n");

					return 2;
				}

				token = argv[++i];
				token_is_set = true;
			} else if (log_data_transfer_flag == false && strcmp(argv[i], LOG_DATA_TRANSFER_TAG) == 0) {
				log_data_transfer_flag = true;
			} else if (program_timeout_is_set == false && strcmp(argv[i], TIMEOUT_TAG) == 0) {
				if (i + 1 == argc) {
					wprintf(L"The program timeout argument error\n");

					return 3;
				}

				int new_program_timeout = -1;

				if (!atoi2(argv[++i], &new_program_timeout)) {
					wprintf(L"The program timeout argument parsing error\n");

					return 4;
				}

				program_timeout = new_program_timeout;
				program_timeout_is_set = true;
			} else if (order_source_is_set == false) {
				size_t string_len = strlen(argv[i]);

				if (string_len > MAX_SOURCE_SIZE) {
					wprintf(L"Invalid order source param!\n");

					return 5;
				}

				strcpy(order_source, argv[i]);

				char* source_part = strtok(order_source, ",");
				int source_index = 0;

				while (source_part != NULL && source_index < MAX_SOURCES) {
					order_sources[source_index++] = source_part;
					source_part = strtok(NULL, ",");
				}

				order_sources_ptr = &(order_sources[0]);
				order_source_is_set = true;
			}
		}
	}

	if (order_source_is_set == false) {
		order_sources_ptr = default_sources;
	}

	dxf_initialize_logger_v2("price-level-book-api.log", true, true, true, log_data_transfer_flag);
	wprintf(L"Price level book sample started.\n");
	wprintf(L"Connecting to host %hs...\n", dxfeed_host);

#ifdef _WIN32
	InitializeCriticalSection(&listener_thread_guard);
#endif

	ERRORCODE connection_result;
	if (token != NULL && token[0] != '\0') {
		connection_result =
			dxf_create_connection_auth_bearer(dxfeed_host, token, on_reader_thread_terminate,
											  NULL, NULL, NULL, NULL, &connection);
	} else {
		connection_result = dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL,
												  NULL, NULL, NULL, &connection);
	}

	if (connection_result == DXF_FAILURE) {
		free(base_symbol);
		process_last_error();

		return 10;
	}

	wprintf(L"Connected\n");

	if (!dxf_create_price_level_book(connection, base_symbol, order_sources_ptr, &book)) {
		free(base_symbol);
		process_last_error();
		dxf_close_connection(connection);

		return 20;
	}

	free(base_symbol);

	if (!dxf_attach_price_level_book_listener(book, &listener, NULL)) {
		process_last_error();
		dxf_close_price_level_book(book);
		dxf_close_connection(connection);

		return 21;
	}

	wprintf(L"Subscribed\n");

	while (!is_thread_terminate() && program_timeout--) {
#ifdef _WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
	}

	if (!dxf_close_price_level_book(book)) {
		process_last_error();
		dxf_close_connection(connection);

		return 22;
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

