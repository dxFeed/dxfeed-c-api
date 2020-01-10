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

#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "DXAlgorithms.h"
#include "TestHelper.h"
#include "SnapshotTests.h"

//Timeout in milliseconds for waiting some snapshots is 1 minutes.
#define SNAPSHOT_READ_TIMEOUT 60000
#define SNAPSHOT_LOOP_SLEEP_TIME 100
//Timeout in milliseconds for waiting some events is 2 minutes.
#define EVENTS_READ_TIMEOUT 120000
#define EVENTS_LOOP_SLEEP_TIME 100

#define CANDLE_DEFAULT_SYMBOL L"XBT/USD"
#define CANDLE_USER_EXCHANGE L'A'
#define CANDLE_USER_PERIOD_VALUE 2
#define CANDLE_USER_PRICE_LEVEL_VALUE 0.1

#define CANDLE_FULL_SYMBOL CANDLE_DEFAULT_SYMBOL L"{=d}"

#define SYMBOL_DEFAULT L"AAPL"
#define SYMBOL_IBM L"IBM"

#define ORDER_SOURCE_DEFAULT NULL
#define ORDER_SOURCE_NTV "NTV"
#define ORDER_SOURCE_IST "IST"
#define ORDER_SOURCE_MARKET_MAKER "COMPOSITE_BID"

const char* g_dxfeed_host = "mddqa.in.devexperts.com:7400";
const dxf_string_t g_default_symbol = SYMBOL_DEFAULT;
const dxf_string_t g_invalid_symbol = NULL;
const dxf_string_t g_empty_symbol = L"";
const char* g_ntv_order_source = ORDER_SOURCE_NTV;
const char* g_default_source = ORDER_SOURCE_DEFAULT;
const dx_event_id_t g_order_event_id = dx_eid_order;
const dx_event_id_t g_candle_event_id = dx_eid_candle;
const dxf_long_t g_default_time = 0;
dxf_listener_thread_data_t g_st_listener_thread_data = NULL;
const int g_event_subscription_type = DX_EVENT_BIT_MASK(dx_eid_quote);

void snapshot_tests_on_thread_terminate(dxf_connection_t connection, void* user_data) {
	if (g_st_listener_thread_data == NULL)
		return;
	on_reader_thread_terminate(g_st_listener_thread_data, connection, user_data);
}

CRITICAL_SECTION g_snap_order_guard;
bool g_snap_order_is_received = false;
dxf_snapshot_data_t g_snap_order_data = { 0, NULL, 0, NULL };
dxf_char_t g_snap_order_symbol_buf[100] = { 0 };
dxf_char_t g_snap_order_source[DXF_RECORD_SUFFIX_SIZE] = { 0 };

void snapshot_order_listener(const dxf_snapshot_data_ptr_t snapshot_data, void* user_data) {
	EnterCriticalSection(&g_snap_order_guard);
	wcscpy(g_snap_order_symbol_buf, snapshot_data->symbol);
	g_snap_order_data.event_type = snapshot_data->event_type;
	g_snap_order_data.symbol = g_snap_order_symbol_buf;
	g_snap_order_data.records_count = snapshot_data->records_count;
	g_snap_order_data.records = snapshot_data->records;
	if (snapshot_data->records_count > 0) {
		dxf_order_t* orders = (dxf_order_t*)snapshot_data->records;
		wcscpy(g_snap_order_source, orders[0].source);
	}
	g_snap_order_is_received = true;
	LeaveCriticalSection(&g_snap_order_guard);
}
void snapshot_order_data_init() {
	InitializeCriticalSection(&g_snap_order_guard);
	g_snap_order_is_received = false;
}
void snapshot_order_data_free() {
	DeleteCriticalSection(&g_snap_order_guard);
	g_snap_order_is_received = false;
}
void snapshot_order_data_reset() {
	EnterCriticalSection(&g_snap_order_guard);
	g_snap_order_is_received = false;
	LeaveCriticalSection(&g_snap_order_guard);
}
bool snapshot_order_data_is_received() {
	bool res = false;
	EnterCriticalSection(&g_snap_order_guard);
	res = g_snap_order_is_received;
	LeaveCriticalSection(&g_snap_order_guard);
	return res;
}
const dxf_snapshot_data_ptr_t snapshot_order_data_get_obj() {
	dxf_snapshot_data_ptr_t obj = NULL;
	EnterCriticalSection(&g_snap_order_guard);
	obj = &g_snap_order_data;
	LeaveCriticalSection(&g_snap_order_guard);
	return obj;
}
dxf_const_string_t snapshot_order_data_get_source() {
	dxf_string_t str = NULL;
	EnterCriticalSection(&g_snap_order_guard);
	str = g_snap_order_source;
	LeaveCriticalSection(&g_snap_order_guard);
	return str;
}

