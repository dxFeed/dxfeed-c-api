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
    map_test();
//    char_test();
//    return snapshot_test(argc, argv, INPUT_FILE);
    return 0;
}
