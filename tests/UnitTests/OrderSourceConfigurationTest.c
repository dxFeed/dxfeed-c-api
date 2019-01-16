#ifdef _WIN32
#include <Windows.h>
#endif
#include <time.h>
#include <stdio.h>

#include "OrderSourceConfigurationTest.h"
#include "DXTypes.h"
#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "EventData.h"
#include "DXAlgorithms.h"
#include "TestHelper.h"

//Timeout in milliseconds for waiting some events is 3 minutes.
#define EVENTS_TIMEOUT 180000
#define EVENTS_LOOP_SLEEP_TIME 100
//The maximum milliseconds interval of expecting between two events
#define WAIT_EVENTS_INTERVAL 200

#define DXFEED_HOST "mddqa.in.devexperts.com:7400"

typedef int(*get_counter_function_t)();

dxf_listener_thread_data_t g_ost_listener_thread_data = NULL;
event_counter_data_t counter_data_ntv;
event_counter_data_t counter_data_dex;
event_counter_data_t counter_data_dea;
event_counter_data_ptr_t g_ntv_counter = &counter_data_ntv;
event_counter_data_ptr_t g_dex_counter = &counter_data_dex;
event_counter_data_ptr_t g_dea_counter = &counter_data_dea;

int ost_get_ntv_counter() { return get_event_counter(g_ntv_counter); }
int ost_get_dex_counter() { return get_event_counter(g_dex_counter); }
int ost_get_dea_counter() { return get_event_counter(g_dea_counter); }
get_counter_function_t g_all_counters[] = { ost_get_ntv_counter, ost_get_dex_counter, ost_get_dea_counter };

/* -------------------------------------------------------------------------- */

static void close_data(dxf_connection_t connection, dxf_subscription_t subscription,
					dxf_event_listener_t listener) {
	if (!dxf_detach_event_listener(subscription, listener))
		process_last_error();
	if (!dxf_close_subscription(subscription))
		process_last_error();
	if (!dxf_close_connection(connection))
		process_last_error();
}

/* -------------------------------------------------------------------------- */

void order_source_tests_on_thread_terminate(dxf_connection_t connection, void* user_data) {
	if (g_ost_listener_thread_data == NULL)
		return;
	on_reader_thread_terminate(g_ost_listener_thread_data, connection, user_data);
}

/* -------------------------------------------------------------------------- */

void listener(int event_type, dxf_const_string_t symbol_name,
			const dxf_event_data_t* data, int data_count, void* user_data) {
	dxf_int_t i = 0;
	dxf_order_t* orders = (dxf_order_t*)data;

	for (; i < data_count; ++i) {
		if (dx_compare_strings(orders[i].source, L"NTV") == 0)
			inc_event_counter(g_ntv_counter);
		else if (dx_compare_strings(orders[i].source, L"DEX") == 0)
			inc_event_counter(g_dex_counter);
		else if (dx_compare_strings(orders[i].source, L"DEA") == 0)
			inc_event_counter(g_dea_counter);
	}
}

bool ost_wait_events(get_counter_function_t counter_function) {
	int timestamp = dx_millisecond_timestamp();
	while (counter_function() == 0) {
		if (is_thread_terminate(g_ost_listener_thread_data)) {
			printf("Test failed: thread was terminated!\n");
			return false;
		}
		if (dx_millisecond_timestamp_diff(dx_millisecond_timestamp(), timestamp) > EVENTS_TIMEOUT) {
			printf("Test failed: timeout is elapsed!");
			return false;
		}
		Sleep(EVENTS_LOOP_SLEEP_TIME);
	}
	return true;
}

bool ost_wait_multiple_events(get_counter_function_t* counter_functions, int function_number) {
	int timestamp = dx_millisecond_timestamp();
	while (true) {
		int i;
		bool res = true;
		for (i = 0; i < function_number; i++) {
			res &= counter_functions[i]() > 0;
		}
		if (res)
			return true;
		if (is_thread_terminate(g_ost_listener_thread_data)) {
			printf("Test failed: thread was terminated!\n");
			return false;
		}
		if (dx_millisecond_timestamp_diff(dx_millisecond_timestamp(), timestamp) > EVENTS_TIMEOUT) {
			printf("Test failed: timeout is elapsed!");
			return false;
		}
		Sleep(EVENTS_LOOP_SLEEP_TIME);
	}
	return true;
}

