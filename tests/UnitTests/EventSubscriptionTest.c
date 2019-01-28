#include <stdio.h>
#include "DXFeed.h"
#include "DataStructures.h"
#include "EventSubscriptionTest.h"
#include "EventSubscription.h"
#include "SymbolCodec.h"
#include "BufferedIOCommon.h"
#include "DXAlgorithms.h"
#include "ConnectionContextData.h"
#include "TestHelper.h"

static int last_event_type = 0;
static dxf_const_string_t last_symbol = NULL;
static int visit_count = 0;

void dummy_listener(int event_type, dxf_const_string_t symbol_name,
					const dxf_event_data_t* data, int data_count, void* user_data) {
	++visit_count;
	last_event_type = event_type;
	last_symbol = symbol_name;
}

/*
 * Test
 */
bool event_subscription_test (void) {
	dxf_connection_t connection;
	dxf_subscription_t sub1;
	dxf_subscription_t sub2;
	dxf_const_string_t large_symbol_set[] = { L"SYMA", L"SYMB", L"SYMC" };
	dxf_const_string_t middle_symbol_set[] = { L"SYMB", L"SYMD" };
	dxf_const_string_t small_symbol_set[] = { L"SYMB" };
	dxf_int_t symbol_code;
	dxf_event_params_t empty_event_params = { 0, 0, 0 };
	dxf_quote_t quote_data = { 0, 0, 0, 0, 'A', 1.0, 1, 0, 'A', 2.0, 1, dxf_osc_regional };
	const dxf_event_data_t quote_event_data = &quote_data;
	dxf_trade_t trade_data = { 0, 0, 0, 'A', 1.0, 1, 1, 1.0, 0, 1.0, 1.0, dxf_dir_up, false, dxf_osc_regional };
	const dxf_event_data_t trade_event_data = &trade_data;

	if (dx_init_symbol_codec() != true) {
		return false;
	}

	connection = dx_init_connection();

	sub1 = dx_create_event_subscription(connection, DXF_ET_TRADE | DXF_ET_QUOTE, 0, 0);
	sub2 = dx_create_event_subscription(connection, DXF_ET_QUOTE, 0, 0);

	if (sub1 == dx_invalid_subscription || sub2 == dx_invalid_subscription) {
		return false;
	}

	if (!dx_add_symbols(sub1, large_symbol_set, 3)) {
		return false;
	}

	if (!dx_add_symbols(sub2, middle_symbol_set, 2)) {
		return false;
	}

	// sub1 - SYMA, SYMB, SYMC; QUOTE | TRADE
	// sub2 - SYMB, SYMD; QUOTE

	if (!dx_add_listener(sub1, dummy_listener, NULL)) {
		return false;
	}

	if (!dx_add_listener(sub2, dummy_listener, NULL)) {
		return false;
	}

	// testing the symbol retrieval
	{
		dxf_const_string_t* symbols = NULL;
		size_t symbol_count = 0;
		size_t i = 0;

		if (!dx_get_event_subscription_symbols(sub2, &symbols, &symbol_count)) {
			return false;
		}

		if (symbol_count != 2) {
			return false;
		}

		for (; i < symbol_count; ++i) {
			if (dx_compare_strings(symbols[i], middle_symbol_set[i])) {
				return false;
			}
		}
	}

	symbol_code = dx_encode_symbol_name(L"SYMB");

	if (!dx_process_event_data(connection, dx_eid_quote, L"SYMB", symbol_code, quote_event_data, 1, &empty_event_params)) {
		return false;
	}

	// both sub1 and sub2 should receive the data

	if (last_event_type != DXF_ET_QUOTE || dx_compare_strings(last_symbol, L"SYMB") || visit_count != 2) {
		return false;
	}

	symbol_code = dx_encode_symbol_name(L"SYMZ");

	// unknown symbol SYMZ must be rejected

	if (!dx_process_event_data(connection, dx_eid_trade, L"SYMZ", symbol_code, trade_event_data, 1, &empty_event_params) ||
		dx_compare_strings(last_symbol, L"SYMB") ||
		visit_count > 2) {
		return false;
	}

	symbol_code = dx_encode_symbol_name(L"SYMD");

	if (!dx_process_event_data(connection, dx_eid_trade, L"SYMD", symbol_code, trade_event_data, 1, &empty_event_params)) {
		return false;
	}

	// SYMD is a known symbol to sub2, but sub2 doesn't support TRADEs

	if (last_event_type != DXF_ET_QUOTE || dx_compare_strings(last_symbol, L"SYMB") || visit_count != 2) {
		return false;
	}

	if (!dx_remove_symbols(sub1, small_symbol_set, 1)) {
		return false;
	}

	// sub1 is no longer receiving SYMBs

	symbol_code = dx_encode_symbol_name(L"SYMB");

	if (!dx_process_event_data(connection, dx_eid_quote, L"SYMB", symbol_code, quote_event_data, 1, &empty_event_params)) {
		return false;
	}

	// ... but sub2 still does

	if (last_event_type != DXF_ET_QUOTE || dx_compare_strings(last_symbol, L"SYMB") || visit_count != 3) {
		return false;
	}

	symbol_code = dx_encode_symbol_name(L"SYMA");

	if (!dx_process_event_data(connection, dx_eid_trade, L"SYMA", symbol_code, trade_event_data, 1, &empty_event_params)) {
		return false;
	}

	// SYMA must be processed by sub1

	if (last_event_type != DXF_ET_TRADE || dx_compare_strings(last_symbol, L"SYMA") || visit_count != 4) {
		return false;
	}

	if (!dx_remove_listener(sub2, dummy_listener)) {
		return false;
	}

	symbol_code = dx_encode_symbol_name(L"SYMB");

	// SYMB is still supported by sub2, but sub2 no longer has a listener

	if (!dx_process_event_data(connection, dx_eid_quote, L"SYMB", symbol_code, quote_event_data, 1, &empty_event_params)) {
		return false;
	}

	if (last_event_type != DXF_ET_TRADE || dx_compare_strings(last_symbol, L"SYMA") || visit_count != 4) {
		return false;
	}

	if (!dx_close_event_subscription(sub1)) {
		return false;
	}

	if (!dx_close_event_subscription(sub2)) {
		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

#define NON_HISTORY_RECORD_TIME -1
//UTC2017-03-28T07:28:06.123
#define TIME_STAMP 0x15B13D08BEB

typedef struct {
	dxf_const_string_t name;
	dxf_long_t expected_subscr_time;
} subscription_time_struct_t;

static subscription_time_struct_t g_subscr_time_structs_list[] = {
	{ L"Trade", NON_HISTORY_RECORD_TIME },
	{ L"Quote", NON_HISTORY_RECORD_TIME },
	{ L"Summary", NON_HISTORY_RECORD_TIME },
	{ L"Profile", NON_HISTORY_RECORD_TIME },
	{ L"MarketMaker", TIME_STAMP },
	{ L"Order", TIME_STAMP },
	{ L"TimeAndSale", 0x58DA10860000007B },
	{ L"Candle", 0x58DA10860000007B },
	{ L"TradeETH", NON_HISTORY_RECORD_TIME },
	{ L"SpreadOrder", TIME_STAMP },
	{ L"Greeks", 0x58DA10860000007B },
	{ L"TheoPrice", NON_HISTORY_RECORD_TIME },
	{ L"Underlying", NON_HISTORY_RECORD_TIME },
	{ L"Series", TIME_STAMP },
	{ L"Configuration", NON_HISTORY_RECORD_TIME }
};
static size_t g_subscr_time_structs_count = SIZE_OF_ARRAY(g_subscr_time_structs_list);

bool is_history_record(const dx_record_item_t* record_info) {
	int i;
	for (i = 0; i < record_info->field_count; ++i) {
		dx_field_info_t field = record_info->fields[i];
		if (field.time == dx_ft_first_time_int_field || field.time == dx_ft_second_time_int_field) {
			return true;
		}
	}
	return false;
}

/*
 * Test
 *
 * Tests dx_create_subscription_time function that should generate 8-byte time value depending on
 * record fields model. Test uses g_subscr_time_structs_list as subscription list and source of
 * time valid data. It filters history records and performs checking for them only.
 *
 * Expected: all generated subscription times via dx_create_subscription_time should be equals to
 * etalon data from g_subscr_time_structs_list. If checking performs succesfully returns true,
 * otherwise returns false.
 */
bool subscription_time_test(void) {
	size_t i;
	dxf_connection_t connection;
	dx_record_id_t record_id;
	const dx_record_item_t* record_info;
	void* dscc = NULL;
	dxf_long_t subscription_time;

	if (!dx_is_equal_size_t(dx_rid_count, g_subscr_time_structs_count)) {
		PRINT_TEST_FAILED_MESSAGE("Update g_records_names_list in EventSubscriptionTest.");
		return false;
	}

	if (dx_init_symbol_codec() != true) {
		return false;
	}

	connection = dx_init_connection();
	dscc = dx_get_data_structures_connection_context(connection);
	if (!dx_is_not_null(dscc)) {
		PRINT_TEST_FAILED_MESSAGE("Unable to receive data structures connection context.");
		return false;
	}

	for (i = 0; i < g_subscr_time_structs_count; i++) {
		subscription_time = LLONG_MAX;
		record_id = dx_add_or_get_record_id(connection, g_subscr_time_structs_list[i].name);
		record_info = dx_get_record_by_id(dscc, record_id);

		if (!is_history_record(record_info))
			continue;

		if (!dx_create_subscription_time(dscc, record_id, TIME_STAMP, OUT &subscription_time)) {
			PRINT_TEST_FAILED_MESSAGE("Create subscription time error!");
			return false;
		}
		if (!dx_is_equal_dxf_long_t(g_subscr_time_structs_list[i].expected_subscr_time, subscription_time)) {
			wprintf(L"Invalid subscription time for %ls record!\n", g_subscr_time_structs_list[i].name);
			PRINT_TEST_FAILED;
			return false;
		}
	}

	if (!dx_is_equal_bool(false, dx_create_subscription_time(NULL, record_id, TIME_STAMP, OUT &subscription_time)) ||
		!dx_is_equal_bool(false, dx_create_subscription_time(dscc, -1, TIME_STAMP, OUT &subscription_time)) ||
		!dx_is_equal_bool(false, dx_create_subscription_time(dscc, record_id, TIME_STAMP, NULL))) {
		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool event_subscription_all_test(void) {
	bool res = true;

	if (!event_subscription_test() ||
		!subscription_time_test()) {

		res = false;
	}
	return res;
}