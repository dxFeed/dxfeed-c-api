#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include <time.h>

#include "../../src/map.h"

// test string generator
# include "strgen.h"
void test_strgen() {
    char *s1 = strdup("00000");
    printf("STR: %s\n", s1);
    int i = 0;
    do {
            char *s2 = strgen(s1);
            if (i++ >= 1000000) {
                    i = 0;
                    printf("STR: %s\n", s1);
            }
            free(s1);
            s1 = s2;
    } while(strcmp(s1, "ffffff") != 0);
    printf("STR: %s\n", s1);
}

// --- map test
#include "DXFeed.h"
#include "DXErrorCodes.h"
#define MAX_STR_LEN 250
#define MAX_ITEMS_AMOUNT 30000000
typedef struct {
	char *key;
	char *value;
} data_t;
data_t* new_data_entry(char *key, char *value) {
	data_t *result = malloc(sizeof(data_t));
	result->key = key;
	result->value = value;
	return result;
}
void data_entry_to_string(data_t *value, char *dest) {
	memset(dest, 0, MAX_STR_LEN);
	strncpy(dest, value->key, MAX_STR_LEN);
	if (strlen(dest) < MAX_STR_LEN - 2) {
		strncpy(dest + strlen(dest), "; ", 2);
		if (strlen(dest) < MAX_STR_LEN - strlen(value->value)) {
			strncpy(dest + strlen(dest), value->value, MAX_STR_LEN - strlen(dest));
		}
	}
}
void print_data_entry(data_t *value) {
	printf("%s; %s\n", value->key, value->value);
}
typedef map_t_ext(char*, data_t*) map_data_t;
void search_test(map_data_t *map, char *key) {
  void **ref = map_get(map, key);
  if (ref) {
    data_t *value = *ref;
    printf("found key %s: %s\n", value->key, value->value);
  } else {
    printf("key %s not found\n", key);
  }

}
void map_test() {
	char buffer[MAX_STR_LEN];
	char *s1 = strdup("000000");
	map_data_t m;
	map_init(&m);
	long addCount = 0;
	int i = 0;

	char *key;
	char *value;
	clock_t t = clock();

	do {
		memset(buffer, 0, MAX_STR_LEN * sizeof(char));
		snprintf(buffer, MAX_STR_LEN, "val: %s", s1);
		key   = s1;
		value = strdup(buffer);
		data_t *entry = new_data_entry(key, value);
		map_set(&m, key, entry);
		data_entry_to_string(entry, buffer);
		if (i++ >= 1000000) {
			t = clock() - t;
			double time_taken = ((double)t)/CLOCKS_PER_SEC;
			printf("[insert] %s -> %s [#%d] (took %.3f seconds)\n", key, buffer, addCount, time_taken);
			i = 1;
			t = clock();
		}
                addCount++;
		s1 = strgen(key);
	} while (strcmp(s1, "ZZZZZZ") != 0 && addCount < MAX_ITEMS_AMOUNT);

	t = clock() - t;
	double time_taken = ((double)t)/CLOCKS_PER_SEC;
	printf("[insert] %s -> %s [#%d] (took %.3f seconds)\n", key, buffer, addCount, time_taken);

	long getCount = 0;
	map_iter_t iter = map_iter(&m);
	i = 0;
        t = clock();

/*
        // iterate over all existing keys
        long getCount = 0;
        map_iter_t iter = map_iter(&m);
        i = 0;
        while ((key = map_next(&m, &iter))) {
                data_t *value = *map_get(&m, key);
                data_entry_to_string(value, buffer);
                if (i++ >= 1000000) {
                        double time_taken = ((double)t)/CLOCKS_PER_SEC;
                        printf("%s -> %s [#%d]\n", key, buffer, getCount);
                        i = 0;
                }
                getCount++;
        }
*/
        // generate same keys and search for them in the map
        s1 = strdup("000000");
        void **ref;
        do {
                free(key);
                key = s1;
                memset(buffer, 0, MAX_STR_LEN * sizeof(char));

                ref = map_get(&m, key);
                if (ref) {
                      data_t *value = *ref;
                      data_entry_to_string(value, buffer);
                      if (i++ >= 1000000) {
                            t = clock() - t;
                            double time_taken = ((double)t)/CLOCKS_PER_SEC;
                            printf("[search] %s -> %s [#%d] (took %.3f seconds)\n", key, buffer, getCount, time_taken);
                            i = 1;
                            t = clock();
                      }
                      getCount++;
                }
                s1 = strgen(key);
	} while (ref && strcmp(s1, "ZZZZZZ") != 0 && getCount < addCount);

        ref = map_get(&m, key);
        if (ref) {
          t = clock() - t;
          time_taken = ((double)t)/CLOCKS_PER_SEC;
          data_t *value = *ref;
          data_entry_to_string(value, buffer);
          printf("[search] %s -> %s [#%d] (took %.3f seconds)\n", key, buffer, getCount, time_taken);
        } else {
          printf("%s not found", key);
        }

        // random search test
        printf("\n");
        printf("----------------------------------------\n");
        search_test(&m, "000001");
        search_test(&m, "0000AA");
        search_test(&m, "ZZZZZZ");
        printf("\n");

        // print map statistics
	map_print_stat(&m, false, false);

        // cleanup memory
	long freedCount = 0;
	iter = map_iter(&m);
	while ((key = map_next(&m, &iter))) {
		data_t *value = *map_get(&m, key);
		free(value->key);
		free(value->value);
		free(value);
//		free(key); // SIGSEGV
		freedCount++;
	}

	printf("added %d items\n", addCount);
	printf("read  %d items\n", getCount);
	printf("freed %d items\n", freedCount);
	printf("----------------------------------------\n");
	map_deinit(&m);

	int c = 0;
	for (int i = 0; i < 100000000; i++) {
		c++;
	}
}
// --------------------------------------------------------------------------