bool ost_wait_two_events(get_counter_function_t f1, get_counter_function_t f2) {
	get_counter_function_t counters[] = { f1, f2 };
	return ost_wait_multiple_events(counters, SIZE_OF_ARRAY(counters));
}

/*
 * Function wait while all events in the input buffer will be handled. It is
 * useful when expected a large amount of data after subscribing.
 *
 * counter_data - event counter data object
 */
static bool wait_all_events_received(event_counter_data_ptr_t counter_data) {
	int timestamp = dx_millisecond_timestamp();
	int prev_value = 0;
	int last_value = 0;
	while ((last_value = get_event_counter(counter_data)) != prev_value) {
		prev_value = last_value;
		if (is_thread_terminate(g_ost_listener_thread_data)) {
			printf("Test failed: thread was terminated!\n");
			return false;
		}
		if (dx_millisecond_timestamp_diff(dx_millisecond_timestamp(), timestamp) > EVENTS_TIMEOUT) {
			printf("Test failed: timeout is elapsed! Interval of waiting "
				"'%s' events possible too much...",
				get_event_counter_name(counter_data));
			return false;
		}
		Sleep(WAIT_EVENTS_INTERVAL);
	}
	return true;
}

/*
 * Test
 *
 * Test a mixed set of function as dxf_set_order_source as dxf_add_order_source.
 * - Perform default subscription on symbols and sources
 * - Wait events with NTV, DEX and DEA sources.
 * - Sets a new sources: NTV only
 * - Wait events with NTV sources and checks that count of DEX and DEA is zero.
 * - Adds a new source to current set: DEX
 * - Wait events with NTV and DEX sources and checks that count of DEA is zero.
 */
bool mixed_order_source_test() {
	dxf_connection_t connection = NULL;
	dxf_subscription_t subscription = NULL;
	dxf_const_string_t symbols[] = { { L"IBM" }, { L"MSFT" }, { L"YHOO" }, { L"C" }, { L"AAPL" } };
	dxf_int_t symbol_count = sizeof(symbols) / sizeof(symbols[0]);

	drop_event_counter(g_ntv_counter);
	drop_event_counter(g_dex_counter);
	drop_event_counter(g_dea_counter);

	if (!dxf_create_connection(DXFEED_HOST, order_source_tests_on_thread_terminate, NULL, NULL, NULL, NULL, &connection) ||
		!dxf_create_subscription(connection, DXF_ET_ORDER, &subscription) ||
		!dxf_add_symbols(subscription, symbols, symbol_count) ||
		!dxf_attach_event_listener(subscription, listener, NULL)) {

		process_last_error();
		return false;
	}

	if (!ost_wait_multiple_events(g_all_counters, SIZE_OF_ARRAY(g_all_counters))) {
		process_last_error();
		close_data(connection, subscription, listener);
		return false;
	}

	if (!dxf_set_order_source(subscription, "NTV")) {
		process_last_error();
		PRINT_TEST_FAILED;
		close_data(connection, subscription, listener);
		return false;
	}
	if (!wait_all_events_received(g_ntv_counter) ||
		!wait_all_events_received(g_dex_counter) ||
		!wait_all_events_received(g_dea_counter)) {

		PRINT_TEST_FAILED;
		close_data(connection, subscription, listener);
		return false;
	}
	drop_event_counter(g_ntv_counter);
	drop_event_counter(g_dex_counter);
	drop_event_counter(g_dea_counter);
	if (!ost_wait_events(ost_get_ntv_counter) ||
		!dx_is_equal_int(0, ost_get_dex_counter()) ||
		!dx_is_equal_int(0, ost_get_dea_counter())) {

		printf("at %s, line: %d\n"
				"    Expected: ntv=%5s, dex=%5s, dea=%5s; \n"
				"    But was:  ntv=%5d, dex=%5d, dea=%5d.\n\n",
				__FUNCTION__, __LINE__, ">0", "0", "0",
				ost_get_ntv_counter(), ost_get_dex_counter(), ost_get_dea_counter());
		close_data(connection, subscription, listener);
		return false;
	}

	if (!dxf_add_order_source(subscription, "DEX")) {
		process_last_error();
		PRINT_TEST_FAILED;
		close_data(connection, subscription, listener);
		return false;
	}
	if (!wait_all_events_received(g_ntv_counter) ||
		!wait_all_events_received(g_dex_counter) ||
		!wait_all_events_received(g_dea_counter)) {

		PRINT_TEST_FAILED;
		close_data(connection, subscription, listener);
		return false;
	}
	drop_event_counter(g_ntv_counter);
	drop_event_counter(g_dex_counter);
	drop_event_counter(g_dea_counter);
	if (!ost_wait_two_events(ost_get_ntv_counter, ost_get_dex_counter) ||
		!dx_is_equal_int(0, ost_get_dea_counter())) {

		printf("at %s, line: %d\n"
				"    Expected: ntv=%5s, dex=%5s, dea=%5s; \n"
				"    But was:  ntv=%5d, dex=%5d, dea=%5d.\n\n",
				__FUNCTION__, __LINE__, ">0", ">0", "0",
				ost_get_ntv_counter(), ost_get_dex_counter(), ost_get_dea_counter());
		close_data(connection, subscription, listener);
		return false;
	}

	if (!dxf_detach_event_listener(subscription, listener) ||
		!dxf_close_subscription(subscription) ||
		!dxf_close_connection(connection)) {

		process_last_error();
		return false;
	}
	return true;
}