CRITICAL_SECTION g_snap_candle_guard;
bool g_snap_candle_is_received = false;
int g_snap_candle_event_type = dx_eid_invalid;
dxf_snapshot_data_t g_snap_candle_data = { 0, NULL, 0, NULL };
dxf_char_t g_snap_candle_symbol_buf[100] = { 0 };
void snapshot_candle_listener(const dxf_snapshot_data_ptr_t snapshot_data, void* user_data) {
	EnterCriticalSection(&g_snap_candle_guard);
	wcscpy(g_snap_candle_symbol_buf, snapshot_data->symbol);
	g_snap_candle_data.event_type = snapshot_data->event_type;
	g_snap_candle_data.symbol = g_snap_candle_symbol_buf;
	g_snap_candle_data.records_count = snapshot_data->records_count;
	g_snap_candle_data.records = snapshot_data->records;
	g_snap_candle_is_received = true;
	LeaveCriticalSection(&g_snap_candle_guard);
}
void snapshot_candle_data_init() {
	InitializeCriticalSection(&g_snap_candle_guard);
	g_snap_candle_is_received = false;
}
void snapshot_candle_data_free() {
	DeleteCriticalSection(&g_snap_candle_guard);
	g_snap_candle_is_received = false;
}
void snapshot_candle_data_reset() {
	EnterCriticalSection(&g_snap_candle_guard);
	g_snap_candle_is_received = false;
	LeaveCriticalSection(&g_snap_candle_guard);
}
bool snapshot_candle_data_is_received() {
	bool res = false;
	EnterCriticalSection(&g_snap_candle_guard);
	res = g_snap_candle_is_received;
	LeaveCriticalSection(&g_snap_candle_guard);
	return res;
}
const dxf_snapshot_data_ptr_t snapshot_candle_data_get_obj() {
	dxf_snapshot_data_ptr_t obj = NULL;
	EnterCriticalSection(&g_snap_candle_guard);
	obj = &g_snap_candle_data;
	LeaveCriticalSection(&g_snap_candle_guard);
	return obj;
}

/*Test*/
bool snapshot_initialization_test(void) {
	dxf_connection_t connection;
	dxf_snapshot_t snapshot;
	dxf_candle_attributes_t candle_attributes = NULL;
	bool res = true;

	if (!dxf_create_connection(g_dxfeed_host, snapshot_tests_on_thread_terminate, NULL, NULL, NULL, NULL, &connection)) {
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}

	//test null connection
	res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_order_snapshot(NULL, g_default_symbol, g_default_source, g_default_time, &snapshot));
	//test invalid symbol
	res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_order_snapshot(connection, g_invalid_symbol, g_default_source, g_default_time, &snapshot));
	//test empty symbol
	res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_order_snapshot(connection, g_empty_symbol, g_default_source, g_default_time, &snapshot));
	//test invalid snapshot
	res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_order_snapshot(connection, g_default_symbol, g_default_source, g_default_time, NULL));
	//test null candle data
	res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_candle_snapshot(connection, NULL, g_default_time, &snapshot));
	if (!res) {
		dxf_close_connection(connection);
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}

	//test Order snapshot
	res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_create_order_snapshot(connection, g_default_symbol, g_ntv_order_source, g_default_time, &snapshot));
	res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_close_snapshot(snapshot));
	if (!res) {
		dxf_close_connection(connection);
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}

	//test Candle snapshot
	if (!dxf_create_candle_symbol_attributes(g_default_symbol,
		DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT,
		DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
		dxf_ctpa_day, dxf_cpa_mark, dxf_csa_default,
		dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, &candle_attributes)) {

		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}
	res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_create_candle_snapshot(connection, candle_attributes, g_default_time, &snapshot));
	res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_close_snapshot(snapshot));
	res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_delete_candle_symbol_attributes(candle_attributes));
	if (!res) {
		dxf_close_connection(connection);
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}

	//test listener subscription on Order snapshot
	res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_create_order_snapshot(connection, g_default_symbol, g_ntv_order_source, g_default_time, &snapshot));
	res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_attach_snapshot_listener(snapshot, NULL, NULL));
	res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_attach_snapshot_listener(snapshot, snapshot_order_listener, NULL));
	res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_detach_snapshot_listener(snapshot, NULL));
	res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_detach_snapshot_listener(snapshot, snapshot_order_listener));
	res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_close_snapshot(snapshot));
	if (!res) {
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
	return true;
}