// --- char test
char* dx_wchar_to_char(wchar_t *input) {

	char s[1024];

	if (NULL == input)
		return NULL;

	size_t len = wcstombs(s, input, sizeof(s));

	if (len > 0) {
		s[len] = '\0';
		return strdup(s);
	} else {
		return NULL;
	}

}
void char_test() {
	char *c;

	c = dx_wchar_to_char(L"test string");
	if (NULL != c) {
		printf("converted 1: %s", c);
		free(c);
	}

	c = dx_wchar_to_char(NULL);
	if (NULL != c) {
		printf("converted 2: %s", c);
		free(c);
	}

}
// --------------------------------------------------------------------------

// --- snapshot test
// ------ linked list to store symbols read from file
struct Node {
    void  *data;
    struct Node *next;
};
void list_push(struct Node** head_ref, void *new_data, size_t data_size) {
    struct Node* new_node = (struct Node*)malloc(sizeof(struct Node));
    new_node->data  = malloc(data_size);
    new_node->next = (*head_ref);
    int i;
    for (i=0; i<data_size; i++)
        *(char *)(new_node->data + i) = *(char *)(new_data + i);
    (*head_ref)    = new_node;
}
void list_print(struct Node *node) {
    while (node != NULL) {
        printf("%s\n", node->data);
        node = node->next;
    }
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
struct Node * readFile(char *fileName) {
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(fileName, "r");
    if (fp == NULL)
        return NULL;

    struct Node *result = NULL;

    int readLines = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        char *pos;
        if ((pos=strchr(line, '\n')) != NULL)
          *pos = '\0';
        list_push(&result, line, read * sizeof(char));
        readLines++;
    }
    printf("read lines: %d\n", readLines);

    fclose(fp);
    if (line)
        free(line);

    return result;
}

// ------ snapshot test itself
#define STATIC_PARAMS_COUNT 4
#define DEFAULT_RECORDS_PRINT_LIMIT 7
#define RECORDS_PRINT_LIMIT_SHORT_PARAM "-l"
#define MAX_SOURCE_SIZE 42
#define TOKEN_PARAM_SHORT_TAG "-T"
#define LS(s) LS2(s)
#define LS2(s) L##s
#define STRINGIFY(a) STR(a)
#define STR(a) #a