/*
 * Test
 *
 * Test a set function: dxf_set_order_source.
 * - Perform default subscription on symbols and sources
 * - Wait events with NTV, DEX and DEA sources.
 * - Sets a new sources: NTV only
 * - Wait events with NTV sources and checks that count of DEX and DEA is zero.
 * - Sets a new sources: DEX only
 * - Wait events with DEX sources and checks that count of NTV and DEA is zero.
 */
bool set_order_source_test() {
	dxf_connection_t connection = NULL;
	dxf_subscription_t subscription = NULL;
	dxf_const_string_t symbols[] = { { L"IBM" }, { L"MSFT" }, { L"YHOO" }, { L"C" }, { L"AAPL" } };
	dxf_int_t symbol_count = sizeof(symbols) / sizeof(symbols[0]);

	drop_event_counter(g_ntv_counter);
	drop_event_counter(g_dex_counter);
	drop_event_counter(g_dea_counter);

	if (!dxf_create_connection(DXFEED_HOST, order_source_tests_on_thread_terminate, NULL, NULL, NULL, NULL, &connection) ||
		!dxf_create_subscription(connection, DXF_ET_ORDER, &subscription) ||
		!dxf_add_symbols(subscription, symbols, symbol_count) ||
		!dxf_attach_event_listener(subscription, listener, NULL)) {

		process_last_error();
		return false;
	}

	if (!ost_wait_multiple_events(g_all_counters, SIZE_OF_ARRAY(g_all_counters))) {
		process_last_error();
		close_data(connection, subscription, listener);
		return false;
	}

	if (!dxf_set_order_source(subscription, "NTV")) {
		process_last_error();
		PRINT_TEST_FAILED;
		close_data(connection, subscription, listener);
		return false;
	}
	if (!wait_all_events_received(g_ntv_counter) ||
		!wait_all_events_received(g_dex_counter) ||
		!wait_all_events_received(g_dea_counter)) {

		PRINT_TEST_FAILED;
		close_data(connection, subscription, listener);
		return false;
	}
	drop_event_counter(g_ntv_counter);
	drop_event_counter(g_dex_counter);
	drop_event_counter(g_dea_counter);
	if (!ost_wait_events(ost_get_ntv_counter) ||
		!dx_is_equal_int(0, ost_get_dex_counter()) ||
		!dx_is_equal_int(0, ost_get_dea_counter())) {

		printf("at %s, line: %d\n"
				"    Expected: ntv=%5s, dex=%5s, dea=%5s; \n"
				"    But was:  ntv=%5d, dex=%5d, dea=%5d.\n\n",
				__FUNCTION__, __LINE__, ">0", "0", "0",
				ost_get_ntv_counter(), ost_get_dex_counter(), ost_get_dea_counter());
		close_data(connection, subscription, listener);
		return false;
	}

	if (!dxf_set_order_source(subscription, "DEX")) {
		process_last_error();
		PRINT_TEST_FAILED;
		close_data(connection, subscription, listener);
		return false;
	}
	if (!wait_all_events_received(g_ntv_counter) ||
		!wait_all_events_received(g_dex_counter) ||
		!wait_all_events_received(g_dea_counter)) {

		PRINT_TEST_FAILED;
		close_data(connection, subscription, listener);
		return false;
	}
	drop_event_counter(g_ntv_counter);
	drop_event_counter(g_dex_counter);
	drop_event_counter(g_dea_counter);
	if (!ost_wait_events(ost_get_dex_counter) ||
		!dx_is_equal_int(0, ost_get_ntv_counter()) ||
		!dx_is_equal_int(0, ost_get_dea_counter())) {

		printf("at %s, line: %d\n"
				"    Expected: ntv=%5s, dex=%5s, dea=%5s; \n"
				"    But was:  ntv=%5d, dex=%5d, dea=%5d.\n\n",
				__FUNCTION__, __LINE__, "0", ">0", "0",
				ost_get_ntv_counter(), ost_get_dex_counter(), ost_get_dea_counter());
		close_data(connection, subscription, listener);
		return false;
	}

	if (!dxf_detach_event_listener(subscription, listener) ||
		!dxf_close_subscription(subscription) ||
		!dxf_close_connection(connection)) {

		process_last_error();
		return false;
	}
	return true;
}

