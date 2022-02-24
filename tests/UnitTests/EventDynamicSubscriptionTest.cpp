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
#include <time.h>

#pragma warning(push)
#pragma warning(disable : 5105)
#include <Windows.h>
#pragma warning(pop)

#include "EventDynamicSubscriptionTest.h"
#include "DXAlgorithms.h"
#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "Logger.h"
#include "TestHelper.h"

//Timeout in milliseconds for waiting some events is 2 minutes.
#define EVENTS_TIMEOUT 120000
#define EVENTS_LOOP_SLEEP_TIME 100

const char dxfeed_host[] = "mddqa.in.devexperts.com:7400";

#define SYMBOLS_COUNT 4
static dxf_const_string_t g_symbols[] = { { L"IBM" }, { L"AAPL" }, { L"XBT/USD" }, { L"C" } };

dxf_listener_thread_data_t g_listener_thread_data;

/* -------------------------------------------------------------------------- */

static dxf_uint_t g_trade_event_counter = 0;
CRITICAL_SECTION g_trade_counter_guard;
static dxf_uint_t g_order_event_counter = 0;
CRITICAL_SECTION g_order_counter_guard;

void inc_order_counter() {
	EnterCriticalSection(&g_order_counter_guard);
	g_order_event_counter++;
	LeaveCriticalSection(&g_order_counter_guard);
}

dxf_uint_t get_order_counter() {
	dxf_uint_t value = 0;
	EnterCriticalSection(&g_order_counter_guard);
	value = g_order_event_counter;
	LeaveCriticalSection(&g_order_counter_guard);
	return value;
}

void drop_order_counter() {
	EnterCriticalSection(&g_order_counter_guard);
	g_order_event_counter = 0;
	LeaveCriticalSection(&g_order_counter_guard);
}

/* -------------------------------------------------------------------------- */

void inc_trade_counter() {
	EnterCriticalSection(&g_trade_counter_guard);
	g_trade_event_counter++;
	LeaveCriticalSection(&g_trade_counter_guard);
}

dxf_uint_t get_trade_counter() {
	dxf_uint_t value = 0;
	EnterCriticalSection(&g_trade_counter_guard);
	value = g_trade_event_counter;
	LeaveCriticalSection(&g_trade_counter_guard);
	return value;
}

void drop_trade_counter() {
	EnterCriticalSection(&g_trade_counter_guard);
	g_trade_event_counter = 0;
	LeaveCriticalSection(&g_trade_counter_guard);
}

/* -------------------------------------------------------------------------- */

void print_timestamp(dxf_long_t timestamp){
	char timefmt[80];

	struct tm * timeinfo;
	time_t tmpint = (int)(timestamp / 1000);
	timeinfo = localtime(&tmpint);
	strftime(timefmt, 80, "%Y%m%d-%H%M%S", timeinfo);
	printf("%s", timefmt);
}
/* -------------------------------------------------------------------------- */

static dxf_uint_t buf_hash(const unsigned char *buf, int size) {
	dxf_uint_t hash = 0;
	dxf_int_t c;
	int i;

	for (i = 0; i < size; i++) {
		c = buf[i];
		hash = c + (hash << 6) + (hash << 16) - hash;
	}

	return hash;
}

/* -------------------------------------------------------------------------- */

typedef struct {
	int event_type;
	dxf_string_t symbol;
	dxf_event_params_t* params;
	dxf_uint_t obj_hash;
	int data_count;
	void *user_data;

	int is_received;
	CRITICAL_SECTION guard;
} event_listener_data_t;

event_listener_data_t order_data;
event_listener_data_t order_data_v2;

void reset_event_listener_data(event_listener_data_t* listener_data) {
	if (listener_data->symbol != NULL) {
		dx_free(listener_data->symbol);
		listener_data->symbol = NULL;
	}
	if (listener_data->params != NULL) {
		dx_free(listener_data->params);
		listener_data->params = NULL;
	}
	listener_data->event_type = 0;
	listener_data->obj_hash = 0;
	listener_data->data_count = 0;
	listener_data->user_data = NULL;
	listener_data->is_received = false;
}

void init_event_listener_data(event_listener_data_t* listener_data) {
	memset((void*)listener_data, 0, sizeof(event_listener_data_t));
	InitializeCriticalSection(&(listener_data->guard));
}

void free_event_listener_data(event_listener_data_t* listener_data) {
	DeleteCriticalSection(&(listener_data->guard));
	reset_event_listener_data(listener_data);
}

int is_received_event_listener_data(event_listener_data_t* listener_data) {
	int is_received = false;
	EnterCriticalSection(&(listener_data->guard));
	is_received = listener_data->is_received;
	LeaveCriticalSection(&(listener_data->guard));
	return is_received;
}

