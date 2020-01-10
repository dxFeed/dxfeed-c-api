#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include "Candle.h"
#include "CandleTest.h"
#include "DXAlgorithms.h"
#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "EventData.h"
#include "TestHelper.h"

#define CANDLE_DEFAULT_SYMBOL L"AAPL"
#define CANDLE_USER_EXCHANGE L'A'
#define CANDLE_USER_PERIOD_VALUE 2
#define CANDLE_USER_PRICE_LEVEL_VALUE 0.1

#define CANDLE_IBM_SYMBOL L"IBM"

#define CANDLE_DEFAULT_FULL_SYMBOL CANDLE_DEFAULT_SYMBOL L"{=d,price=mark}"
#define CANDLE_IBM_FULL_SYMBOL CANDLE_IBM_SYMBOL L"{=d,price=mark}"

//Timeout in milliseconds for waiting some events is 2 minutes.
#define EVENTS_TIMEOUT 120000
#define EVENTS_LOOP_SLEEP_TIME 100

#define EPSILON 0.000001

static const char dxfeed_host[] = "mddqa.in.devexperts.com:7400";

candle_attribute_test_case_t g_candle_attribute_cases[] = {
	{ CANDLE_DEFAULT_SYMBOL, DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
	dxf_ctpa_default, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
		dxf_ctpa_default, dxf_cpa_default, dxf_csa_default, dxf_caa_default, CANDLE_USER_PRICE_LEVEL_VALUE, L"AAPL{pl=0.1}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
	dxf_ctpa_default, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A", __LINE__ },

	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
	dxf_ctpa_second, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=s}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
	dxf_ctpa_minute, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=m}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
		dxf_ctpa_minute, dxf_cpa_default, dxf_csa_default, dxf_caa_default, CANDLE_USER_PRICE_LEVEL_VALUE, L"AAPL&A{=m,pl=0.1}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
	dxf_ctpa_hour, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=h}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
	dxf_ctpa_day, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=d}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
	dxf_ctpa_week, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=w}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
	dxf_ctpa_month, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=mo}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
	dxf_ctpa_optexp, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=o}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
	dxf_ctpa_year, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=y}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
	dxf_ctpa_volume, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=v}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
	dxf_ctpa_price, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=p}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
	dxf_ctpa_price_momentum, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=pm}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
	dxf_ctpa_price_renko, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=pr}", __LINE__ },

	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_default, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2t}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_second, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2s}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_minute, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2m}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_hour, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2h}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_day, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2d}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_week, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2w}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_month, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2mo}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_optexp, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2o}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_year, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2y}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_volume, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2v}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_price, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2p}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_price_momentum, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2pm}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_price_renko, dxf_cpa_default, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2pr}", __LINE__ },

	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_day, dxf_cpa_bid, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2d,price=bid}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_day, dxf_cpa_ask, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2d,price=ask}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_day, dxf_cpa_mark, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2d,price=mark}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_day, dxf_cpa_settlement, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2d,price=s}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
		dxf_ctpa_day, dxf_cpa_settlement, dxf_csa_default, dxf_caa_default, CANDLE_USER_PRICE_LEVEL_VALUE, L"AAPL&A{=2d,pl=0.1,price=s}", __LINE__ },

	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_day, dxf_cpa_mark, dxf_csa_regular, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2d,price=mark,tho=true}", __LINE__ },

	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
	dxf_ctpa_day, dxf_cpa_mark, dxf_csa_regular, dxf_caa_session, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, L"AAPL&A{=2d,a=s,price=mark,tho=true}", __LINE__ },
	{ CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
			dxf_ctpa_day, dxf_cpa_mark, dxf_csa_regular, dxf_caa_session, CANDLE_USER_PRICE_LEVEL_VALUE, L"AAPL&A{=2d,a=s,pl=0.1,price=mark,tho=true}", __LINE__ }
};

