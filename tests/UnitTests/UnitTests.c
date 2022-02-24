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

#include <stdio.h>
#include <string.h>
#include "AddressParserTest.h"
#include "AlgorithmsTest.h"
#include "ConnectionTest.h"
#include "DXNetworkTests.h"
#include "EventSubscriptionTest.h"
#include "EventDynamicSubscriptionTest.h"
#include "OrderSourceConfigurationTest.h"
#include "CandleTest.h"
#include "SnapshotTests.h"

typedef int(*test_function_t)(void);

typedef struct {
	const char* name;
	test_function_t function;
} test_function_data_t;

const char* result_to_text (int res) {
	if (res) {
		return "OK";
	} else {
		return "FAIL";
	}
}

static test_function_data_t g_tests[] = {
	{ "event_subscription_test", event_subscription_all_test },
	{ "address_parser_test", address_parser_all_tests },
	{ "algorithms_test", algorithms_all_tests },
	{ "network_test", network_all_test },
	{ "event_dymamic_subscription_test", event_dynamic_subscription_all_test },
	{ "order_source_test", order_source_configuration_test },
	{ "candle_test", candle_all_tests },
	{ "connection_test", connection_all_test },
	{ "snapshot_test", snapshot_all_test },
	{ "snapshot_unit_test", snapshot_all_unit_test }
};

#define TESTS_COUNT (sizeof(g_tests) / sizeof(g_tests[0]))

void print_usage() {
	int i;
	printf("Usage: UnitTests [<test-name>]...\n"
		"where test-name is empty or any combination of next values:\n");
	for (i = 0; i < TESTS_COUNT; i++)
		printf("   %s\n", g_tests[i].name);
}

int run_test(const char* test_name) {
	int i;
	int result = true;
	int test_complete = false;
	for (i = 0; i < TESTS_COUNT; i++) {
		if (test_name == NULL || stricmp(test_name, g_tests[i].name) == 0) {
			printf("\t%-35s:\r", g_tests[i].name);
			result &= (g_tests[i].function)();
			printf("\t%-35s:\t%s\n", g_tests[i].name, result_to_text(result));
			test_complete = true;
		}
	}
	if (test_name != NULL && !test_complete) {
		printf("Unknown test procedure: '%s'\n", test_name);
		return false;
	}
	return result;
}

int main (int argc, char* argv[]) {
	int i;
	int result = true;

	print_usage();

	printf("\nDXFeed unit test procedure started...\n");

	if (argc == 1) {
		/* run all tests */
		result &= run_test(NULL);
	}
	else {
		/* run tests which names in command line */
		for (i = 1; i < argc; i++) {
			result &= run_test(argv[i]);
		}
	}

	printf("DXFeed unit test procedure finished: %s.\n", result_to_text(result));

	return 0;
}