const candle_attribute_test_case_t g_candle_cases[] = {
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
#define CANDLE_CASES_SIZE sizeof(g_candle_cases) / sizeof(g_candle_cases[0])

const char* g_order_source_cases[] = {
	ORDER_SOURCE_DEFAULT, ORDER_SOURCE_NTV, ORDER_SOURCE_IST, ORDER_SOURCE_MARKET_MAKER,
	ORDER_SOURCE_DEFAULT, ORDER_SOURCE_NTV, ORDER_SOURCE_IST, ORDER_SOURCE_MARKET_MAKER
};
dxf_const_string_t g_order_symbol_cases[] = {
	SYMBOL_DEFAULT, SYMBOL_DEFAULT, SYMBOL_DEFAULT, SYMBOL_DEFAULT,
	SYMBOL_IBM, SYMBOL_IBM, SYMBOL_IBM, SYMBOL_IBM
};
#define ORDER_CASES_SIZE sizeof(g_order_source_cases) / sizeof(g_order_source_cases[0])

/*Test*/
bool snapshot_duplicates_test(void) {
	dxf_connection_t connection;
	dxf_snapshot_t invalid_snapshot = NULL;
	bool res = true;
	int i;
	int num = CANDLE_CASES_SIZE + ORDER_CASES_SIZE;
	dxf_snapshot_t *snapshots = NULL;

	if (!dxf_create_connection(g_dxfeed_host, snapshot_tests_on_thread_terminate, NULL, NULL, NULL, NULL, &connection)) {
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}

	snapshots = dx_calloc(num, sizeof(dxf_snapshot_t));

	for (i = 0; i < ORDER_CASES_SIZE; ++i) {
		//original snapshot
		res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_create_order_snapshot(connection, g_order_symbol_cases[i],
			g_order_source_cases[i], g_default_time, &snapshots[i]));
		res &= dx_is_not_null(snapshots[i]);
		//duplicate snapshot
		res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_order_snapshot(connection, g_order_symbol_cases[i],
			g_order_source_cases[i], g_default_time, &invalid_snapshot));
		res &= dx_is_null(invalid_snapshot);
	}
	for (i = 0; i < CANDLE_CASES_SIZE; ++i) {
		dxf_candle_attributes_t candle_attributes;
		candle_attribute_test_case_t attr = g_candle_cases[i];
		res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_create_candle_symbol_attributes(attr.symbol,
			attr.exchange_code, attr.period_value, attr.period_type, attr.price, attr.session,
			attr.alignment, attr.price_level, &candle_attributes));

		//original snapshot
		res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_create_candle_snapshot(connection, candle_attributes, g_default_time, &snapshots[i]));
		res &= dx_is_not_null(snapshots[i]);
		//duplicate snapshot
		res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_candle_snapshot(connection, candle_attributes, g_default_time, &snapshots[i]));
		res &= dx_is_null(invalid_snapshot);

		res &= dxf_delete_candle_symbol_attributes(candle_attributes);
	}

	for (i = 0; i < num; ++i) {
		if (snapshots[i] != NULL)
			dxf_close_snapshot(snapshots[i]);
	}

	dx_free(snapshots);

	if (!dxf_close_connection(connection)) {
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}
	return res;
}

bool wait_snapshot(bool (*snapshot_data_is_received_func_ptr)(),
				const dxf_snapshot_data_ptr_t (*snapshot_data_get_obj_func_ptr)()) {
	int timestamp = dx_millisecond_timestamp();
	while (!snapshot_data_is_received_func_ptr() || snapshot_data_get_obj_func_ptr()->records_count == 0) {
		if (is_thread_terminate(g_st_listener_thread_data)) {
			printf("Error: Thread was terminated!\n");
			return false;
		}
		if (dx_millisecond_timestamp_diff(dx_millisecond_timestamp(), timestamp) > SNAPSHOT_READ_TIMEOUT) {
			printf("Test failed: timeout is elapsed!\n");
			return false;
		}
		Sleep(SNAPSHOT_LOOP_SLEEP_TIME);
	}
	return true;
}