void set_event_listener_data(int event_type, dxf_const_string_t symbol_name,
							const dxf_event_params_t* params, const dxf_event_data_t* data,
							int data_count, void* user_data, int event_size,
							event_listener_data_t* listener_data) {
	EnterCriticalSection(&(listener_data->guard));
	reset_event_listener_data(listener_data);
	listener_data->event_type = event_type;
	listener_data->symbol = dx_create_string_src(symbol_name);
	if (params != NULL) {
		listener_data->params = dx_calloc(1, sizeof(event_listener_data_t));
		dx_memcpy(listener_data->params, params, sizeof(event_listener_data_t));
	}
	listener_data->obj_hash = buf_hash((const unsigned char*)data, event_size * data_count);
	listener_data->data_count = data_count;
	listener_data->user_data = user_data;

	listener_data->is_received = true;
	LeaveCriticalSection(&(listener_data->guard));
}

/* -------------------------------------------------------------------------- */

void trade_listener(int event_type, dxf_const_string_t symbol_name,
					const dxf_event_data_t* data, int data_count, void* user_data) {
	inc_trade_counter();
}

void order_listener(int event_type, dxf_const_string_t symbol_name,
					const dxf_event_data_t* data, int data_count, void* user_data) {
	inc_order_counter();
}

/* -------------------------------------------------------------------------- */

void order_event_copy_listener(int event_type, dxf_const_string_t symbol_name,
							const dxf_event_data_t* data, int data_count, void* user_data) {
	if (order_data.is_received)
		return;
	set_event_listener_data(event_type, symbol_name, NULL, data, data_count, user_data, sizeof(dxf_order_t), &order_data);
}

void order_event_copy_listener_v2(int event_type, dxf_const_string_t symbol_name,
								const dxf_event_data_t* data, int data_count,
								const dxf_event_params_t* params, void* user_data) {
	if (order_data_v2.is_received)
		return;
	set_event_listener_data(event_type, symbol_name, params, data, data_count, user_data, sizeof(dxf_order_t), &order_data_v2);
}

/* -------------------------------------------------------------------------- */

int subscribe_to_event(dxf_connection_t connection, dxf_subscription_t* subscription,
						int event_type, dxf_event_listener_t event_listener, void* user_data) {
	if (!dxf_create_subscription(connection, event_type, subscription)) {
		process_last_error();

		return false;
	};

	if (!dxf_add_symbols(*subscription, g_symbols, SYMBOLS_COUNT)) {
		process_last_error();

		return false;
	};

	if (!dxf_attach_event_listener(*subscription, event_listener, user_data)) {
		process_last_error();

		return false;
	};

	return true;
}

/* -------------------------------------------------------------------------- */

void on_thread_terminate(dxf_connection_t connection, void* user_data) {
	if (g_listener_thread_data == NULL)
		return;
	on_reader_thread_terminate(g_listener_thread_data, connection, user_data);
}

/* -------------------------------------------------------------------------- */

/*Test*/
int event_dynamic_subscription_test(void) {
	dxf_connection_t connection = NULL;
	dxf_subscription_t trade_subscription = NULL;
	dxf_subscription_t order_subscription = NULL;
	int loop_counter = EVENTS_TIMEOUT / EVENTS_LOOP_SLEEP_TIME;
	int timestamp = 0;

	reset_thread_terminate(g_listener_thread_data);

	if (!dxf_create_connection(dxfeed_host, on_thread_terminate, NULL, NULL, NULL, NULL, &connection)) {
		process_last_error();
		return false;
	}

	//Subscribe on trade events
	if (!subscribe_to_event(connection, &trade_subscription, DXF_ET_TRADE, trade_listener, NULL)) {
		dxf_close_connection(connection);
		return false;
	}

	//Wait trade events
	timestamp = dx_millisecond_timestamp();
	while (get_trade_counter() == 0) {
		if (is_thread_terminate(g_listener_thread_data)) {
			dxf_close_subscription(trade_subscription);
			dxf_close_connection(connection);
			printf("Error: Thread was terminated!\n");
			PRINT_TEST_FAILED;
			return false;
		}
		if (dx_millisecond_timestamp_diff(dx_millisecond_timestamp(), timestamp) > EVENTS_TIMEOUT) {
			dxf_close_subscription(trade_subscription);
			dxf_close_connection(connection);
			printf("Test failed: timeout is elapsed!");
			PRINT_TEST_FAILED;
			return false;
		}
		Sleep(EVENTS_LOOP_SLEEP_TIME);
	}

	//Subscribe on order events
	if (!subscribe_to_event(connection, &order_subscription, DXF_ET_ORDER, order_listener, NULL)) {
		dxf_close_subscription(trade_subscription);
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}

	drop_order_counter();
	drop_trade_counter();

	//Check trade and order events
	timestamp = dx_millisecond_timestamp();
	while (get_trade_counter() == 0 || get_order_counter() == 0) {
		if (is_thread_terminate(g_listener_thread_data)) {
			dxf_close_subscription(trade_subscription);
			dxf_close_subscription(order_subscription);
			dxf_close_connection(connection);
			printf("Error: Thread was terminated!\n");
			PRINT_TEST_FAILED;
			return false;
		}
		if (dx_millisecond_timestamp_diff(dx_millisecond_timestamp(), timestamp) > EVENTS_TIMEOUT) {
			dxf_close_subscription(trade_subscription);
			dxf_close_subscription(order_subscription);
			dxf_close_connection(connection);
			printf("Test failed: timeout is elapsed!");
			PRINT_TEST_FAILED;
			return false;
		}
		Sleep(EVENTS_LOOP_SLEEP_TIME);
	}

	//Unsubscribe trade events
	dxf_detach_event_listener(trade_subscription, trade_listener);
	drop_order_counter();
	drop_trade_counter();

	//Check order events
	while (loop_counter--) {
		if (is_thread_terminate(g_listener_thread_data)) {
			dxf_close_subscription(trade_subscription);
			dxf_close_subscription(order_subscription);
			dxf_close_connection(connection);
			printf("Error: Thread was terminated!\n");
			PRINT_TEST_FAILED;
			return false;
		}
		Sleep(EVENTS_LOOP_SLEEP_TIME);
	}
	if (get_order_counter() == 0 || get_trade_counter() > 0) {
		dxf_close_subscription(trade_subscription);
		dxf_close_subscription(order_subscription);
		dxf_close_connection(connection);
		printf("Test failed: no order events or trade events is occur!");
		PRINT_TEST_FAILED;
		return false;
	}

	dxf_detach_event_listener(order_subscription, order_listener);

	dxf_close_subscription(trade_subscription);
	dxf_close_subscription(order_subscription);

	if (!dxf_close_connection(connection)) {
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}

	return true;
}

