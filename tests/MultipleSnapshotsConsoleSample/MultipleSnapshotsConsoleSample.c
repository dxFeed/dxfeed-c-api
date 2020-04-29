// MultipleSnapshotsConsoleSample.cpp : Defines the entry point for the console application.
//

#ifdef _WIN32
#include <Windows.h>
#else

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wctype.h>

#define stricmp strcasecmp
#endif

#include <inttypes.h>
#include <stdio.h>
#include <time.h>

#include "DXErrorCodes.h"
#include "DXFeed.h"

#define STRINGIFY(a) STR(a)
#define STR(a)		 #a

#define LS(s)  LS2(s)
#define LS2(s) L##s

#ifndef true
typedef int bool;

#define true 1
#define false 0
#endif

// Prevents file names globbing (converting * to all files in the current dir)
#ifdef __MINGW64_VERSION_MAJOR
int _CRT_glob = 0;
#endif

// ------ linked list to store symbols read from file
struct Node {
	void *data;
	struct Node *next;
};
void list_push(struct Node **head_ref, void *new_data, size_t data_size) {
	struct Node *new_node = (struct Node *)malloc(sizeof(struct Node));
	new_node->data = malloc(data_size);
	new_node->next = (*head_ref);
	int i;
	for (i = 0; i < data_size; i++) *(char *)(new_node->data + i) = *(char *)(new_data + i);
	(*head_ref) = new_node;
}
void list_print(struct Node *node) {
	while (node != NULL) {
		printf("%s\n", node->data);
		node = node->next;
	}
}
int list_size(struct Node *node) {
	int size = 0;
	while (node != NULL) {
		node = node->next;
		size++;
	}
	return size;
}
void list_free(struct Node *node) {
	while (node != NULL) {
		struct Node *curr = node;
		node = node->next;
		free(curr->data);
		free(curr);
	}
}

// ------ read symbols from file into list
struct Node *readFile(char *fileName) {
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	fp = fopen(fileName, "r");
	if (fp == NULL) return NULL;

	struct Node *result = NULL;

	int readLines = 0;
	while ((read = getline(&line, &len, fp)) != -1) {
		char *pos;
		if ((pos = strchr(line, '\n')) != NULL) *pos = '\0';
		if (strlen(line) > 0) list_push(&result, line, read * sizeof(char));
		readLines++;
	}

	fclose(fp);
	if (line) free(line);

	return result;
}

// plus the name of the executable
#define STATIC_PARAMS_COUNT				4
#define DEFAULT_RECORDS_PRINT_LIMIT		7
#define RECORDS_PRINT_LIMIT_SHORT_PARAM "-l"
#define MAX_SOURCE_SIZE					42
#define TOKEN_PARAM_SHORT_TAG			"-T"

dxf_const_string_t dx_event_type_to_string(int event_type) {
	switch (event_type) {
		case DXF_ET_TRADE:
			return L"Trade";
		case DXF_ET_QUOTE:
			return L"Quote";
		case DXF_ET_SUMMARY:
			return L"Summary";
		case DXF_ET_PROFILE:
			return L"Profile";
		case DXF_ET_ORDER:
			return L"Order";
		case DXF_ET_TIME_AND_SALE:
			return L"Time&Sale";
		case DXF_ET_CANDLE:
			return L"Candle";
		case DXF_ET_TRADE_ETH:
			return L"TradeETH";
		case DXF_ET_SPREAD_ORDER:
			return L"SpreadOrder";
		case DXF_ET_GREEKS:
			return L"Greeks";
		case DXF_ET_SERIES:
			return L"Series";
		case DXF_ET_CONFIGURATION:
			return L"Configuration";
		default:
			return L"";
	}
}

/* -------------------------------------------------------------------------- */
#ifdef _WIN32
static bool is_listener_thread_terminated = false;
CRITICAL_SECTION listener_thread_guard;