/*
 * Test
 *
 * Test a add function: dxf_add_order_source.
 * - Perform subscription on symbols and NTV source only
 * - Wait events with NTV sources and checks that count of DEX and DEA is zero.
 * - Adds a new source to current set: DEX
 * - Wait events with NTV and DEX sources and checks that count of DEA is zero.
 * - Adds a new source to current set: DEA
 * - Wait events with NTV, DEX and DEA sources.
 */
bool add_order_source_test() {
	dxf_connection_t connection = NULL;
	dxf_subscription_t subscription = NULL;
	dxf_const_string_t symbols[] = { L"IBM", L"MSFT", L"YHOO", L"C", L"AAPL", L"XBT/USD" };
	dxf_int_t symbol_count = sizeof(symbols) / sizeof(symbols[0]);

	drop_event_counter(g_ntv_counter);
	drop_event_counter(g_dex_counter);
	drop_event_counter(g_dea_counter);

	if (!dxf_create_connection(DXFEED_HOST, order_source_tests_on_thread_terminate, NULL, NULL, NULL, NULL, &connection) ||
		!dxf_create_subscription(connection, DXF_ET_ORDER, &subscription) ||
		!dxf_set_order_source(subscription, "NTV") ||
		!dxf_add_symbols(subscription, symbols, symbol_count) ||
		!dxf_attach_event_listener(subscription, listener, NULL)) {

		process_last_error();
		return false;
	}

	if (!ost_wait_events(ost_get_ntv_counter) ||
		!dx_is_equal_int(0, ost_get_dex_counter()) ||
		!dx_is_equal_int(0, ost_get_dea_counter())) {

		printf("at %s, line: %d\n"
				"    Expected: ntv=%5s, dex=%5s, dea=%5s; \n"
				"    But was:  ntv=%5d, dex=%5d, dea=%5d.\n\n",
				__FUNCTION__, __LINE__, ">0", "0", "0",
				ost_get_ntv_counter(), ost_get_dex_counter(), ost_get_dea_counter());
		close_data(connection, subscription, listener);
		return false;
	}

	if (!dxf_add_order_source(subscription, "DEX")) {
		process_last_error();
		PRINT_TEST_FAILED;
		close_data(connection, subscription, listener);
		return false;
	}
	if (!wait_all_events_received(g_ntv_counter) ||
		!wait_all_events_received(g_dex_counter) ||
		!wait_all_events_received(g_dea_counter)) {

		PRINT_TEST_FAILED;
		close_data(connection, subscription, listener);
		return false;
	}
	drop_event_counter(g_ntv_counter);
	drop_event_counter(g_dex_counter);
	drop_event_counter(g_dea_counter);
	if (!ost_wait_two_events(ost_get_ntv_counter, ost_get_dex_counter) ||
		!dx_is_equal_int(0, ost_get_dea_counter())) {

		printf("at %s, line: %d\n"
				"    Expected: ntv=%5s, dex=%5s, dea=%5s; \n"
				"    But was:  ntv=%5d, dex=%5d, dea=%5d.\n\n",
				__FUNCTION__, __LINE__, ">0", ">0", "0",
				ost_get_ntv_counter(), ost_get_dex_counter(), ost_get_dea_counter());
		close_data(connection, subscription, listener);
		return false;
	}

	if (!dxf_add_order_source(subscription, "DEA")) {
		process_last_error();
		PRINT_TEST_FAILED;
		close_data(connection, subscription, listener);
		return false;
	}
	if (!wait_all_events_received(g_ntv_counter) ||
		!wait_all_events_received(g_dex_counter) ||
		!wait_all_events_received(g_dea_counter)) {

		PRINT_TEST_FAILED;
		close_data(connection, subscription, listener);
		return false;
	}
	drop_event_counter(g_ntv_counter);
	drop_event_counter(g_dex_counter);
	drop_event_counter(g_dea_counter);
	if (!ost_wait_multiple_events(g_all_counters, SIZE_OF_ARRAY(g_all_counters))) {
		printf("at %s, line: %d\n"
				"    Expected: ntv=%5s, dex=%5s, dea=%5s; \n"
				"    But was:  ntv=%5d, dex=%5d, dea=%5d.\n\n",
				__FUNCTION__, __LINE__, ">0", ">0", ">0",
				ost_get_ntv_counter(), ost_get_dex_counter(), ost_get_dea_counter());
		close_data(connection, subscription, listener);
		return false;
	}

	if (!dxf_detach_event_listener(subscription, listener) ||
		!dxf_close_subscription(subscription) ||
		!dxf_close_connection(connection)) {

		process_last_error();
		return false;
	}
	return true;
}