bool create_order_snapshot_no_errors(dxf_connection_t connection, OUT dxf_snapshot_t *res_snapshot,
									bool is_no_errors) {
	dxf_snapshot_t snapshot;
	dxf_string_t w_order_source = NULL;
	bool res = true;
	dxf_snapshot_data_ptr_t snapshot_order_data = NULL;

	if (!dxf_create_order_snapshot(connection, g_default_symbol, g_ntv_order_source, g_default_time, &snapshot)) {
		if (!is_no_errors) {
			process_last_error();
			PRINT_TEST_FAILED;
		}
		return false;
	}
	if (!dxf_attach_snapshot_listener(snapshot, snapshot_order_listener, NULL)) {
		dxf_close_snapshot(snapshot);
		if (!is_no_errors) {
			process_last_error();
			PRINT_TEST_FAILED;
		}
		return false;
	}

	*res_snapshot = snapshot;
	return true;
}

bool create_order_snapshot(dxf_connection_t connection, OUT dxf_snapshot_t *res_snapshot) {
	return create_order_snapshot_no_errors(connection, res_snapshot, false);
}

bool check_order_snapshot_data_test(dxf_connection_t connection, dxf_snapshot_t snapshot) {
	dxf_string_t w_order_source = NULL;
	bool res = true;
	dxf_snapshot_data_ptr_t snapshot_order_data = NULL;

	if (!wait_snapshot(snapshot_order_data_is_received, snapshot_order_data_get_obj)) {
		PRINT_TEST_FAILED;
		return false;
	}

	snapshot_order_data = snapshot_order_data_get_obj();
	res &= dx_is_equal_int(DX_EVENT_BIT_MASK(g_order_event_id), snapshot_order_data->event_type);
	res &= dx_is_equal_dxf_string_t(g_default_symbol, snapshot_order_data->symbol);

	w_order_source = dx_ansi_to_unicode(g_ntv_order_source);
	res &= dx_is_equal_dxf_const_string_t(w_order_source, snapshot_order_data_get_source());
	free(w_order_source);
	if (!res) {
		PRINT_TEST_FAILED;
		return false;
	}

	return true;
}

bool create_candle_snapshot(dxf_connection_t connection, OUT dxf_snapshot_t *res_snapshot) {
	dxf_snapshot_t snapshot;
	bool res = true;
	dxf_snapshot_data_ptr_t snapshot_candle_data = NULL;
	dxf_candle_attributes_t candle_attributes;

	if (!dxf_create_candle_symbol_attributes(CANDLE_DEFAULT_SYMBOL,
		DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT,
		DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
		dxf_ctpa_day, dxf_cpa_default, dxf_csa_default,
		dxf_caa_default, DXF_CANDLE_PRICE_LEVEL_ATTRIBUTE_DEFAULT, &candle_attributes)) {

		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}

	if (!dxf_create_candle_snapshot(connection, candle_attributes, g_default_time, &snapshot)) {
		process_last_error();
		PRINT_TEST_FAILED;
		dxf_delete_candle_symbol_attributes(candle_attributes);
		return false;
	}

	dxf_delete_candle_symbol_attributes(candle_attributes);

	if (!dxf_attach_snapshot_listener(snapshot, snapshot_candle_listener, NULL)) {
		dxf_close_snapshot(snapshot);
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}

	*res_snapshot = snapshot;
	return true;
}

bool check_candle_snapshot_data_test(dxf_connection_t connection, dxf_snapshot_t snapshot) {
	bool res = true;
	dxf_snapshot_data_ptr_t snapshot_candle_data = NULL;

	if (!wait_snapshot(snapshot_candle_data_is_received, snapshot_candle_data_get_obj)) {
		PRINT_TEST_FAILED;
		return false;
	}

	snapshot_candle_data = snapshot_candle_data_get_obj();
	res &= dx_is_equal_int(DX_EVENT_BIT_MASK(g_candle_event_id), snapshot_candle_data->event_type);
	res &= dx_is_equal_dxf_string_t(CANDLE_FULL_SYMBOL, snapshot_candle_data->symbol);

	if (!res) {
		PRINT_TEST_FAILED;
		return false;
	}

	return true;
}

