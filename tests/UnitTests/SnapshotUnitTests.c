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
#include "DXAlgorithms.h"
#include "DXFeed.h"
#include "DXTypes.h"
#include "EventSubscription.h"
#include "SnapshotTests.h"
#include "Snapshot.h"
#include "SymbolCodec.h"
#include "TestHelper.h"

#define SYMBOL_DEFAULT L"SPY"
#define SOURCE_DEFAULT L"NTV"
#define TIME_DEFAULT 0
#define LEVEL_ORDER 3
#define dxf_osd_buy 0
#define dxf_osd_sell 1
#define dxf_osc_order 3

/* -------------------------------------------------------------------------- */

typedef struct {
	int listener_call_counter;
	bool result;
} dx_snap_test_state_t;

bool play_events(dxf_connection_t connection, dxf_snapshot_t snapshot, dxf_order_t* events, size_t size) {
	size_t i;
	dxf_string_t symbol = dx_get_snapshot_symbol(snapshot);
	dxf_int_t symbol_code = dx_encode_symbol_name(symbol);
	for (i = 0; i < size; i++) {
		dxf_order_t order = events[i];
		dxf_event_data_t event_data = (dxf_event_data_t)&order;
		dxf_ulong_t snapshot_key = dx_new_snapshot_key(dx_rid_order, symbol, order.source);
		// Note: use index as time_int_field for test only
		dxf_event_params_t event_params = { order.event_flags, order.index, snapshot_key };
		if (!dx_process_event_data(connection, dx_eid_order, symbol, symbol_code, event_data, 1, &event_params)) {
			return false;
		}
	}
	return true;
}

bool snapshot_test_runner(dxf_order_t* events, size_t size, dxf_snapshot_listener_t listener, int expected_listener_call_count) {
	dxf_connection_t connection;
	dxf_subscription_t subscription;
	dxf_snapshot_t snapshot;
	dx_event_subscr_flag subscr_flags = dx_esf_time_series | dx_esf_single_record;
	dxf_const_string_t symbol = SYMBOL_DEFAULT;
	dx_snap_test_state_t state = { 0, true };

	connection = dx_init_connection();
	subscription = dx_create_event_subscription(connection, DXF_ET_ORDER, subscr_flags, TIME_DEFAULT);

	if (subscription == dx_invalid_subscription ||
		!dx_init_symbol_codec()) {

		return false;
	}

	dx_clear_order_source(subscription);
	dx_add_order_source(subscription, SOURCE_DEFAULT);

	snapshot = dx_create_snapshot(connection, subscription, dx_eid_order, dx_rid_order, symbol, SOURCE_DEFAULT, TIME_DEFAULT);

	if (snapshot == dx_invalid_snapshot ||
		!dx_add_symbols(subscription, &symbol, 1) ||
		!dx_add_snapshot_listener(snapshot, listener, (void*)&state) ||
		!play_events(connection, snapshot, events, size) ||
		!dx_is_equal_int(expected_listener_call_count, state.listener_call_counter) ||
		!state.result) {

		return false;
	}

	dx_close_snapshot(snapshot);
	dx_close_event_subscription(subscription);
	dx_deinit_connection(connection);

	return true;
}

//Compare snapshots by index and size
bool dx_compare_snapshots(const dxf_snapshot_data_ptr_t snapshot_data, dxf_order_t* test_snap_data, size_t test_snap_size) {
	size_t i;
	bool res = true;
	dxf_order_t* order_records = (dxf_order_t*)snapshot_data->records;
	if (!dx_is_equal_size_t(test_snap_size, snapshot_data->records_count)) {
		return false;
	}
	for (i = 0; i < snapshot_data->records_count; i++) {
		dxf_order_t actual = order_records[i];
		dxf_order_t expected = test_snap_data[i];
		res &= dx_is_equal_dxf_ulong_t(expected.index, actual.index) && dx_is_equal_dxf_long_t(expected.size, actual.size);
	}
	return res;
}

/* -------------------------------------------------------------------------- */