/*
 * Test
 *
 * Test a mixed set of function as dxf_set_order_source as dxf_add_order_source.
 * - Perform default subscription on symbols and sources
 * - Wait events with NTV, DEX and DEA sources.
 * - Sets a new sources: NTV only
 * - Wait events with NTV sources and checks that count of DEX and DEA is zero.
 * - Adds a new source to current set: DEX
 * - Wait events with NTV and DEX sources and checks that count of DEA is zero.
 */
bool input_order_source_test() {
	dxf_connection_t connection = NULL;
	dxf_subscription_t subscription = NULL;
	dxf_const_string_t symbols[] = { { L"IBM" },{ L"MSFT" },{ L"YHOO" },{ L"C" },{ L"AAPL" } };
	dxf_int_t symbol_count = sizeof(symbols) / sizeof(symbols[0]);

	if (!dxf_create_connection(DXFEED_HOST, order_source_tests_on_thread_terminate, NULL, NULL, NULL, NULL, &connection) ||
		!dxf_create_subscription(connection, DXF_ET_ORDER, &subscription)) {

		process_last_error();
		return false;
	}

	if (
		//Try to add order source with NULL subscription
		dxf_add_order_source(NULL, "NTV") ||
		//Try to add NULL order source
		dxf_add_order_source(subscription, NULL) ||
		//Try to add empty-string order source
		dxf_add_order_source(subscription, "") ||
		//Try to add very-long-string order source
		dxf_add_order_source(subscription, "abcde") ||
		//Try to set order source with NULL subscription
		dxf_set_order_source(NULL, "NTV") ||
		//Try to set NULL order source
		dxf_set_order_source(subscription, NULL) ||
		//Try to set empty-string order source
		dxf_set_order_source(subscription, "") ||
		//Try to set very-long-string order source
		dxf_set_order_source(subscription, "abcde")
		) {

		PRINT_TEST_FAILED;
		close_data(connection, subscription, listener);
		return false;
	}

	if (!dxf_close_subscription(subscription) ||
		!dxf_close_connection(connection)) {

		process_last_error();
		return false;
	}
	return true;
}

bool order_source_configuration_test(void) {
	bool res = true;

	dxf_initialize_logger("log.log", true, true, true);

	init_listener_thread_data(&g_ost_listener_thread_data);
	init_event_counter2(g_ntv_counter, "NTV");
	init_event_counter2(g_dex_counter, "DEX");
	init_event_counter2(g_dea_counter, "DEA");

	if (!input_order_source_test() ||
		!add_order_source_test() ||
		!set_order_source_test() ||
		!mixed_order_source_test()) {

		res &= false;
	}

	free_event_counter(g_dea_counter);
	free_event_counter(g_dex_counter);
	free_event_counter(g_ntv_counter);
	free_listener_thread_data(g_ost_listener_thread_data);

	return res;
}