/*Test*/
bool snapshot_subscription_test(void) {
	dxf_connection_t connection;
	dxf_snapshot_t snapshot;
	bool res = true;

	snapshot_order_data_reset();
	reset_thread_terminate(g_st_listener_thread_data);

	if (!dxf_create_connection(g_dxfeed_host, snapshot_tests_on_thread_terminate, NULL, NULL, NULL, NULL, &connection)) {
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}

	if (!create_order_snapshot(connection, &snapshot)) {
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}
	if (!check_order_snapshot_data_test(connection, snapshot)) {
		dxf_close_snapshot(snapshot);
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}

	//close order subscription
	if (!dxf_close_snapshot(snapshot)) {
		process_last_error();
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}
	snapshot = NULL;

	//open other subscriprion
	snapshot_candle_data_reset();
	reset_thread_terminate(g_st_listener_thread_data);

	if (!create_candle_snapshot(connection, &snapshot)) {
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}
	if (!check_candle_snapshot_data_test(connection, snapshot)) {
		dxf_close_snapshot(snapshot);
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}

	//close candle subscription
	if (!dxf_close_snapshot(snapshot)) {
		process_last_error();
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}
	snapshot = NULL;

	if (!dxf_close_connection(connection)) {
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}
	return true;
}

/*Test*/
bool snapshot_multiply_subscription_test(void) {
	dxf_connection_t connection;
	dxf_snapshot_t order_snapshot;
	dxf_snapshot_t candle_snapshot;
	dxf_snapshot_t invalid_snapshot = NULL;
	bool res = true;

	if (!dxf_create_connection(g_dxfeed_host, snapshot_tests_on_thread_terminate, NULL, NULL, NULL, NULL, &connection)) {
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}

	reset_thread_terminate(g_st_listener_thread_data);

	//create a both snapshot objects
	if (!create_order_snapshot(connection, &order_snapshot)) {
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}

	if (!create_candle_snapshot(connection, &candle_snapshot)) {
		dxf_close_snapshot(order_snapshot);
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}

	//check snapshot data for both snapshots objects
	snapshot_order_data_reset();
	snapshot_candle_data_reset();
	if (!check_order_snapshot_data_test(connection, order_snapshot)) {
		dxf_close_snapshot(order_snapshot);
		dxf_close_snapshot(candle_snapshot);
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}
	if (!check_candle_snapshot_data_test(connection, candle_snapshot)) {
		dxf_close_snapshot(order_snapshot);
		dxf_close_snapshot(candle_snapshot);
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}

	//try to close candle snapshot and wait snapshot data for order
	if (!dxf_close_snapshot(candle_snapshot)) {
		process_last_error();
		dxf_close_snapshot(order_snapshot);
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}
	candle_snapshot = NULL;
	reset_thread_terminate(g_st_listener_thread_data);
	snapshot_order_data_reset();
	if (!check_order_snapshot_data_test(connection, order_snapshot)) {
		dxf_close_snapshot(order_snapshot);
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}

	//test complete, close order snapshot and connection
	if (!dxf_close_snapshot(order_snapshot)) {
		process_last_error();
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}
	order_snapshot = NULL;

	if (!dxf_close_connection(connection)) {
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}
	return true;
}

/* -------------------------------------------------------------------------- */

static dxf_uint_t g_event_subscription_counter = 0;
CRITICAL_SECTION g_event_subscription_counter_guard;

void init_event_subscription_counter() {
	InitializeCriticalSection(&g_event_subscription_counter_guard);
	g_event_subscription_counter = 0;
}

void free_subscription_event_counter() {
	DeleteCriticalSection(&g_event_subscription_counter_guard);
}

void inc_event_subscription_counter() {
	EnterCriticalSection(&g_event_subscription_counter_guard);
	g_event_subscription_counter++;
	LeaveCriticalSection(&g_event_subscription_counter_guard);
}

dxf_uint_t get_event_subscription_counter() {
	dxf_uint_t value = 0;
	EnterCriticalSection(&g_event_subscription_counter_guard);
	value = g_event_subscription_counter;
	LeaveCriticalSection(&g_event_subscription_counter_guard);
	return value;
}

void drop_event_subscription_counter() {
	EnterCriticalSection(&g_event_subscription_counter_guard);
	g_event_subscription_counter = 0;
	LeaveCriticalSection(&g_event_subscription_counter_guard);
}

void events_listener(int event_type, dxf_const_string_t symbol_name,
	const dxf_event_data_t* data, int data_count, void* user_data) {

	if (event_type == g_event_subscription_type) {
		inc_event_subscription_counter();
	}
}

bool check_event_subscription_test() {
	int timestamp = dx_millisecond_timestamp();
	while (get_event_subscription_counter() == 0) {
		if (is_thread_terminate(g_st_listener_thread_data)) {
			printf("Error: Thread was terminated!\n");
			return false;
		}
		if (dx_millisecond_timestamp_diff(dx_millisecond_timestamp(), timestamp) > EVENTS_READ_TIMEOUT) {
			printf("Test failed: timeout is elapsed!\n");
			return false;
		}
		Sleep(EVENTS_LOOP_SLEEP_TIME);
	}

	return dx_ge_dxf_uint_t(get_event_subscription_counter(), 1);
}

