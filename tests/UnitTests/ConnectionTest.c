#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <string.h>
#include <wctype.h>
#include <stdlib.h>
#define stricmp strcasecmp
#endif

#include "ConnectionContextData.h"
#include "ConnectionTest.h"
#include "DXAlgorithms.h"
#include "DXFeed.h"
#include "DXTypes.h"
#include "EventSubscription.h"
#include "SymbolCodec.h"
#include "TestHelper.h"
#include "DXThreads.h"
#include "DXMemory.h"
#include "ClientMessageProcessor.h"

#define MULTIPLE_CONNECTION_THREAD_COUNT 10

static dxf_const_string_t g_symbol_list[] = { L"SYMA", L"SYMB", L"SYMC" };
static int g_symbol_size = sizeof(g_symbol_list) / sizeof(g_symbol_list[0]);
static int g_event_case_list[] = { DXF_ET_TRADE, DXF_ET_QUOTE, DXF_ET_SUMMARY, DXF_ET_PROFILE };
static int g_event_case_size = sizeof(g_event_case_list) / sizeof(g_event_case_list[0]);

typedef struct {
	dxf_connection_t connection;
	dxf_subscription_t subscription;
	int event_types;
} multiple_connection_data_t;

unsigned multiple_connection_routine(void* arg) {
	multiple_connection_data_t* data = arg;
	dxf_connection_t connection = data->connection;
	dxf_subscription_t subscription = data->subscription;

	dx_init_symbol_codec();
	connection = dx_init_connection();
	subscription = dx_create_event_subscription(connection, data->event_types, 0, 0);
	dx_add_symbols(subscription, g_symbol_list, g_symbol_size);
	dx_send_record_description(connection, true);
	dx_subscribe_symbols_to_events(connection, dx_get_order_source(subscription), g_symbol_list, g_symbol_size, data->event_types, false, true, 0, 0);
#ifdef _WIN32
	Sleep(2000);
#else
	sleep(2);
#endif
	return 1;
}

/*
 * Test
 * Simulates opening multiple connections simultaneously.
 * Expected: application shouldn't crash.
 */
bool multiple_connection_test(void) {
	size_t i;
	dx_thread_t thread_list[MULTIPLE_CONNECTION_THREAD_COUNT];
	size_t thread_count = MULTIPLE_CONNECTION_THREAD_COUNT;
	multiple_connection_data_t* connections = dx_calloc(thread_count, sizeof(multiple_connection_data_t));
	dxf_const_string_t symbol_set[] = { L"SYMA", L"SYMB", L"SYMC" };
	for (i = 0; i < thread_count; i++) {
		multiple_connection_data_t* connection_data = connections + i;
		connection_data->event_types = g_event_case_list[dx_random_integer(g_event_case_size - 1)];
		dx_thread_create(&thread_list[i], NULL, multiple_connection_routine, connection_data);
	}
	for (i = 0; i < thread_count; i++) {
		dx_wait_for_thread(thread_list[i], NULL);
	}
	for (i = 0; i < thread_count; i++) {
		multiple_connection_data_t* connection_data = connections + i;
		dx_close_event_subscription(connection_data->subscription);
		dx_deinit_connection(connection_data->connection);
		dx_close_thread_handle(thread_list[i]);
	}
	dx_free(connections);
	return true;
}

/*
 * Test
 * Tries to create connection with invalid address and other input parameters cases.
 * No real connections opens in this test.
 * Expected: application shouldn't crash; all attempts to create connection with invalid data
 *           should completes with error state.
 */
bool invalid_connection_address_test(void) {
	const char* invalid_address = "demo.dxfeed.com::7300";
	dxf_connection_t connection;

	//invalid connection handler
	DX_CHECK(dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_connection("demo.dxfeed.com:7300", NULL, NULL, NULL, NULL, NULL, NULL)));
	//invalid address
	DX_CHECK(dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_connection(invalid_address, NULL, NULL, NULL, NULL, NULL, &connection)));
	DX_CHECK(dx_is_equal_ptr(NULL, connection));
	DX_CHECK(dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_connection("no-port", NULL, NULL, NULL, NULL, NULL, &connection)));
	DX_CHECK(dx_is_equal_ptr(NULL, connection));
	DX_CHECK(dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_connection(":123", NULL, NULL, NULL, NULL, NULL, &connection)));
	DX_CHECK(dx_is_equal_ptr(NULL, connection));
	DX_CHECK(dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_connection("a:123", NULL, NULL, NULL, NULL, NULL, &connection)));
	DX_CHECK(dx_is_equal_ptr(NULL, connection));

	return true;
}

/* -------------------------------------------------------------------------- */

bool connection_all_test(void) {
	bool res = true;

	if (!multiple_connection_test() ||
		!invalid_connection_address_test()) {

		res = false;
	}
	return res;
}