static event_counter_data_t g_aapl_candle_event_counter_data;
static event_counter_data_t g_ibm_candle_event_counter_data;
static event_counter_data_t g_order_event_counter_data;
static event_counter_data_ptr_t g_aapl_candle_data = &g_aapl_candle_event_counter_data;
static event_counter_data_ptr_t g_ibm_candle_data = &g_ibm_candle_event_counter_data;
static event_counter_data_ptr_t g_order_data = &g_order_event_counter_data;
static dxf_candle_t g_last_candle = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0 };
dxf_listener_thread_data_t g_ct_listener_thread_data = NULL;

/* -------------------------------------------------------------------------- */

void candle_tests_on_thread_terminate(dxf_connection_t connection, void* user_data) {
	if (g_ct_listener_thread_data == NULL)
		return;
	on_reader_thread_terminate(g_ct_listener_thread_data, connection, user_data);
}

/* -------------------------------------------------------------------------- */

void aapl_candle_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data,
						int data_count, void* user_data) {
	dxf_candle_t* candles = NULL;

	/* symbol hardcoded */
	if (event_type != DXF_ET_CANDLE || wcscmp(symbol_name, L"AAPL{=d,price=mark}") != 0 || data_count < 1)
		return;
	inc_event_counter(g_aapl_candle_data);
	candles = (dxf_candle_t*)data;
	memcpy(&g_last_candle, &(candles[data_count - 1]), sizeof(dxf_candle_t));
}

void ibm_candle_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data,
						int data_count, void* user_data) {
	/* symbol hardcoded */
	if (event_type != DXF_ET_CANDLE || wcscmp(symbol_name, L"IBM{=d,price=mark}") != 0 || data_count < 1)
		return;
	inc_event_counter(g_ibm_candle_data);
}

void order_event_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data,
						int data_count, void* user_data) {
	if (event_type != DXF_ET_ORDER || data_count < 1)
		return;
	inc_event_counter(g_order_data);
}

void common_candle_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data,
							int data_count, void* user_data) {
	dxf_candle_t* candles = NULL;

	/* symbol hardcoded */
	if (event_type == DXF_ET_CANDLE && wcscmp(symbol_name, L"AAPL{=d,price=mark}") == 0 && data_count > 0) {
		inc_event_counter(g_aapl_candle_data);
		candles = (dxf_candle_t*)data;
		memcpy(&g_last_candle, &(candles[data_count - 1]), sizeof(dxf_candle_t));
	}
	if (event_type == DXF_ET_CANDLE && wcscmp(symbol_name, L"IBM{=d,price=mark}") == 0 && data_count > 0) {
		inc_event_counter(g_ibm_candle_data);
	}
}

/* -------------------------------------------------------------------------- */

bool add_symbol_to_existing_candle(dxf_subscription_t subscription, dxf_const_string_t symbol) {
	dxf_candle_attributes_t candle_attributes = NULL;

	if (!dxf_create_candle_symbol_attributes(symbol, DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT,
		DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT, dxf_ctpa_day,
		dxf_cpa_mark, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, &candle_attributes)) {

		process_last_error();
		return false;
	}

	if (!dxf_add_candle_symbol(subscription, candle_attributes)) {
		process_last_error();
		dxf_delete_candle_symbol_attributes(candle_attributes);
		return false;
	};

	dxf_delete_candle_symbol_attributes(candle_attributes);
	return true;
}

bool remove_symbol_from_existing_candle(dxf_subscription_t subscription, dxf_const_string_t symbol) {
	dxf_candle_attributes_t candle_attributes = NULL;

	if (!dxf_create_candle_symbol_attributes(symbol, DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT,
		DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT, dxf_ctpa_day,
		dxf_cpa_mark, dxf_csa_default, dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, &candle_attributes)) {

		process_last_error();
		return false;
	}

	if (!dxf_remove_candle_symbol(subscription, candle_attributes)) {
		process_last_error();
		dxf_delete_candle_symbol_attributes(candle_attributes);
		return false;
	};

	dxf_delete_candle_symbol_attributes(candle_attributes);
	return true;
}

