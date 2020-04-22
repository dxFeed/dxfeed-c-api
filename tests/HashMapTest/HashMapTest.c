#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include <time.h>

#include "../../thirdparty/map/map.h"

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
#define MAX_ITEMS_AMOUNT 300000
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
        free(s1);
        free(key);

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
        free(s1);
        free(key);

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


#define INPUT_FILE ""

int main(int argc, char *argv[]) {
//    test_strgen();
    map_test();
//    char_test();
    return 0;
}