bool is_thread_terminate() {
	bool res;
	EnterCriticalSection(&listener_thread_guard);
	res = is_listener_thread_terminated;
	LeaveCriticalSection(&listener_thread_guard);

	return res;
}
#else
static volatile bool is_listener_thread_terminated = false;

bool is_thread_terminate() {
	bool res;
	res = is_listener_thread_terminated;
	return res;
}

#endif

/* -------------------------------------------------------------------------- */

#ifdef _WIN32
void on_reader_thread_terminate(dxf_connection_t connection, void *user_data) {
	EnterCriticalSection(&listener_thread_guard);
	is_listener_thread_terminated = true;
	LeaveCriticalSection(&listener_thread_guard);

	printf("\nTerminating listener thread\n");
}
#else

void on_reader_thread_terminate(dxf_connection_t connection, void *user_data) {
	is_listener_thread_terminated = true;
	printf("\nTerminating listener thread\n");
}

#endif

void print_timestamp(dxf_long_t timestamp) {
	char timefmt[80];

	struct tm *timeinfo;
	time_t tmpint = (time_t)(timestamp / 1000);
	timeinfo = localtime(&tmpint);
	strftime(timefmt, 80, "%Y%m%d-%H%M%S", timeinfo);
	printf("<<< TIME STAMP >>>: %s\n", timefmt);
}

/* -------------------------------------------------------------------------- */

