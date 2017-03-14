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
#define SIDE_BUY 0
#define SIDE_SELL 1
#define SCOPE_ORDER 3

/* -------------------------------------------------------------------------- */

typedef struct {
    dxf_event_flags_t flags;
    dxf_order_t data;
} dx_event_data_t;

typedef struct {
    int listener_call_counter;
    bool result;
} dx_snap_test_state_t;

bool play_events(dxf_connection_t connection, dxf_snapshot_t snapshot, dx_event_data_t* events, size_t size) {
    size_t i;
    dxf_string_t symbol = dx_get_snapshot_symbol(snapshot);
    dxf_int_t symbol_code = dx_encode_symbol_name(symbol);
    for (i = 0; i < size; i++) {
        dxf_order_t order = events[i].data;
        dxf_event_data_t event_data = (dxf_event_data_t)&order;
        dxf_ulong_t snapshot_key = dx_new_snapshot_key(dx_rid_order, symbol, order.source);
        // Note: use index as time_int_field for test only
        dxf_event_params_t event_params = { events[i].flags, order.index, snapshot_key };
        if (!dx_process_event_data(connection, dx_eid_order, symbol, symbol_code, event_data, 1, &event_params)) {
            return false;
        }
    }
    return true;
}

bool snapshot_test_runner(dx_event_data_t* events, size_t size, dxf_snapshot_listener_t listener, int expected_listener_call_count) {
    dxf_connection_t connection;
    dxf_subscription_t subscription;
    dxf_snapshot_t snapshot;
    dxf_uint_t subscr_flags = DX_SUBSCR_FLAG_TIME_SERIES | DX_SUBSCR_FLAG_SINGLE_RECORD;
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

static dx_event_data_t simple_test_data[] = {
    { dxf_ef_snapshot_begin,    { 0, 0, 0, 0x4e54560000000006, LEVEL_ORDER, SIDE_BUY, 0, SCOPE_ORDER, 0, 0, L"NTV", 0, 0, 0 } }, 
    { 0,                        { 0, 0, L'Q', 0x4e54560000000005, LEVEL_ORDER, SIDE_BUY, 100.50, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" } }, 
    { 0,                        { 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_SELL, 101.00, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" } },
    { 0,                        { 0, 0, 0, 0x4e54560000000003, LEVEL_ORDER, SIDE_BUY, 0, SCOPE_ORDER, 0, 0, L"NTV", 0, 0, 0 } }, 
    { 0,                        { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.40, SCOPE_ORDER, 0, 102, L"NTV", 1488551035020, 0, L"NSDQ" } },
    { 0,                        { 0, 0, L'Q', 0x4e54560000000001, LEVEL_ORDER, SIDE_BUY, 100.30, SCOPE_ORDER, 0, 103, L"NTV", 1488551035030, 0, L"NSDQ" } },
    { dxf_ef_snapshot_end,      { 0, 0, L'Q', 0x4e54560000000000, LEVEL_ORDER, SIDE_BUY, 100.20, SCOPE_ORDER, 0, 104, L"NTV", 1488551035040, 0, L"NSDQ" } }
};

static dxf_order_t simple_test_snap_data[] = {
    { 0, 0, L'Q', 0x4e54560000000005, LEVEL_ORDER, SIDE_BUY, 100.50, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_SELL, 101.00, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.40, SCOPE_ORDER, 0, 102, L"NTV", 1488551035020, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000001, LEVEL_ORDER, SIDE_BUY, 100.30, SCOPE_ORDER, 0, 103, L"NTV", 1488551035030, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000000, LEVEL_ORDER, SIDE_BUY, 100.20, SCOPE_ORDER, 0, 104, L"NTV", 1488551035040, 0, L"NSDQ" }
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

static dx_event_data_t empty_test_data[] = {
    { 
        dxf_ef_remove_event | dxf_ef_snapshot_begin | dxf_ef_snapshot_end,
        { 0, 0, L'Q', 0x4e54560000000000, LEVEL_ORDER, SIDE_BUY, 100.20, SCOPE_ORDER, 0, 104, L"NTV", 1488551035040, 0, L"NSDQ" } 
    }
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

static dx_event_data_t update_test_data[] = {
    // initial data
    { dxf_ef_snapshot_begin,    { 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_BUY, 100.50, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" } },
    { 0,                        { 0, 0, L'Q', 0x4e54560000000003, LEVEL_ORDER, SIDE_SELL, 101.00, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" } },
    { 0,                        { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.40, SCOPE_ORDER, 0, 102, L"NTV", 1488551035020, 0, L"NSDQ" } },
    { 0,                        { 0, 0, L'Q', 0x4e54560000000001, LEVEL_ORDER, SIDE_BUY, 100.30, SCOPE_ORDER, 0, 103, L"NTV", 1488551035030, 0, L"NSDQ" } },
    { dxf_ef_snapshot_end,      { 0, 0, L'Q', 0x4e54560000000000, LEVEL_ORDER, SIDE_BUY, 100.20, SCOPE_ORDER, 0, 104, L"NTV", 1488551035040, 0, L"NSDQ" } },
    // Update#1: one row update
    { 0,                        { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.40, SCOPE_ORDER, 0, 1000, L"NTV", 1488551035020, 0, L"NSDQ" } },
    // Update#2: one row remove via flags
    { dxf_ef_remove_event,      { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.40, SCOPE_ORDER, 0, 1001, L"NTV", 1488551035020, 0, L"NSDQ" } },
    // Update#3: one row remove via zero event
    { 0,                        { 0, 0, 0, 0x4e54560000000001, LEVEL_ORDER, SIDE_BUY, 0, SCOPE_ORDER, 0, 0, L"NTV", 0, 0, 0 } },
    // Update#4: complex update (inserting x 2, updating, removing, inserting)
    { dxf_ef_tx_pending,        { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.41, SCOPE_ORDER, 0, 1001, L"NTV", 1488551035020, 0, L"NSDQ" } },
    { dxf_ef_tx_pending,        { 0, 0, L'Q', 0x4e54560000000005, LEVEL_ORDER, SIDE_BUY, 100.42, SCOPE_ORDER, 0, 1002, L"NTV", 1488551035020, 0, L"NSDQ" } },
    { dxf_ef_tx_pending,        { 0, 0, L'Q', 0x4e54560000000000, LEVEL_ORDER, SIDE_BUY, 100.20, SCOPE_ORDER, 0, 100500, L"NTV", 1488551035040, 0, L"NSDQ" } },
    { dxf_ef_tx_pending | dxf_ef_remove_event,{ 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_BUY, 100.50, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" } },
    { 0,                        { 0, 0, L'Q', 0x4e54560000000010, LEVEL_ORDER, SIDE_BUY, 100.60, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" } },
};

static dxf_order_t update_test_snap_1_data[] = {
    { 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_BUY, 100.50, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000003, LEVEL_ORDER, SIDE_SELL, 101.00, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.40, SCOPE_ORDER, 0, 1000, L"NTV", 1488551035020, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000001, LEVEL_ORDER, SIDE_BUY, 100.30, SCOPE_ORDER, 0, 103, L"NTV", 1488551035030, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000000, LEVEL_ORDER, SIDE_BUY, 100.20, SCOPE_ORDER, 0, 104, L"NTV", 1488551035040, 0, L"NSDQ" }
};
static int update_test_snap_1_size = SIZE_OF_ARRAY(update_test_snap_1_data);

static dxf_order_t update_test_snap_2_data[] = {
    { 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_BUY, 100.50, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000003, LEVEL_ORDER, SIDE_SELL, 101.00, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000001, LEVEL_ORDER, SIDE_BUY, 100.30, SCOPE_ORDER, 0, 103, L"NTV", 1488551035030, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000000, LEVEL_ORDER, SIDE_BUY, 100.20, SCOPE_ORDER, 0, 104, L"NTV", 1488551035040, 0, L"NSDQ" }
};
static int update_test_snap_2_size = SIZE_OF_ARRAY(update_test_snap_2_data);

static dxf_order_t update_test_snap_3_data[] = {
    { 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_BUY, 100.50, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000003, LEVEL_ORDER, SIDE_SELL, 101.00, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000000, LEVEL_ORDER, SIDE_BUY, 100.20, SCOPE_ORDER, 0, 104, L"NTV", 1488551035040, 0, L"NSDQ" }
};
static int update_test_snap_3_size = SIZE_OF_ARRAY(update_test_snap_3_data);

static dxf_order_t update_test_snap_4_data[] = {
    { 0, 0, L'Q', 0x4e54560000000010, LEVEL_ORDER, SIDE_BUY, 100.60, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000005, LEVEL_ORDER, SIDE_BUY, 100.42, SCOPE_ORDER, 0, 1002, L"NTV", 1488551035020, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000003, LEVEL_ORDER, SIDE_SELL, 101.00, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.41, SCOPE_ORDER, 0, 1001, L"NTV", 1488551035020, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000000, LEVEL_ORDER, SIDE_BUY, 100.20, SCOPE_ORDER, 0, 100500, L"NTV", 1488551035040, 0, L"NSDQ" }
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

static dx_event_data_t bid_ask_test_data[] = {
    // initial data
    { dxf_ef_snapshot_begin,    { 0, 0, L'Q', 0x4e54560000000005, LEVEL_ORDER, SIDE_BUY, 100.50, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" } },
    { 0,                        { 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_SELL, 101.00, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" } },
    { 0,                        { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.40, SCOPE_ORDER, 0, 102, L"NTV", 1488551035020, 0, L"NSDQ" } },
    { 0,                        { 0, 0, L'Q', 0x4e54560000000001, LEVEL_ORDER, SIDE_BUY, 100.30, SCOPE_ORDER, 0, 103, L"NTV", 1488551035030, 0, L"NSDQ" } },
    { dxf_ef_snapshot_end,      { 0, 0, L'Q', 0x4e54560000000000, LEVEL_ORDER, SIDE_BUY, 100.20, SCOPE_ORDER, 0, 104, L"NTV", 1488551035040, 0, L"NSDQ" } },
    // Update#1: replace max bid with new min ask
    { 0,                        { 0, 0, L'Q', 0x4e54560000000005, LEVEL_ORDER, SIDE_SELL, 100.45, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" } }
};

static dxf_order_t bid_ask_test_snap_0_data[] = {
    { 0, 0, L'Q', 0x4e54560000000005, LEVEL_ORDER, SIDE_BUY, 100.50, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_SELL, 101.00, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.40, SCOPE_ORDER, 0, 102, L"NTV", 1488551035020, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000001, LEVEL_ORDER, SIDE_BUY, 100.30, SCOPE_ORDER, 0, 103, L"NTV", 1488551035030, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000000, LEVEL_ORDER, SIDE_BUY, 100.20, SCOPE_ORDER, 0, 104, L"NTV", 1488551035040, 0, L"NSDQ" }
};
static int bid_ask_test_snap_0_size = SIZE_OF_ARRAY(bid_ask_test_snap_0_data);

static dxf_order_t bid_ask_test_snap_1_data[] = {
    { 0, 0, L'Q', 0x4e54560000000005, LEVEL_ORDER, SIDE_SELL, 100.45, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_SELL, 101.00, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.40, SCOPE_ORDER, 0, 102, L"NTV", 1488551035020, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000001, LEVEL_ORDER, SIDE_BUY, 100.30, SCOPE_ORDER, 0, 103, L"NTV", 1488551035030, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000000, LEVEL_ORDER, SIDE_BUY, 100.20, SCOPE_ORDER, 0, 104, L"NTV", 1488551035040, 0, L"NSDQ" }
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

        if (order.side == DXF_ORDER_SIDE_BUY) {
            if (order.price > max_bid)
                max_bid = order.price;
        } else if (order.side == DXF_ORDER_SIDE_SELL) {
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

static dx_event_data_t duplicate_index_test_data[] = {
    { dxf_ef_snapshot_begin,    { 0, 0, L'Q', 0x4e54560000000005, LEVEL_ORDER, SIDE_BUY, 100.50, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" } }, 
    { 0,                        { 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_SELL, 101.00, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" } },
    { 0,                        { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.40, SCOPE_ORDER, 0, 102, L"NTV", 1488551035020, 0, L"NSDQ" } },
    { 0,                        { 0, 0, L'Q', 0x4e54560000000001, LEVEL_ORDER, SIDE_BUY, 100.30, SCOPE_ORDER, 0, 103, L"NTV", 1488551035030, 0, L"NSDQ" } },
    { 0,                        { 0, 0, L'Q', 0x4e54560000000000, LEVEL_ORDER, SIDE_BUY, 100.20, SCOPE_ORDER, 0, 104, L"NTV", 1488551035040, 0, L"NSDQ" } },
    { 0,                        { 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_SELL, 101.50, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" } },
    { dxf_ef_snapshot_end,      { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.60, SCOPE_ORDER, 0, 102, L"NTV", 1488551035020, 0, L"NSDQ" } },
};

static dxf_order_t duplicate_index_test_snap_data[] = {
    { 0, 0, L'Q', 0x4e54560000000005, LEVEL_ORDER, SIDE_BUY, 100.50, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_SELL, 101.50, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.60, SCOPE_ORDER, 0, 102, L"NTV", 1488551035020, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000001, LEVEL_ORDER, SIDE_BUY, 100.30, SCOPE_ORDER, 0, 103, L"NTV", 1488551035030, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000000, LEVEL_ORDER, SIDE_BUY, 100.20, SCOPE_ORDER, 0, 104, L"NTV", 1488551035040, 0, L"NSDQ" }
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

static dx_event_data_t buildin_update_test_data[] = {
    { dxf_ef_snapshot_begin,    { 0, 0, L'Q', 0x4e54560000000005, LEVEL_ORDER, SIDE_BUY, 100.50, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" } }, 
    { 0,                        { 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_SELL, 101.00, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" } },
    { 0,                        { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.40, SCOPE_ORDER, 0, 102, L"NTV", 1488551035020, 0, L"NSDQ" } },
    { 0,                        { 0, 0, L'Q', 0x4e54560000000001, LEVEL_ORDER, SIDE_BUY, 100.30, SCOPE_ORDER, 0, 103, L"NTV", 1488551035030, 0, L"NSDQ" } },
    { 
        dxf_ef_snapshot_end | dxf_ef_tx_pending | dxf_ef_remove_event,
        { 0, 0, L'Q', 0x4e54560000000000, LEVEL_ORDER, SIDE_BUY, 100.20, SCOPE_ORDER, 0, 104, L"NTV", 1488551035040, 0, L"NSDQ" } 
    },
    { dxf_ef_tx_pending,        { 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_SELL, 101.50, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" } },
    { 0,                        { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.60, SCOPE_ORDER, 0, 102, L"NTV", 1488551035020, 0, L"NSDQ" } },
};

static dxf_order_t buildin_update_test_snap_data[] = {
    { 0, 0, L'Q', 0x4e54560000000005, LEVEL_ORDER, SIDE_BUY, 100.50, SCOPE_ORDER, 0, 100, L"NTV", 1488551035000, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000004, LEVEL_ORDER, SIDE_SELL, 101.50, SCOPE_ORDER, 0, 101, L"NTV", 1488551035010, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000002, LEVEL_ORDER, SIDE_BUY, 100.60, SCOPE_ORDER, 0, 102, L"NTV", 1488551035020, 0, L"NSDQ" },
    { 0, 0, L'Q', 0x4e54560000000001, LEVEL_ORDER, SIDE_BUY, 100.30, SCOPE_ORDER, 0, 103, L"NTV", 1488551035030, 0, L"NSDQ" }
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

bool snapshot_all_unit_test(void) {
    bool res = true;

    if (!snapshot_simple_test() ||
        !snapshot_empty_test() ||
        !snapshot_update_test() ||
        !snapshot_bid_ask_test() ||
        !snapshot_duplicate_index_test() ||
        !snapshot_buildin_update_test()) {

        res = false;
    }
    return res;
}