/* -------------------------------------------------------------------------- */

/*Test*/
bool snapshot_subscription_and_events_test(void) {
	dxf_connection_t connection;
	dxf_snapshot_t order_snapshot;
	dxf_subscription_t event_subscription;

	if (!dxf_create_connection(g_dxfeed_host, snapshot_tests_on_thread_terminate, NULL, NULL, NULL, NULL, &connection)) {
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}

	reset_thread_terminate(g_st_listener_thread_data);
	snapshot_order_data_reset();
	drop_event_subscription_counter();

	//create snapshot
	if (!create_order_snapshot(connection, &order_snapshot)) {
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}

	//create event
	if (!create_event_subscription(connection, g_event_subscription_type, g_default_symbol,
		events_listener, &event_subscription)) {

		dxf_close_snapshot(order_snapshot);
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}

	//check snapshot data and events
	if (!check_order_snapshot_data_test(connection, order_snapshot)) {
		dxf_close_subscription(event_subscription);
		dxf_close_snapshot(order_snapshot);
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}
	if (!check_event_subscription_test()) {
		dxf_close_subscription(event_subscription);
		dxf_close_snapshot(order_snapshot);
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}

	//close snapshot and check event subscription
	if (!dxf_close_snapshot(order_snapshot)) {
		process_last_error();
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}
	order_snapshot = NULL;
	drop_event_subscription_counter();
	if (!check_event_subscription_test()) {
		dxf_close_subscription(event_subscription);
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}

	//test complete, close subscription
	if (!dxf_close_subscription(event_subscription)) {
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}

	if (!dxf_close_connection(connection)) {
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}
	return true;
}

/*Test*/
bool snapshot_symbols_test(void) {
	dxf_connection_t connection;
	dxf_snapshot_t snapshot;
	bool res = true;
	dxf_string_t returned_symbol = NULL;

	snapshot_order_data_reset();
	reset_thread_terminate(g_st_listener_thread_data);

	if (!dxf_create_connection(g_dxfeed_host, snapshot_tests_on_thread_terminate, NULL, NULL, NULL, NULL, &connection)) {
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}

	if (!create_order_snapshot(connection, &snapshot)) {
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}

	if (!dxf_get_snapshot_symbol(snapshot, &returned_symbol) ||
		!dx_is_equal_dxf_string_t(g_default_symbol, returned_symbol)) {

		PRINT_TEST_FAILED;
		dxf_close_snapshot(snapshot);
		dxf_close_connection(connection);
		return false;
	}

	//close order subscription
	if (!dxf_close_snapshot(snapshot)) {
		process_last_error();
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}
	snapshot = NULL;

	//open other subscriprion
	snapshot_candle_data_reset();
	reset_thread_terminate(g_st_listener_thread_data);

	if (!create_candle_snapshot(connection, &snapshot)) {
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}

	if (!dxf_get_snapshot_symbol(snapshot, &returned_symbol) ||
		!dx_is_equal_dxf_string_t(CANDLE_FULL_SYMBOL, returned_symbol)) {

		PRINT_TEST_FAILED;
		dxf_close_snapshot(snapshot);
		dxf_close_connection(connection);
		return false;
	}

	//close candle subscription
	if (!dxf_close_snapshot(snapshot)) {
		process_last_error();
		dxf_close_connection(connection);
		PRINT_TEST_FAILED;
		return false;
	}
	snapshot = NULL;

	if (!dxf_close_connection(connection)) {
		process_last_error();
		PRINT_TEST_FAILED;
		return false;
	}
	return true;
}

bool snapshot_all_test(void) {
	bool res = true;
	init_listener_thread_data(&g_st_listener_thread_data);
	snapshot_order_data_init();
	snapshot_candle_data_init();
	init_event_subscription_counter();

	if (!snapshot_initialization_test() ||
		!snapshot_duplicates_test() ||
		!snapshot_subscription_test() ||
		!snapshot_multiply_subscription_test() ||
		!snapshot_subscription_and_events_test() ||
		!snapshot_symbols_test()) {

		res = false;
	}

	free_subscription_event_counter();
	snapshot_candle_data_free();
	snapshot_order_data_free();
	free_listener_thread_data(g_st_listener_thread_data);
	g_st_listener_thread_data = NULL;
	return res;
}