void process_last_error() {
	int error_code = dx_ec_success;
	dxf_const_string_t error_descr = NULL;
	int res;

	res = dxf_get_last_error(&error_code, &error_descr);

	if (res == DXF_SUCCESS) {
		if (error_code == dx_ec_success) {
			printf("no error information is stored");
			return;
		}

		printf(
			"Error occurred and successfully retrieved:\n"
			"error code = %d, description = \"%ls\"\n",
			error_code, error_descr);
		return;
	}

	printf("An error occurred but the error subsystem failed to initialize\n");
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

	int printDetails = false;

	printf("Snapshot %ls{symbol=%ls, records_count=%zu}\n", dx_event_type_to_string(snapshot_data->event_type),
		   snapshot_data->symbol, records_count);
	/*
if (snapshot_data->event_type == DXF_ET_ORDER) {
	dxf_order_t *order_records = (dxf_order_t *) snapshot_data->records;
	for (i = 0; i < records_count; ++i) {
		dxf_order_t order = order_records[i];

		if (records_print_limit > 0 && i >= records_print_limit) {
			printf("   { ... %zu records left ...}\n", records_count - i);
			break;
		}

//			printf("   {index=0x%llX, side=%i, scope=%i, time=",
//			        order.index, order.side, order.scope);
		print_timestamp(order.time);
//			printf(", exchange code=%c, market maker=%ls, price=%f, size=%d",
//			        order.exchange_code, order.market_maker, order.price, order.size);
//			if (wcslen(order.source) > 0)
//				printf(", source=%ls", order.source);
//			printf(", count=%d}\n", order.count);
	}
} else if (snapshot_data->event_type == DXF_ET_CANDLE) {
	dxf_candle_t *candle_records = (dxf_candle_t *) snapshot_data->records;
	for (i = 0; i < snapshot_data->records_count; ++i) {
		dxf_candle_t candle = candle_records[i];

		if (records_print_limit > 0 && i >= records_print_limit) {
			printf("   { ... %zu records left ...}\n", records_count - i);
			break;
		}

//			printf("   {time=");
		print_timestamp(candle.time);
//			printf(", sequence=%d, count=%f, open=%f, high=%f, low=%f, close=%f, volume=%f, "
//			        "VWAP=%f, bidVolume=%f, askVolume=%f}\n",
//			        candle.sequence, candle.count, candle.open, candle.high,
//			        candle.low, candle.close, candle.volume, candle.vwap,
//			        candle.bid_volume, candle.ask_volume);
	}
} else if (snapshot_data->event_type == DXF_ET_SPREAD_ORDER) {
	dxf_order_t *order_records = (dxf_order_t *) snapshot_data->records;
	for (i = 0; i < records_count; ++i) {
		dxf_order_t order = order_records[i];

		if (records_print_limit > 0 && i >= records_print_limit) {
			printf("   { ... %zu records left ...}\n", records_count - i);
			break;
		}

//			printf("   {index=0x%llX, side=%i, scope=%i, time=",
//			        order.index, order.side, order.scope);
		print_timestamp(order.time);
//			printf(", sequence=%i, exchange code=%c, price=%f, size=%d, source=%ls, "
//			        "count=%i, flags=%i, spread symbol=%ls}\n",
//			        order.sequence, order.exchange_code, order.price, order.size,
//			        wcslen(order.source) > 0 ? order.source : L"",
//			        order.count, order.event_flags,
//			        wcslen(order.spread_symbol) > 0 ? order.spread_symbol : L"");
	}
} else */
	if (printDetails && snapshot_data->event_type == DXF_ET_TIME_AND_SALE) {
		dxf_time_and_sale_t *time_and_sale_records = (dxf_time_and_sale_t *)snapshot_data->records;
		for (i = 0; i < snapshot_data->records_count; ++i) {
			dxf_time_and_sale_t tns = time_and_sale_records[i];

			if (records_print_limit > 0 && i >= records_print_limit) {
				printf("   { ... %zu records left ...}\n", records_count - i);
				break;
			}

			printf(
				"   {event id=%d, time=%d, exchange code=%c, price=%f, size=%i, bid price=%f, ask price=%f, "
				"exchange sale conditions=\'%ls\', is ETH trade=%s, type=%i}\n",
				tns.index, tns.time, tns.exchange_code, tns.price, tns.size, tns.bid_price, tns.ask_price,
				tns.exchange_sale_conditions, tns.is_eth_trade ? "True" : "False", tns.type);
		}
	} /*else if (snapshot_data->event_type == DXF_ET_GREEKS) {
		dxf_greeks_t *greeks_records = (dxf_greeks_t *) snapshot_data->records;
		for (i = 0; i < snapshot_data->records_count; ++i) {
			dxf_greeks_t grks = greeks_records[i];

			if (records_print_limit > 0 && i >= records_print_limit) {
				printf("   { ... %zu records left ...}\n", records_count - i);
				break;
			}

//			printf("   {time=");
			print_timestamp(grks.time);
//			printf(", index=0x%"LS(PRIX64)L", greeks price=%f, volatility=%f, "
//			        "delta=%f, gamma=%f, theta=%f, rho=%f, vega=%f}\n",
//			        grks.index, grks.price, grks.volatility, grks.delta,
//			        grks.gamma, grks.theta, grks.rho, grks.vega);
		}
	} else if (snapshot_data->event_type == DXF_ET_SERIES) {
		dxf_series_t *series_records = (dxf_series_t *) snapshot_data->records;
		for (i = 0; i < snapshot_data->records_count; ++i) {
			dxf_series_t srs = series_records[i];

			if (records_print_limit > 0 && i >= records_print_limit) {
				printf("   { ... %zu records left ...}\n", records_count - i);
				break;
			}
//			printf("   {index=%"LS(PRId64)L", time=", srs.index);
			print_timestamp(srs.time);
//			printf(", sequence=%i, expiration=%d, volatility=%f, put call ratio=%f, "
//			        "forward_price=%f, dividend=%f, interest=%f, index=0x%"LS(PRIX64)"}\n",
//			        srs.sequence, srs.expiration, srs.volatility, srs.put_call_ratio,
//			        srs.forward_price, srs.dividend, srs.interest, srs.index);
		}
	}*/
}

/* -------------------------------------------------------------------------- */