bool create_candle_subscription(dxf_connection_t connection, dxf_const_string_t symbol,
								dxf_event_listener_t event_listener,
								OUT dxf_subscription_t* res_subscription) {
	dxf_subscription_t subscription = NULL;

	if (!dxf_create_subscription_timed(connection, DXF_ET_CANDLE, 0, &subscription)) {
		process_last_error();
		return false;
	};

	if (!add_symbol_to_existing_candle(subscription, symbol)) {
		process_last_error();
		dxf_close_subscription(subscription);
		return false;
	};

	if (!dxf_attach_event_listener(subscription, event_listener, NULL)) {
		process_last_error();
		dxf_close_subscription(subscription);
		return false;
	};

	*res_subscription = subscription;
	return true;
}

bool wait_events(int (*get_counter_function)()) {
	int timestamp = dx_millisecond_timestamp();
	while (get_counter_function() == 0) {
		if (is_thread_terminate(g_ct_listener_thread_data)) {
			printf("Error: Thread was terminated!\n");
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

int get_aapl_candle_counter() {
	return get_event_counter(g_aapl_candle_data);
}

int get_ibm_candle_counter() {
	return get_event_counter(g_ibm_candle_data);
}

int get_order_event_counter() {
	return get_event_counter(g_order_data);
}

/* -------------------------------------------------------------------------- */

/*Test*/
bool candle_attributes_test(void) {
	int candle_attribute_cases_size = sizeof(g_candle_attribute_cases) / sizeof(g_candle_attribute_cases[0]);
	int i;
	for (i = 0; i < candle_attribute_cases_size; i++) {
		dxf_candle_attributes_t attributes;
		candle_attribute_test_case_t* params = &g_candle_attribute_cases[i];
		dxf_string_t attributes_string = NULL;
		bool res = true;
		dxf_create_candle_symbol_attributes(params->symbol, params->exchange_code,
			params->period_value, params->period_type, params->price, params->session,
			params->alignment, params->price_level, &attributes);
		if (!dx_candle_symbol_to_string(attributes, &attributes_string)) {
			PRINT_TEST_FAILED;
			process_last_error();
			return false;
		}
		if (wcscmp(attributes_string, params->expected) != 0) {
			wprintf(L"The %ls failed on case #%d, line:%d! Expected: %ls, but was: %ls\n", __FUNCTIONW__, i, params->line, params->expected, attributes_string);
			res = false;
		}
		dxf_delete_candle_symbol_attributes(attributes);
		free(attributes_string);
		if (!res)
			return false;
	}
	return true;
}

/* -------------------------------------------------------------------------- */

/*Test*/
bool candle_subscription_test(void) {
	dxf_connection_t connection = NULL;
	dxf_subscription_t subscription = NULL;

	if (!dxf_create_connection(dxfeed_host, candle_tests_on_thread_terminate, NULL, NULL, NULL, NULL, &connection)) {
		PRINT_TEST_FAILED;
		process_last_error();
		return false;
	}

	drop_event_counter(g_aapl_candle_data);

	if (!create_candle_subscription(connection, CANDLE_DEFAULT_SYMBOL, aapl_candle_listener, &subscription)) {
		PRINT_TEST_FAILED;
		dxf_close_connection(connection);
		return false;
	}

	if (!wait_events(get_aapl_candle_counter)) {
		PRINT_TEST_FAILED;
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);
		return false;
	}

	if (!dx_ge_dxf_long_t(g_last_candle.time, 0) ||
		!dx_ge_double(g_last_candle.count, 0.0) ||
		!dx_ge_double(g_last_candle.open, 0.0) ||
		!dx_ge_double(g_last_candle.high, 0.0) ||
		!dx_ge_double(g_last_candle.low, 0.0) ||
		!dx_ge_double(g_last_candle.close, 0.0)) {

		PRINT_TEST_FAILED;
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);
		return false;
	}

	if (!dxf_close_subscription(subscription)) {
		PRINT_TEST_FAILED;
		process_last_error();
		dxf_close_connection(connection);
		return false;
	}

	if (!dxf_close_connection(connection)) {
		PRINT_TEST_FAILED;
		process_last_error();
		return false;
	}

	return true;
}

/*Test*/
bool candle_multiply_subscription_test(void) {
	dxf_connection_t connection = NULL;
	dxf_subscription_t aapl_candle_subscription = NULL;
	dxf_subscription_t ibm_candle_subscription = NULL;
	dxf_subscription_t order_subscription = NULL;

	if (!dxf_create_connection(dxfeed_host, candle_tests_on_thread_terminate, NULL, NULL, NULL, NULL,
		&connection)) {

		PRINT_TEST_FAILED;
		process_last_error();
		return false;
	}

	drop_event_counter(g_aapl_candle_data);
	if (!create_candle_subscription(connection, CANDLE_DEFAULT_SYMBOL, aapl_candle_listener,
		&aapl_candle_subscription)) {

		PRINT_TEST_FAILED;
		dxf_close_connection(connection);
		return false;
	}
	drop_event_counter(g_ibm_candle_data);
	if (!create_candle_subscription(connection, L"IBM", ibm_candle_listener,
		&ibm_candle_subscription)) {

		PRINT_TEST_FAILED;
		dxf_close_subscription(aapl_candle_subscription);
		dxf_close_connection(connection);
		return false;
	}
	drop_event_counter(g_order_data);
	if (!create_event_subscription(connection, DXF_ET_ORDER, CANDLE_DEFAULT_SYMBOL,
		order_event_listener, &order_subscription)) {

		PRINT_TEST_FAILED;
		dxf_close_subscription(ibm_candle_subscription);
		dxf_close_subscription(aapl_candle_subscription);
		dxf_close_connection(connection);
		return false;
	}

	if (!wait_events(get_aapl_candle_counter) ||
		!wait_events(get_ibm_candle_counter) ||
		!wait_events(get_order_event_counter)) {

		PRINT_TEST_FAILED;
		dxf_close_subscription(order_subscription);
		dxf_close_subscription(ibm_candle_subscription);
		dxf_close_subscription(aapl_candle_subscription);
		dxf_close_connection(connection);
		return false;
	}

	//close one candle subscription
	if (!dxf_close_subscription(ibm_candle_subscription)) {
		PRINT_TEST_FAILED;
		dxf_close_subscription(order_subscription);
		dxf_close_subscription(aapl_candle_subscription);
		dxf_close_connection(connection);
		return false;
	}

	//wait events from left subscriptions
	drop_event_counter(g_aapl_candle_data);
	drop_event_counter(g_ibm_candle_data);
	drop_event_counter(g_order_data);
	if (!wait_events(get_aapl_candle_counter) ||
		!wait_events(get_order_event_counter)) {

		PRINT_TEST_FAILED;
		dxf_close_subscription(order_subscription);
		dxf_close_subscription(aapl_candle_subscription);
		dxf_close_connection(connection);
		return false;
	}

	if (!dxf_close_subscription(order_subscription) ||
		!dxf_close_subscription(aapl_candle_subscription)) {

		PRINT_TEST_FAILED;
		process_last_error();
		dxf_close_connection(connection);
		return false;
	}

	if (!dxf_close_connection(connection)) {
		PRINT_TEST_FAILED;
		process_last_error();
		return false;
	}

	return true;
}

/*Test*/
bool candle_symbol_test(void) {
	dxf_connection_t connection = NULL;
	dxf_subscription_t subscription = NULL;
	dxf_const_string_t* symbols = NULL;
	int symbol_count = 0;
	int i;

	if (!dxf_create_connection(dxfeed_host, candle_tests_on_thread_terminate, NULL, NULL, NULL, NULL, &connection)) {
		PRINT_TEST_FAILED;
		process_last_error();
		return false;
	}

	drop_event_counter(g_aapl_candle_data);
	drop_event_counter(g_ibm_candle_data);

	if (!create_candle_subscription(connection, CANDLE_DEFAULT_SYMBOL, common_candle_listener, &subscription)) {
		PRINT_TEST_FAILED;
		dxf_close_connection(connection);
		return false;
	}

	if (!add_symbol_to_existing_candle(subscription, CANDLE_IBM_SYMBOL)) {
		PRINT_TEST_FAILED;
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);
		return false;
	}

	if (!dxf_get_symbols(subscription, &symbols, &symbol_count) ||
		!dx_is_equal_int(2, symbol_count)) {

		PRINT_TEST_FAILED;
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);
		return false;
	}
	for (i = 0; i < symbol_count; i++) {
		if (dx_compare_strings(CANDLE_DEFAULT_FULL_SYMBOL, symbols[i]) == 0 ||
			dx_compare_strings(CANDLE_IBM_FULL_SYMBOL, symbols[i]) == 0)
			continue;
		PRINT_TEST_FAILED;
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);
		return false;
	}

	/* try to add duplicate symbol */
	if (!dx_is_equal_ERRORCODE(DXF_SUCCESS, add_symbol_to_existing_candle(subscription, CANDLE_IBM_SYMBOL))) {
		PRINT_TEST_FAILED;
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);
		return false;
	}
	if (!dxf_get_symbols(subscription, &symbols, &symbol_count) ||
		!dx_is_equal_int(2, symbol_count)) {

		PRINT_TEST_FAILED;
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);
		return false;
	}

	/* wait events */
	if (!wait_events(get_aapl_candle_counter) ||
		!wait_events(get_ibm_candle_counter)) {

		PRINT_TEST_FAILED;
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);
		return false;
	}

	if (!remove_symbol_from_existing_candle(subscription, CANDLE_DEFAULT_SYMBOL)) {
		PRINT_TEST_FAILED;
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);
		return false;
	}

	if (!dxf_get_symbols(subscription, &symbols, &symbol_count) ||
		!dx_is_equal_int(1, symbol_count) ||
		!dx_is_equal_dxf_const_string_t(CANDLE_IBM_FULL_SYMBOL, symbols[0])) {

		PRINT_TEST_FAILED;
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);
		return false;
	}

	drop_event_counter(g_aapl_candle_data);
	drop_event_counter(g_ibm_candle_data);

	if (!wait_events(get_ibm_candle_counter)) {
		PRINT_TEST_FAILED;
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);
		return false;
	}

	if (!dx_is_equal_int(0, get_aapl_candle_counter())) {
		PRINT_TEST_FAILED;
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);
		return false;
	}

	if (!dxf_close_subscription(subscription)) {
		process_last_error();
		PRINT_TEST_FAILED;
		dxf_close_connection(connection);
		return false;
	}

	if (!dxf_close_connection(connection)) {
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}

	return true;
}

bool candle_all_tests(void) {
	bool res = true;

	dxf_initialize_logger("log.log", true, true, true);

	init_listener_thread_data(&g_ct_listener_thread_data);
	init_event_counter(g_aapl_candle_data);
	init_event_counter(g_ibm_candle_data);
	init_event_counter(g_order_data);

	if (!candle_attributes_test() ||
		!candle_subscription_test() ||
		!candle_multiply_subscription_test() ||
		!candle_symbol_test()) {

		res = false;
	}

	free_event_counter(g_order_data);
	free_event_counter(g_ibm_candle_data);
	free_event_counter(g_aapl_candle_data);
	free_listener_thread_data(g_ct_listener_thread_data);

	return res;
}
