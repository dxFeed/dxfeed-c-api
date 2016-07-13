
#include "EventSubscriptionTest.h"
#include <stdio.h>
#include <string.h>
#include "EventDynamicSubscriptionTest.h"
#include "OrderSourceConfigurationTest.h"
#include "CandleTest.h"
#include "SnapshotTests.h"

typedef bool(*test_function_t)(void);

typedef struct {
    const char* name;
    test_function_t function;
} test_function_data_t;

const char* result_to_text (bool res) {
    if (res) {
        return "OK";
    } else {
        return "FAIL";
    }
}

static test_function_data_t g_tests[] = {
    { "event_subscription_tests", event_subscription_test },
    { "event_dymamic_subscription_tests", event_dynamic_subscription_all_test }, 
    { "order_source_tests", order_source_configuration_test },
    { "candle_tests", candle_all_tests },
    { "snapshot_tests", snapshot_all_test }
};

#define TESTS_COUNT (sizeof(g_tests) / sizeof(g_tests[0]))

void run_test(const char* test_name) {
    int i;
    for (i = 0; i < TESTS_COUNT; i++) {
        if (test_name == NULL || stricmp(test_name, g_tests[i].name) == 0) {
            printf("\t%-30s:\t%s\n", g_tests[i].name, result_to_text((g_tests[i].function)()));
            return;
        }
    }
    if (test_name != NULL)
        printf("Unknown test procedure: '%s'\n", test_name);
}

int main (int argc, char* argv[]) {
    int i;

    printf("DXFeed unit test procedure started...\n");

    if (argc == 1) {
        /* run all tests */
        run_test(NULL);
    }
    else {
        /* run tests which names in command line */
        for (i = 1; i < argc; i++) {
            run_test(argv[i]);
        }
    }

    printf("DXFeed unit test procedure finished.\n");

    return 0;
}