bool atoi2(char *str, int *result) {
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

dxf_connection_t connection;
struct Node *symbols_list = NULL;
dxf_snapshot_t *snapshots;
dxf_candle_attributes_t *candle_attributes;

int process_exit(int exit_code) {
	struct Node *n = symbols_list;
	int index = 0;

	while (n != NULL) {
		printf("   > clean data for %s\n", n->data);
		if (NULL != snapshots) {
			void *snapshot = snapshots[index];
			if (NULL != snapshot) printf("     - close snapshot\n");
			if (NULL != snapshot && !dxf_close_snapshot(snapshot)) {
				process_last_error();
			}
		}

		if (NULL != candle_attributes) {
			void *candle_attribute = candle_attributes[index];
			if (NULL != candle_attribute) printf("     - remove symbol attributes\n");
			if (NULL != candle_attribute && !dxf_delete_candle_symbol_attributes(candle_attribute)) {
				process_last_error();
			}
		}

		n = n->next;
		index++;
	}

	list_free(symbols_list);

	if (NULL != connection) {
		if (!dxf_close_connection(connection)) {
			process_last_error();
		} else {
			printf("Disconnect successful!\n");
		}
		//    free(connection);
	}

	return exit_code;
}

int main(int argc, char *argv[]) {
	char *event_type_name = NULL;
	dx_event_id_t event_id;
	char *dxfeed_host = NULL;
	dxf_string_t dxfeed_host_u = NULL;
	char *inputFileName;

	if (argc < STATIC_PARAMS_COUNT) {
		printf("DXFeed command line sample.\n"
		       "Usage: SnapshotConsoleSample <server address> <event type> <symbol> [order_source] [" RECORDS_PRINT_LIMIT_SHORT_PARAM " <records_print_limit>] [" TOKEN_PARAM_SHORT_TAG " <token>]\n"
		       "  <server address> - The DXFeed server address, e.g. demo.dxfeed.com:7300\n"
		       "  <event type> - The event type, one of the following: ORDER, CANDLE, SPREAD_ORDER,\n"
		       "                 TIME_AND_SALE, GREEKS, SERIES\n"
		       "  <inputFiule> - File with symbols list\n"
		       "  [order_source] - a) source for Order (also can be empty), e.g. NTV, BYX, BZX, DEA,\n"
		       "                      ISE, DEX, IST\n"
		       "                   b) source for MarketMaker, one of following: COMPOSITE_BID or \n"
		       "                      COMPOSITE_ASK\n"
		       "  " RECORDS_PRINT_LIMIT_SHORT_PARAM " <records_print_limit> - The number of displayed records (0 - unlimited, default: " STRINGIFY(
				       DEFAULT_RECORDS_PRINT_LIMIT) ")\n"
		       "  " TOKEN_PARAM_SHORT_TAG " <token> - The authorization token\n"
		);

		return 0;
	}

	dxf_initialize_logger("snapshot-console-api.log", true, true, true);

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
		printf("Unknown event type.\n");
		return -1;
	}

	inputFileName = argv[3];
	if (inputFileName == NULL) {
		return -1;
	}
	symbols_list = readFile(inputFileName);
	if (symbols_list == NULL) {
		return -1;
	}

	printf("Total symbols loaded from file: %d\n", list_size(symbols_list));

	char *order_source_ptr = NULL;
	int records_print_limit = DEFAULT_RECORDS_PRINT_LIMIT;
	char *token = NULL;

	if (argc > STATIC_PARAMS_COUNT) {
		int i = 0;
		bool records_print_limit_is_set = false;
		bool token_is_set = false;
		bool order_source_is_set = false;

		for (i = STATIC_PARAMS_COUNT; i < argc; i++) {
			if (records_print_limit_is_set == false && strcmp(argv[i], RECORDS_PRINT_LIMIT_SHORT_PARAM) == 0) {
				if (i + 1 == argc) {
					printf("The records print limit argument error\n");

					return -1;
				}

				int new_records_print_limit = -1;

				if (!atoi2(argv[++i], &new_records_print_limit)) {
					printf("The records print limit argument parsing error\n");

					return -1;
				}

				records_print_limit = new_records_print_limit;
				records_print_limit_is_set = true;
			} else if (token_is_set == false && strcmp(argv[i], TOKEN_PARAM_SHORT_TAG) == 0) {
				if (i + 1 == argc) {
					printf("The token argument error\n");

					return -1;
				}

				token = argv[++i];
				token_is_set = true;
			} else if (order_source_is_set == false) {
				size_t string_len = 0;
				string_len = strlen(argv[i]);

				if (string_len > MAX_SOURCE_SIZE) {
					printf("Invalid order source param!\n");

					return -1;
				}

				char order_source[MAX_SOURCE_SIZE + 1] = {0};

				strcpy(order_source, argv[i]);
				order_source_ptr = &(order_source[0]);
				order_source_is_set = true;
			}
		}
	}

	printf("MultipleSnapshots test started.\n");
	dxfeed_host_u = ansi_to_unicode(dxfeed_host);
	printf("Connecting to host %ls...\n", dxfeed_host_u);
	free(dxfeed_host_u);

#ifdef _WIN32
	InitializeCriticalSection(&listener_thread_guard);
#endif

	if (token != NULL && token[0] != '\0') {
		if (!dxf_create_connection_auth_bearer(dxfeed_host, token, on_reader_thread_terminate, NULL, NULL, NULL, NULL,
											   &connection)) {
			process_last_error();

			return -1;
		}
	} else if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, NULL, &connection)) {
		process_last_error();

		return -1;
	}

	printf("Connection successful!\n");

	snapshots = malloc(sizeof(dxf_snapshot_data_ptr_t) * list_size(symbols_list));
	candle_attributes = malloc(sizeof(dxf_candle_attributes_t) * list_size(symbols_list));

	struct Node *symbol = symbols_list;
	int index = 0;
	while (symbol != NULL) {
		dxf_snapshot_t snapshot;
		dxf_candle_attributes_t candle_attribute;
		dxf_string_t base_symbol = ansi_to_unicode(symbol->data);

		//            printf("%ls\n", base_symbol);

		if (event_id == dx_eid_candle) {
			if (!dxf_create_candle_symbol_attributes(base_symbol, DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT,
													 DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT, dxf_ctpa_day,
													 dxf_cpa_default, dxf_csa_default, dxf_caa_default,
													 DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, &candle_attribute)) {
				process_last_error();
				return process_exit(-1);
			}

			if (!dxf_create_candle_snapshot(connection, candle_attribute, 0, &snapshot)) {
				process_last_error();
				return process_exit(-1);
			}
			candle_attributes[index] = candle_attribute;
		} else if (event_id == dx_eid_order) {
			if (!dxf_create_order_snapshot(connection, base_symbol, order_source_ptr, 0, &snapshot)) {
				process_last_error();
				process_exit(-1);
			}
			snapshots[index] = snapshot;
		} else {
			if (!dxf_create_snapshot(connection, event_id, base_symbol, NULL, 0, &snapshot)) {
				process_last_error();
				return process_exit(-1);
			}
			snapshots[index] = snapshot;
		}

		if (!dxf_attach_snapshot_listener(snapshot, listener, (void *)&records_print_limit)) {
			process_last_error();
			return process_exit(-1);
		};

		printf("Subscribed to '%ls'\n", base_symbol);

		free(base_symbol);

		symbol = symbol->next;
		index++;
	}

	printf("Subscription successful!\n");

	int i = 0;
	while (!is_thread_terminate() && i++ < 10) {
#ifdef _WIN32
		Sleep(100);
#else
		sleep(1);
#endif
	}
	is_listener_thread_terminated = true;

	process_exit(0);

#ifdef _WIN32
	DeleteCriticalSection(&listener_thread_guard);
#endif
	printf("MultipleSnapshots test completed successfully!\n");
	return 0;
}

// valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt
// ./MultipleSnapshotsConsoleSampled_64 mddqa.in.devexperts.com:7400 TIME_AND_SALE /data/symbols3