/*Test*/
int listener_v2_test(void) {
	dxf_connection_t connection = NULL;
	dxf_subscription_t order_subscription = NULL;
	int timestamp = 0;
	int user_data = 1;
	int res = true;

	reset_thread_terminate(g_listener_thread_data);

	if (!dxf_create_connection(dxfeed_host, on_thread_terminate, NULL, NULL, NULL, NULL, &connection)) {
		process_last_error();
		return false;
	}

	drop_order_counter();

	//Subscribe on order events
	if (!subscribe_to_event(connection, &order_subscription, DXF_ET_ORDER, order_event_copy_listener, &user_data)) {
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}
	if (!dxf_attach_event_listener_v2(order_subscription, order_event_copy_listener_v2, (void*)&user_data)) {
		dxf_close_subscription(order_subscription);
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		process_last_error();
		return false;
	};

	//Wait order events
	timestamp = dx_millisecond_timestamp();
	while (!is_received_event_listener_data(&order_data) || !is_received_event_listener_data(&order_data_v2)) {
		if (is_thread_terminate(g_listener_thread_data)) {
			dxf_close_subscription(order_subscription);
			dxf_close_connection(connection);
			printf("Error: Thread was terminated!\n");
			PRINT_TEST_FAILED;
			return false;
		}
		if (dx_millisecond_timestamp_diff(dx_millisecond_timestamp(), timestamp) > EVENTS_TIMEOUT) {
			dxf_close_subscription(order_subscription);
			dxf_close_connection(connection);
			printf("Test failed: timeout is elapsed!");
			PRINT_TEST_FAILED;
			return false;
		}
		Sleep(EVENTS_LOOP_SLEEP_TIME);
	}

	if (!dx_is_equal_int(order_data.event_type, order_data_v2.event_type) ||
		!dx_is_equal_dxf_const_string_t(order_data.symbol, order_data_v2.symbol) ||
		!dx_is_equal_dxf_uint_t(order_data.obj_hash, order_data_v2.obj_hash) ||
		!dx_is_equal_int(order_data.data_count, order_data_v2.data_count) ||
		!dx_is_equal_ptr(order_data.user_data, order_data_v2.user_data) ||
		!dx_is_not_null(order_data_v2.params)) {

		res = false;
	}

	if (!dxf_close_subscription(order_subscription)) {
		dxf_close_connection(connection);
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}

	if (!dxf_close_connection(connection)) {
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}

	return res;
}

int event_dynamic_subscription_all_test(void) {
	int res = true;

	dxf_initialize_logger("log.log", true, true, true);
	init_listener_thread_data(&g_listener_thread_data);
	InitializeCriticalSection(&g_order_counter_guard);
	InitializeCriticalSection(&g_trade_counter_guard);
	init_event_listener_data(&order_data);
	init_event_listener_data(&order_data_v2);

	if (!event_dynamic_subscription_test() ||
		!listener_v2_test()) {
		res = false;
	}

	free_event_listener_data(&order_data_v2);
	free_event_listener_data(&order_data);
	free_listener_thread_data(g_listener_thread_data);
	DeleteCriticalSection(&g_order_counter_guard);
	DeleteCriticalSection(&g_trade_counter_guard);

	return res;
}