static dxf_order_t simple_test_data[] = {
	{ dxf_ef_snapshot_begin, 0x4e54560000000006, 0,             0, 0, 0,      0,   0, dxf_osc_order, dxf_osd_buy,  0,    L"NTV", 0       },
	{ 0,                     0x4e54560000000005, 1488551035000, 0, 0, 100.50, 100, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0,                     0x4e54560000000004, 1488551035010, 0, 0, 101.00, 101, 0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ 0,                     0x4e54560000000003, 0,             0, 0, 0,      0,   0, dxf_osc_order, dxf_osd_buy,  0,    L"NTV", 0       },
	{ 0,                     0x4e54560000000002, 1488551035020, 0, 0, 100.40, 102, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0,                     0x4e54560000000001, 1488551035030, 0, 0, 100.30, 103, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ dxf_ef_snapshot_end,   0x4e54560000000000, 1488551035040, 0, 0, 100.20, 104, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" }
};

static dxf_order_t simple_test_snap_data[] = {
	{ 0, 0x4e54560000000005, 1488551035000, 0, 0, 100.50, 100, 0, dxf_osc_order, dxf_osd_buy,  L'Q',  L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000004, 1488551035010, 0, 0, 101.00, 101, 0, dxf_osc_order, dxf_osd_sell, L'Q',  L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000002, 1488551035020, 0, 0, 100.40, 102, 0, dxf_osc_order, dxf_osd_buy,  L'Q',  L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000001, 1488551035030, 0, 0, 100.30, 103, 0, dxf_osc_order, dxf_osd_buy,  L'Q',  L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000000, 1488551035040, 0, 0, 100.20, 104, 0, dxf_osc_order, dxf_osd_buy,  L'Q',  L"NTV", L"NSDQ" }
};

static int simple_test_snap_size = SIZE_OF_ARRAY(simple_test_snap_data);

void simple_test_listener(const dxf_snapshot_data_ptr_t snapshot_data, void* user_data) {
	dx_snap_test_state_t* state = (dx_snap_test_state_t*)user_data;
	state->listener_call_counter++;
	state->result &= dx_compare_snapshots(snapshot_data, simple_test_snap_data, simple_test_snap_size);
}

/*
 * Test
 * Simulates a simple receiving of snapshot data with two zero-events.
 */
bool snapshot_simple_test(void) {
	return snapshot_test_runner(simple_test_data, SIZE_OF_ARRAY(simple_test_data), simple_test_listener, 1);
}

/* -------------------------------------------------------------------------- */

static dxf_order_t empty_test_data[] = {
	{ dxf_ef_remove_event | dxf_ef_snapshot_begin | dxf_ef_snapshot_end, 0x4e54560000000000, 1488551035040, 0, 0, 100.20, 104, 0, dxf_osc_order, dxf_osd_buy, L'Q', L"NTV", L"NSDQ" }
};

void empty_test_listener(const dxf_snapshot_data_ptr_t snapshot_data, void* user_data) {
	dx_snap_test_state_t* state = (dx_snap_test_state_t*)user_data;
	state->listener_call_counter++;
	state->result &= dx_is_equal_size_t(0, snapshot_data->records_count);
}

/*
 * Test
 * Simulates receiving of empty snapshot.
 */
bool snapshot_empty_test(void) {
	return snapshot_test_runner(empty_test_data, SIZE_OF_ARRAY(empty_test_data), empty_test_listener, 1);
}

/* -------------------------------------------------------------------------- */

static dxf_order_t update_test_data[] = {
	// initial data
	{ dxf_ef_snapshot_begin,                   0x4e54560000000004, 1488551035000, 0, 0, 100.50, 100,    0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0,                                       0x4e54560000000003, 1488551035010, 0, 0, 101.00, 101,    0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ 0,                                       0x4e54560000000002, 1488551035020, 0, 0, 100.40, 102,    0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0,                                       0x4e54560000000001, 1488551035030, 0, 0, 100.30, 103,    0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ dxf_ef_snapshot_end,                     0x4e54560000000000, 1488551035040, 0, 0, 100.20, 104,    0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	// Update#1: one row update
	{ 0,                                       0x4e54560000000002, 1488551035020, 0, 0, 100.40, 1000,   0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	// Update#2: one row remove via flags
	{ dxf_ef_remove_event,                     0x4e54560000000002, 1488551035020, 0, 0, 100.40, 1001,   0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	// Update#3: one row remove via zero event
	{ 0,                                       0x4e54560000000001, 0,             0, 0, 0.0,    0,      0, dxf_osc_order, dxf_osd_buy,  0,    L"NTV" },
	// Update#4: complex update (inserting x 2, updating, removing, inserting)
	{ dxf_ef_tx_pending,                       0x4e54560000000002, 1488551035020, 0, 0, 100.41, 1001,   0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ dxf_ef_tx_pending,                       0x4e54560000000005, 1488551035020, 0, 0, 100.42, 1002,   0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ dxf_ef_tx_pending,                       0x4e54560000000000, 1488551035040, 0, 0, 100.20, 100500, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ dxf_ef_tx_pending | dxf_ef_remove_event, 0x4e54560000000004, 1488551035000, 0, 0, 100.50, 100,    0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0,                                       0x4e54560000000010, 1488551035000, 0, 0, 100.60, 100,    0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" }
};

static dxf_order_t update_test_snap_1_data[] = {                
	{ 0, 0x4e54560000000004, 1488551035000, 0, 0, 100.50, 100,  0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000003, 1488551035010, 0, 0, 101.00, 101,  0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000002, 1488551035020, 0, 0, 100.40, 1000, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000001, 1488551035030, 0, 0, 100.30, 103,  0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000000, 1488551035040, 0, 0, 100.20, 104,  0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" }
};
static int update_test_snap_1_size = SIZE_OF_ARRAY(update_test_snap_1_data);

static dxf_order_t update_test_snap_2_data[] = {
	{ 0, 0x4e54560000000004, 1488551035000, 0, 0, 100.50, 100, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000003, 1488551035010, 0, 0, 101.00, 101, 0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000001, 1488551035030, 0, 0, 100.30, 103, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000000, 1488551035040, 0, 0, 100.20, 104, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" }
};
static int update_test_snap_2_size = SIZE_OF_ARRAY(update_test_snap_2_data);

static dxf_order_t update_test_snap_3_data[] = {
	{ 0, 0x4e54560000000004, 1488551035000, 0, 0, 100.50, 100, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000003, 1488551035010, 0, 0, 101.00, 101, 0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000000, 1488551035040, 0, 0, 100.20, 104, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" }
};
static int update_test_snap_3_size = SIZE_OF_ARRAY(update_test_snap_3_data);

static dxf_order_t update_test_snap_4_data[] = {
	{ 0, 0x4e54560000000010, 1488551035000, 0, 0, 100.60, 100,    0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000005, 1488551035020, 0, 0, 100.42, 1002,   0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000003, 1488551035010, 0, 0, 101.00, 101,    0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000002, 1488551035020, 0, 0, 100.41, 1001,   0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000000, 1488551035040, 0, 0, 100.20, 100500, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" }
};
static int update_test_snap_4_size = SIZE_OF_ARRAY(update_test_snap_4_data);

void update_test_listener(const dxf_snapshot_data_ptr_t snapshot_data, void* user_data) {
	dx_snap_test_state_t* state = (dx_snap_test_state_t*)user_data;
	state->listener_call_counter++;
	switch (state->listener_call_counter) {
	case 1:
		// initial snapshot not tested in this case
		break;
	case 2:
		state->result &= dx_compare_snapshots(snapshot_data, update_test_snap_1_data, update_test_snap_1_size);
		break;
	case 3:
		state->result &= dx_compare_snapshots(snapshot_data, update_test_snap_2_data, update_test_snap_2_size);
		break;
	case 4:
		state->result &= dx_compare_snapshots(snapshot_data, update_test_snap_3_data, update_test_snap_3_size);
		break;
	case 5:
		state->result &= dx_compare_snapshots(snapshot_data, update_test_snap_4_data, update_test_snap_4_size);
		break;
	default:
		state->result = false;
		break;
	}
}

/*
 * Test
 * Simulates various variants of update.
 */
bool snapshot_update_test(void) {
	return snapshot_test_runner(update_test_data, SIZE_OF_ARRAY(update_test_data), update_test_listener, 5);
}

/* -------------------------------------------------------------------------- */

static dxf_order_t bid_ask_test_data[] = {
	// initial data
	{ dxf_ef_snapshot_begin, 0x4e54560000000005, 1488551035000, 0, 0, 100.50, 100, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0,                     0x4e54560000000004, 1488551035010, 0, 0, 101.00, 101, 0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ 0,                     0x4e54560000000002, 1488551035020, 0, 0, 100.40, 102, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0,                     0x4e54560000000001, 1488551035030, 0, 0, 100.30, 103, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ dxf_ef_snapshot_end,   0x4e54560000000000, 1488551035040, 0, 0, 100.20, 104, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	// Update#1: replace max bid with new min ask
	{ 0,                     0x4e54560000000005, 1488551035000, 0, 0, 100.45, 100, 0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" }
};

static dxf_order_t bid_ask_test_snap_0_data[] = {
	{ 0, 0x4e54560000000005, 1488551035000, 0, 0, 100.50, 100, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000004, 1488551035010, 0, 0, 101.00, 101, 0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000002, 1488551035020, 0, 0, 100.40, 102, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000001, 1488551035030, 0, 0, 100.30, 103, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000000, 1488551035040, 0, 0, 100.20, 104, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" }
};
static int bid_ask_test_snap_0_size = SIZE_OF_ARRAY(bid_ask_test_snap_0_data);

static dxf_order_t bid_ask_test_snap_1_data[] = {
	{ 0, 0x4e54560000000005, 1488551035000, 0, 0, 100.45, 100, 0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000004, 1488551035010, 0, 0, 101.00, 101, 0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000002, 1488551035020, 0, 0, 100.40, 102, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000001, 1488551035030, 0, 0, 100.30, 103, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000000, 1488551035040, 0, 0, 100.20, 104, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" }
};
static int bid_ask_test_snap_1_size = SIZE_OF_ARRAY(bid_ask_test_snap_1_data);

void bid_ask_test_listener(const dxf_snapshot_data_ptr_t snapshot_data, void* user_data) {
	size_t i;
	dxf_order_t* order_records = (dxf_order_t*)snapshot_data->records;
	dx_snap_test_state_t* state = (dx_snap_test_state_t*)user_data;
	double max_bid = -1;
	double min_ask = -1;

	state->listener_call_counter++;

	for (i = 0; i < snapshot_data->records_count; i++) {
		dxf_order_t order = order_records[i];

		if (order.side == dxf_osd_buy) {
			if (order.price > max_bid)
				max_bid = order.price;
		} else if (order.side == dxf_osd_sell) {
			if (order.price < min_ask || min_ask == -1)
				min_ask = order.price;
		}
	}
	state->result &= min_ask > max_bid;

	switch (state->listener_call_counter) {
	case 1:
		state->result &= dx_compare_snapshots(snapshot_data, bid_ask_test_snap_0_data, bid_ask_test_snap_0_size) &&
			dx_is_equal_double(100.50, max_bid) &&
			dx_is_equal_double(101.00, min_ask);
		break;
	case 2:
		state->result &= dx_compare_snapshots(snapshot_data, bid_ask_test_snap_1_data, bid_ask_test_snap_1_size) &&
			dx_is_equal_double(100.40, max_bid) &&
			dx_is_equal_double(100.45, min_ask);
		break;
	default:
		state->result = false;
		break;
	}
}

/*
 * Test
 * Simulates the changing max bid and min ask values via update. This test
 * checks correctness of the bid ask data after update.
 */
bool snapshot_bid_ask_test(void) {
	return snapshot_test_runner(bid_ask_test_data, SIZE_OF_ARRAY(bid_ask_test_data), bid_ask_test_listener, 2);
}

/* -------------------------------------------------------------------------- */

static dxf_order_t duplicate_index_test_data[] = {
	{ dxf_ef_snapshot_begin, 0x4e54560000000005, 1488551035000, 0, 0, 100.50, 100, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0,                     0x4e54560000000004, 1488551035010, 0, 0, 101.00, 101, 0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ 0,                     0x4e54560000000002, 1488551035020, 0, 0, 100.40, 102, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0,                     0x4e54560000000001, 1488551035030, 0, 0, 100.30, 103, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0,                     0x4e54560000000000, 1488551035040, 0, 0, 100.20, 104, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0,                     0x4e54560000000004, 1488551035010, 0, 0, 101.50, 101, 0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ dxf_ef_snapshot_end,   0x4e54560000000002, 1488551035020, 0, 0, 100.60, 102, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" }
};

static dxf_order_t duplicate_index_test_snap_data[] = {
	{ 0, 0x4e54560000000005, 1488551035000, 0, 0, 100.50, 100, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000004, 1488551035010, 0, 0, 101.50, 101, 0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000002, 1488551035020, 0, 0, 100.60, 102, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000001, 1488551035030, 0, 0, 100.30, 103, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000000, 1488551035040, 0, 0, 100.20, 104, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" }
};

static int duplicate_index_test_snap_size = SIZE_OF_ARRAY(duplicate_index_test_snap_data);

void duplicate_index_test_listener(const dxf_snapshot_data_ptr_t snapshot_data, void* user_data) {
	dx_snap_test_state_t* state = (dx_snap_test_state_t*)user_data;
	state->listener_call_counter++;
	state->result &= dx_compare_snapshots(snapshot_data, duplicate_index_test_snap_data, duplicate_index_test_snap_size);
}

/*
 * Test
 * Simulates duplicated indexes in snapshot transmission.
 */
bool snapshot_duplicate_index_test(void) {
	return snapshot_test_runner(duplicate_index_test_data, SIZE_OF_ARRAY(duplicate_index_test_data), duplicate_index_test_listener, 1);
}

/* -------------------------------------------------------------------------- */

static dxf_order_t buildin_update_test_data[] = {
	{ dxf_ef_snapshot_begin,                                         0x4e54560000000005, 1488551035000, 0, 0, 100.50, 100, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0,                                                             0x4e54560000000004, 1488551035010, 0, 0, 101.00, 101, 0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ 0,                                                             0x4e54560000000002, 1488551035020, 0, 0, 100.40, 102, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0,                                                             0x4e54560000000001, 1488551035030, 0, 0, 100.30, 103, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ dxf_ef_snapshot_end | dxf_ef_tx_pending | dxf_ef_remove_event, 0x4e54560000000000, 1488551035040, 0, 0, 100.20, 104, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ dxf_ef_tx_pending,                                             0x4e54560000000004, 1488551035010, 0, 0, 101.50, 101, 0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ 0,                                                             0x4e54560000000002, 1488551035020, 0, 0, 100.60, 102, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" }
};

static dxf_order_t buildin_update_test_snap_data[] = {
	{ 0, 0x4e54560000000005, 1488551035000, 0, 0, 100.50, 100, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000004, 1488551035010, 0, 0, 101.50, 101, 0, dxf_osc_order, dxf_osd_sell, L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000002, 1488551035020, 0, 0, 100.60, 102, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" },
	{ 0, 0x4e54560000000001, 1488551035030, 0, 0, 100.30, 103, 0, dxf_osc_order, dxf_osd_buy,  L'Q', L"NTV", L"NSDQ" }
};

static int buildin_update_test_snap_size = SIZE_OF_ARRAY(buildin_update_test_snap_data);

void buildin_update_test_listener(const dxf_snapshot_data_ptr_t snapshot_data, void* user_data) {
	dx_snap_test_state_t* state = (dx_snap_test_state_t*)user_data;
	state->listener_call_counter++;
	state->result &= dx_compare_snapshots(snapshot_data, buildin_update_test_snap_data, buildin_update_test_snap_size);
}

/*
 * Test
 * Simulates start of update before SNAPSHOT_END flag transmitted.
 */
bool snapshot_buildin_update_test(void) {
	return snapshot_test_runner(buildin_update_test_data, SIZE_OF_ARRAY(buildin_update_test_data), buildin_update_test_listener, 1);
}

/* -------------------------------------------------------------------------- */

static dx_record_info_id_t g_record_info_ids_list[] = {
	dx_rid_order, dx_rid_time_and_sale, dx_rid_candle,
	dx_rid_spread_order, dx_rid_greeks, dx_rid_series
};
static size_t g_record_info_ids_count = SIZE_OF_ARRAY(g_record_info_ids_list);

static dxf_const_string_t g_symbols[] = {
	L"AAPL", L"FB", L"QQQ", L"AMZN", L"MSFT", L"GOOGL", L"BAC", L"GOOG", L"NFLX", L"TSLA", L"PFE",
	L"AGN", L"BABA", L"VRX", L"GE", L"TLT", L"XOM", L"JPM", L"GILD", L"C", L"WFC", L"T", L"JNJ",
	L"DIS", L"PCLN", L"XIV", L"CVX", L"MCD", L"INTC", L"VZ", L"CHTR", L"CSCO", L"HD", L"V", L"BIDU",
	L"NKE", L"BA", L"WMT", L"KO", L"IBM", L"PG", L"SLB", L"CMCSA", L"SHPG", L"IBB", L"YHOO",
	L"ABBV", L"GS", L"FCX", L"ORCL", L"QCOM", L"CVS", L"MRK", L"SBUX", L"BIIB", L"CMG", L"PEP",
	L"BAX", L"DAL", L"TGT", L"CELG", L"MON", L"BRK-B", L"AMGN", L"UNH", L"HAL", L"MDT", L"F",
	L"BMY", L"AVGO", L"PM", L"CAT", L"COST", L"NVDA", L"JD", L"UNP", L"ABT", L"UTX", L"COP",
	L"AIG", L"MO", L"MS", L"LNKD", L"WBA", L"REGN", L"LOW", L"GM", L"TWTR", L"NXPI", L"AAL", L"DVN",
	L"MA", L"TEVA", L"MDLZ", L"ABX", L"VLO", L"DOW", L"MMM", L"UA", L"PXD" };
static size_t g_symbols_count = SIZE_OF_ARRAY(g_symbols);

const dxf_const_string_t g_sources_list[] = {
	L"NTV",
	L"BYX",
	L"BZX",
	L"DEA",
	L"ISE",
	L"DEX",
	L"IST",
	NULL
};
const size_t g_sources_count = SIZE_OF_ARRAY(g_sources_list);

typedef struct {
	dxf_ulong_t* elements;
	size_t size;
	size_t capacity;
} snapshot_key_array_t;

/*
 * Test
 */
bool snapshot_key_test(void) {
	size_t record_index;
	size_t symbol_index;
	size_t source_index;
	dxf_ulong_t key;
	snapshot_key_array_t all_keys = DX_EMPTY_ARRAY;
	bool found = false;
	bool error = false;
	size_t position = 0;
	for (record_index = 0; record_index < g_record_info_ids_count; record_index++) {
		for (symbol_index = 0; symbol_index < g_symbols_count; symbol_index++) {
			for (source_index = 0; source_index < g_sources_count; source_index++) {
				dx_record_info_id_t info_id = g_record_info_ids_list[record_index];
				dxf_const_string_t symbol = g_symbols[symbol_index];
				dxf_const_string_t source = g_sources_list[source_index];
				key = dx_new_snapshot_key(info_id, symbol, source);
				DX_ARRAY_BINARY_SEARCH(all_keys.elements, 0, all_keys.size, key, DX_NUMERIC_COMPARATOR, found, position);
				if (found) {
					wprintf(L"Duplicate snapshot keys detected: %llu! Record id: %d, symbol:'%ls', source:'%ls'.\n", key, info_id, symbol, source);
					PRINT_TEST_FAILED;
					dx_free(all_keys.elements);
					return false;
				} else {
					DX_ARRAY_INSERT(all_keys, dxf_ulong_t, key, position, dx_capacity_manager_halfer, error);
					if (!dx_is_equal_bool(false, error)) {
						PRINT_TEST_FAILED_MESSAGE("Insert array error!");
						dx_free(all_keys.elements);
						return false;
					}
				}
			}
		}
	}
	dx_free(all_keys.elements);
	return true;
}

/* -------------------------------------------------------------------------- */

typedef struct {
	dxf_int_t* elements;
	size_t size;
	size_t capacity;
} hashs_array_t;

/*
 * Test
 */
bool symbol_name_hasher_test(void) {
	size_t symbol_index;
	dxf_int_t hash;
	hashs_array_t all_hashs = DX_EMPTY_ARRAY;
	bool found = false;
	bool error = false;
	size_t position = 0;
	for (symbol_index = 0; symbol_index < g_symbols_count; symbol_index++) {
		dxf_const_string_t symbol = g_symbols[symbol_index];
		hash = dx_symbol_name_hasher(symbol);
		DX_ARRAY_BINARY_SEARCH(all_hashs.elements, 0, all_hashs.size, hash, DX_NUMERIC_COMPARATOR, found, position);
		if (found) {
			wprintf(L"Duplicate hashs detected: %d! symbol:'%ls'.\n", hash, symbol);
			PRINT_TEST_FAILED;
			dx_free(all_hashs.elements);
			return false;
		}
		else {
			DX_ARRAY_INSERT(all_hashs, dxf_int_t, hash, position, dx_capacity_manager_halfer, error);
			if (!dx_is_equal_bool(false, error)) {
				PRINT_TEST_FAILED_MESSAGE("Insert array error!");
				dx_free(all_hashs.elements);
				return false;
			}
		}
	}
	dx_free(all_hashs.elements);
	return true;
}

/* -------------------------------------------------------------------------- */

bool snapshot_all_unit_test(void) {
	bool res = true;

	if (!snapshot_simple_test() ||
		!snapshot_empty_test() ||
		!snapshot_update_test() ||
		!snapshot_bid_ask_test() ||
		!snapshot_duplicate_index_test() ||
		!snapshot_buildin_update_test() ||
		!snapshot_key_test() ||
		!symbol_name_hasher_test()) {

		res = false;
	}
	return res;
}