#ifndef true
typedef int bool;
#define true 1
#define false 0
#endif

#ifndef stricmp
#define stricmp strcasecmp
#endif

static volatile bool is_listener_thread_terminated = false;

void on_reader_thread_terminate (dxf_connection_t connection, void *user_data) {
  is_listener_thread_terminated = true;
  wprintf(L"\nTerminating listener thread\n");
}
bool is_thread_terminate () {
  bool res;
  res = is_listener_thread_terminated;
  return res;
}
void print_timestamp (dxf_long_t timestamp) {
  wchar_t timefmt[80];

  struct tm *timeinfo;
  time_t tmpint = (time_t) (timestamp / 1000);
  timeinfo = localtime(&tmpint);
  wcsftime(timefmt, 80, L"%Y%m%d-%H%M%S", timeinfo);
  wprintf(L"%ls", timefmt);
}
void process_last_error () {
  int error_code = dx_ec_success;
  dxf_const_string_t error_descr = NULL;
  int res;

  res = dxf_get_last_error(&error_code, &error_descr);

  if (res == DXF_SUCCESS) {
    if (error_code == dx_ec_success) {
      wprintf(L"no error information is stored");
      return;
    }

    wprintf(L"Error occurred and successfully retrieved:\n"
            L"error code = %d, description = \"%ls\"\n",
            error_code, error_descr);
    return;
  }

  wprintf(L"An error occurred but the error subsystem failed to initialize\n");
}
dxf_string_t ansi_to_unicode (const char *ansi_str) {
  dxf_string_t wide_str = NULL;
  size_t wide_size = mbstowcs(NULL, ansi_str, 0); // 0 is ignored

  if (wide_size > 0 && wide_size != (size_t) -1) {
    wide_str = calloc(wide_size + 1, sizeof(dxf_char_t));
    mbstowcs(wide_str, ansi_str, wide_size + 1);
  }

  return wide_str;
}
bool atoi2 (char *str, int *result) {
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

void listener (const dxf_snapshot_data_ptr_t snapshot_data, void *user_data) {
  size_t i;
  size_t records_count = snapshot_data->records_count;
  int records_print_limit = DEFAULT_RECORDS_PRINT_LIMIT;

  if (user_data) {
    records_print_limit = *(int *) user_data;
  }

  wprintf(L"Snapshot %ls{symbol=%ls, records_count=%zu}\n",
          dx_event_type_to_string(snapshot_data->event_type), snapshot_data->symbol,
          records_count);

  if (snapshot_data->event_type == DXF_ET_ORDER) {
    dxf_order_t *order_records = (dxf_order_t *) snapshot_data->records;
    for (i = 0; i < records_count; ++i) {
      dxf_order_t order = order_records[i];

      if (records_print_limit > 0 && i >= records_print_limit) {
        wprintf(L"   { ... %zu records left ...}\n", records_count - i);
        break;
      }

      wprintf(L"   {index=0x%llX, side=%i, scope=%i, time=",
              order.index, order.side, order.scope);
      print_timestamp(order.time);
      wprintf(L", exchange code=%c, market maker=%ls, price=%f, size=%d",
              order.exchange_code, order.market_maker, order.price, order.size);
      if (wcslen(order.source) > 0)
        wprintf(L", source=%ls", order.source);
      wprintf(L", count=%d}\n", order.count);
    }
  } else if (snapshot_data->event_type == DXF_ET_CANDLE) {
    dxf_candle_t *candle_records = (dxf_candle_t *) snapshot_data->records;
    for (i = 0; i < snapshot_data->records_count; ++i) {
      dxf_candle_t candle = candle_records[i];

      if (records_print_limit > 0 && i >= records_print_limit) {
        wprintf(L"   { ... %zu records left ...}\n", records_count - i);
        break;
      }

      wprintf(L"   {time=");
      print_timestamp(candle.time);
      wprintf(L", sequence=%d, count=%f, open=%f, high=%f, low=%f, close=%f, volume=%f, "
              L"VWAP=%f, bidVolume=%f, askVolume=%f}\n",
              candle.sequence, candle.count, candle.open, candle.high,
              candle.low, candle.close, candle.volume, candle.vwap,
              candle.bid_volume, candle.ask_volume);
    }
  } else if (snapshot_data->event_type == DXF_ET_SPREAD_ORDER) {
    dxf_order_t *order_records = (dxf_order_t *) snapshot_data->records;
    for (i = 0; i < records_count; ++i) {
      dxf_order_t order = order_records[i];

      if (records_print_limit > 0 && i >= records_print_limit) {
        wprintf(L"   { ... %zu records left ...}\n", records_count - i);
        break;
      }

      wprintf(L"   {index=0x%llX, side=%i, scope=%i, time=",
              order.index, order.side, order.scope);
      print_timestamp(order.time);
      wprintf(L", sequence=%i, exchange code=%c, price=%f, size=%d, source=%ls, "
              L"count=%i, flags=%i, spread symbol=%ls}\n",
              order.sequence, order.exchange_code, order.price, order.size,
              wcslen(order.source) > 0 ? order.source : L"",
              order.count, order.event_flags,
              wcslen(order.spread_symbol) > 0 ? order.spread_symbol : L"");
    }
  } else if (snapshot_data->event_type == DXF_ET_TIME_AND_SALE) {
    dxf_time_and_sale_t *time_and_sale_records = (dxf_time_and_sale_t *) snapshot_data->records;
    for (i = 0; i < snapshot_data->records_count; ++i) {
      dxf_time_and_sale_t tns = time_and_sale_records[i];

      if (records_print_limit > 0 && i >= records_print_limit) {
        wprintf(L"   { ... %zu records left ...}\n", records_count - i);
        break;
      }

//      wprintf(L"   {event id=%"LS(PRId64)L", time=%"LS(
//          PRId64)L", exchange code=%c, price=%f, size=%i, bid price=%f, ask price=%f, "
//                 L"exchange sale conditions=\'%ls\', is ETH trade=%ls, type=%i}\n",
//          tns.index, tns.time, tns.exchange_code, tns.price, tns.size,
//          tns.bid_price, tns.ask_price, tns.exchange_sale_conditions,
//          tns.is_eth_trade ? L"True" : L"False", tns.type);
    }
  } else if (snapshot_data->event_type == DXF_ET_GREEKS) {
    dxf_greeks_t *greeks_records = (dxf_greeks_t *) snapshot_data->records;
    for (i = 0; i < snapshot_data->records_count; ++i) {
      dxf_greeks_t grks = greeks_records[i];

      if (records_print_limit > 0 && i >= records_print_limit) {
        wprintf(L"   { ... %zu records left ...}\n", records_count - i);
        break;
      }

//      wprintf(L"   {time=");
//      print_timestamp(grks.time);
//      wprintf(L", index=0x%"LS(PRIX64)L", greeks price=%f, volatility=%f, "
//                                      L"delta=%f, gamma=%f, theta=%f, rho=%f, vega=%f}\n",
//          grks.index, grks.price, grks.volatility, grks.delta,
//          grks.gamma, grks.theta, grks.rho, grks.vega);
    }
  } else if (snapshot_data->event_type == DXF_ET_SERIES) {
    dxf_series_t *series_records = (dxf_series_t *) snapshot_data->records;
    for (i = 0; i < snapshot_data->records_count; ++i) {
      dxf_series_t srs = series_records[i];

      if (records_print_limit > 0 && i >= records_print_limit) {
        wprintf(L"   { ... %zu records left ...}\n", records_count - i);
        break;
      }
//      wprintf(L"   {index=%"LS(PRId64)L", time=", srs.index);
//      print_timestamp(srs.time);
//      wprintf(L", sequence=%i, expiration=%d, volatility=%f, put call ratio=%f, "
//              L"forward_price=%f, dividend=%f, interest=%f, index=0x%"LS(PRIX64)L"}\n",
//          srs.sequence, srs.expiration, srs.volatility, srs.put_call_ratio,
//          srs.forward_price, srs.dividend, srs.interest, srs.index);
    }
  }
}


int snapshot_test(int argc, char *argv[], char *inputFileName) {
    struct Node *symbols = readFile(inputFileName);
//    list_print(list);

    dxf_connection_t connection;
    dxf_candle_attributes_t candle_attributes = NULL;
    char *event_type_name = NULL;
    dx_event_id_t event_id;
    dxf_string_t base_symbol = NULL;
    char *dxfeed_host = NULL;
    dxf_string_t dxfeed_host_u = NULL;

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
        wprintf(L"Unknown event type.\n");
        return -1;
    }

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
                    wprintf(L"The records print limit argument error\n");
                    return -1;
                }
                int new_records_print_limit = -1;
                if (!atoi2(argv[++i], &new_records_print_limit)) {
                  wprintf(L"The records print limit argument parsing error\n");

                  return -1;
                }
                records_print_limit = new_records_print_limit;
                records_print_limit_is_set = true;
            } else if (token_is_set == false && strcmp(argv[i], TOKEN_PARAM_SHORT_TAG) == 0) {
                if (i + 1 == argc) {
                  wprintf(L"The token argument error\n");

                  return -1;
                }
                token = argv[++i];
                token_is_set = true;
            } else if (order_source_is_set == false) {
                size_t string_len = 0;
                string_len = strlen(argv[i]);
                if (string_len > MAX_SOURCE_SIZE) {
                    wprintf(L"Invalid order source param!\n");

                    return -1;
                }
                char order_source[MAX_SOURCE_SIZE + 1] = {0};
                strcpy(order_source, argv[i]);
                order_source_ptr = &(order_source[0]);
                order_source_is_set = true;
            }
        }
    }

    wprintf(L"HashMapTest started.\n");
    dxfeed_host_u = ansi_to_unicode(dxfeed_host);
    wprintf(L"Connecting to host %ls...\n", dxfeed_host_u);
    free(dxfeed_host_u);

    if (token != NULL && token[0] != '\0') {
        if (!dxf_create_connection_auth_bearer(
            dxfeed_host, token, on_reader_thread_terminate, NULL, NULL, NULL, NULL, &connection)) {
            process_last_error();
            return -1;
        }
    } else if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, NULL, &connection)) {
        process_last_error();
        return -1;
    }

    wprintf(L"Connection successful!\n");

    // create snapshot for each symbol read from file and attach snapshot listener for every created snapshot

    struct Node *snapshots = NULL;

    struct Node *node = symbols;
    while (node != NULL) {
        char symbol = node->data;
        node = node->next;
        dxf_snapshot_t snapshot;
        if (!dxf_create_snapshot(connection, event_id, base_symbol, NULL, 0, &snapshot)) {
            process_last_error();
            dxf_close_connection(connection);
            return -1;
        }
        list_push(&snapshots, snapshot, sizeof(dxf_snapshot_t));
    }

    if (!dxf_create_snapshot(connection, event_id, base_symbol, NULL, 0, &snapshot)) {
    process_last_error();
    dxf_close_connection(connection);
    return -1;
  }

    // ...
    // ...
    // ...

    wprintf(L"Disconnecting from host...\n");
    if (!dxf_close_connection(connection)) {
        process_last_error();
        return -1;
    }
    wprintf(L"Disconnect successful!\nHashMapTest completed successfully!\n");

    list_free(symbols);
}
// --------------------------------------------------------------------------


#define INPUT_FILE ""

int main(int argc, char *argv[]) {
//    test_strgen();
//    map_test();
//    char_test();
    return snapshot_test(argc, argv, INPUT_FILE);
//    return 0;
}
