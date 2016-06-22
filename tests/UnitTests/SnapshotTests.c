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

const char* g_dxfeed_host = "mddqa.in.devexperts.com:7400";
const dxf_string_t g_default_symbol = L"AAPL";
const dxf_string_t g_other_symbol = L"IBM";
const dxf_string_t g_invalid_symbol = NULL;
const dxf_string_t g_empty_symbol = L"";
const char* g_ntv_order_source = "NTV";
const char* g_ist_order_source = "IST";
const char* g_market_maker_source = "COMPOSITE_BID";
const char* g_default_source = NULL;
const dx_event_id_t g_supported_event_ids[] = { dx_eid_order };
const dx_event_id_t g_invalid_event_id = dx_eid_invalid;
const dx_event_id_t g_order_event_id = dx_eid_order;
//TODO: candle
const dx_event_id_t g_candle_event_id = dx_eid_order;
const dxf_int_t g_default_time = 0;
dxf_listener_thread_data_t g_listener_thread_data = NULL;
const int g_event_subscription_type = DX_EVENT_BIT_MASK(dx_eid_quote);

bool dx_is_supported_event(dx_event_id_t event_id) {
    int i;
    for (i = 0; i < sizeof(g_supported_event_ids); i++) {
        if (event_id == g_supported_event_ids[i])
            return true;
    }
    return false;
}

void snapshot_tests_on_thread_terminate(dxf_connection_t connection, void* user_data) {
    if (g_listener_thread_data == NULL)
        return;
    on_reader_thread_terminate(g_listener_thread_data, connection, user_data);
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

bool snapshot_initialization_test(void) {
    dxf_connection_t connection;
    dxf_snapshot_t snapshot;
    dx_event_id_t event_id;
    bool res = true;

    if (!dxf_create_connection(g_dxfeed_host, snapshot_tests_on_thread_terminate, NULL, NULL, NULL, &connection)) {
        process_last_error();
        PRINT_TEST_FAILED;
        return false;
    }

    //test null connection
    res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_snapshot(NULL, g_order_event_id, g_default_symbol, g_default_source, g_default_time, &snapshot));
    //test invalid event_id
    res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_snapshot(connection, g_invalid_event_id, g_default_symbol, g_default_source, g_default_time, &snapshot));
    //test invalid symbol
    res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_snapshot(connection, g_order_event_id, g_invalid_symbol, g_default_source, g_default_time, &snapshot));
    //test empty symbol
    res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_snapshot(connection, g_order_event_id, g_empty_symbol, g_default_source, g_default_time, &snapshot));
    //test invalid snapshot
    res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_snapshot(connection, g_order_event_id, g_default_symbol, g_default_source, g_default_time, NULL));
    //test unsupported events
    for (event_id = dx_eid_begin; event_id < dx_eid_count; ++event_id) {
        if (dx_is_supported_event(event_id))
            continue;
        res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_snapshot(connection, event_id, g_default_symbol, g_default_source, g_default_time, &snapshot));
    }
    if (!res) {
        dxf_close_connection(connection);
        process_last_error();
        PRINT_TEST_FAILED;
        return false;
    }

    //test Order snapshot
    res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_create_snapshot(connection, dx_eid_order, g_default_symbol, g_ntv_order_source, g_default_time, &snapshot));
    res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_close_snapshot(snapshot));
    if (!res) {
        dxf_close_connection(connection);
        process_last_error();
        PRINT_TEST_FAILED;
        return false;
    }

    //TODO candle
    //test Candle snapshot
    /*res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_create_snapshot(connection, dx_eid_candle, g_default_symbol, g_order_source, time, &snapshot));
    res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_close_snapshot(snapshot));
    if (!res) {
        dxf_close_connection(connection);
        process_last_error();
        PRINT_TEST_FAILED;
        return false;
    }*/
    

    //test listener subscription on Order snapshot
    res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_create_snapshot(connection, dx_eid_order, g_default_symbol, g_ntv_order_source, g_default_time, &snapshot));
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

bool snapshot_duplicates_test(void) {
    dxf_connection_t connection;
    dxf_snapshot_t invalid_snapshot = NULL;
    bool res = true;
    int i;
    const char* source_cases[] = { g_default_source, g_ntv_order_source, g_ist_order_source, g_market_maker_source, g_default_source };
    dx_event_id_t event_cases[] = { g_order_event_id, g_order_event_id, g_order_event_id, g_order_event_id, g_candle_event_id };
    dxf_snapshot_t snapshots[sizeof(event_cases) / sizeof(event_cases[0])][2] = { 0 };
    const int DEF_SNAP_COL = 0;
    const int OTHER_SNAP_COL = 1;

    if (!dxf_create_connection(g_dxfeed_host, snapshot_tests_on_thread_terminate, NULL, NULL, NULL, &connection)) {
        process_last_error();
        PRINT_TEST_FAILED;
        return false;
    }

    for (i = 0; i < sizeof(event_cases) / sizeof(event_cases[0]); ++i) {
        //original snapshot
        res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_create_snapshot(connection, event_cases[i],
            g_default_symbol, source_cases[i], g_default_time, &snapshots[i][DEF_SNAP_COL]));
        res &= dx_is_not_null(snapshots[i][DEF_SNAP_COL]);
        //duplicate snapshot
        res &= dx_is_equal_ERRORCODE(DXF_FAILURE, dxf_create_snapshot(connection, event_cases[i],
            g_default_symbol, source_cases[i], g_default_time, &invalid_snapshot));
        res &= dx_is_null(invalid_snapshot);
        //other symbol snapshot
        res &= dx_is_equal_ERRORCODE(DXF_SUCCESS, dxf_create_snapshot(connection, event_cases[i],
            g_other_symbol, source_cases[i], g_default_time, &snapshots[i][OTHER_SNAP_COL]));
        res &= dx_is_not_null(snapshots[i][OTHER_SNAP_COL]);
    }

    for (i = 0; i < sizeof(event_cases) / sizeof(event_cases[0]); ++i) {
        if (snapshots[i][DEF_SNAP_COL] != NULL)
            dxf_close_snapshot(snapshots[i][DEF_SNAP_COL]);
        if (snapshots[i][OTHER_SNAP_COL] != NULL)
            dxf_close_snapshot(snapshots[i][OTHER_SNAP_COL]);
    }

    if (!dxf_close_connection(connection)) {
        process_last_error();
        PRINT_TEST_FAILED;
        return false;
    }
    return res;
}

bool wait_snapshot(bool (*snapshot_data_is_received_func_ptr)(), const dxf_snapshot_data_ptr_t (*snapshot_data_get_obj_func_ptr)()) {
    int timestamp = dx_millisecond_timestamp();
    while (!snapshot_data_is_received_func_ptr() || snapshot_data_get_obj_func_ptr()->records_count == 0) {
        if (is_thread_terminate(g_listener_thread_data)) {
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

bool create_order_snapshot_no_errors(dxf_connection_t connection, OUT dxf_snapshot_t *res_snapshot, bool is_no_errors) {
    dxf_snapshot_t snapshot;
    dxf_string_t w_order_source = NULL;
    bool res = true;
    dxf_snapshot_data_ptr_t snapshot_order_data = NULL;

    if (!dxf_create_snapshot(connection, g_order_event_id, g_default_symbol, g_ntv_order_source, g_default_time, &snapshot)) {
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

    //TODO: time
    if (!dxf_create_snapshot(connection, g_candle_event_id, g_default_symbol, /*TODO: g_default_source*/"DEX", g_default_time, &snapshot)) {
        process_last_error();
        PRINT_TEST_FAILED;
        return false;
    }
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
    res &= dx_is_equal_dxf_string_t(g_default_symbol, snapshot_candle_data->symbol);

    if (!res) {
        PRINT_TEST_FAILED;
        return false;
    }

    return true;
}

bool snapshot_subscription_test(void) {
    dxf_connection_t connection;
    dxf_snapshot_t snapshot;
    bool res = true;

    snapshot_order_data_reset();
    reset_thread_terminate(g_listener_thread_data);

    if (!dxf_create_connection(g_dxfeed_host, snapshot_tests_on_thread_terminate, NULL, NULL, NULL, &connection)) {
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
    reset_thread_terminate(g_listener_thread_data);

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

bool snapshot_multiply_subscription_test(void) {
    dxf_connection_t connection;
    dxf_snapshot_t order_snapshot;
    dxf_snapshot_t candle_snapshot;
    dxf_snapshot_t invalid_snapshot = NULL;
    bool res = true;

    if (!dxf_create_connection(g_dxfeed_host, snapshot_tests_on_thread_terminate, NULL, NULL, NULL, &connection)) {
        process_last_error();
        PRINT_TEST_FAILED;
        return false;
    }

    reset_thread_terminate(g_listener_thread_data);

    //create a both snapshot objects
    if (!create_order_snapshot(connection, &order_snapshot)) {
        dxf_close_connection(connection);
        PRINT_TEST_FAILED;
        return false;
    }

    if (!create_candle_snapshot(connection, &candle_snapshot)) {
        dxf_close_connection(connection);
        PRINT_TEST_FAILED;
        return false;
    }

    //check snapshot data for both snapshots objects
    snapshot_order_data_reset();
    snapshot_candle_data_reset();
    if (!check_order_snapshot_data_test(connection, order_snapshot)) {
        dxf_close_snapshot(order_snapshot);
        dxf_close_connection(connection);
        PRINT_TEST_FAILED;
        return false;
    }
    if (!check_candle_snapshot_data_test(connection, candle_snapshot)) {
        dxf_close_snapshot(candle_snapshot);
        dxf_close_connection(connection);
        PRINT_TEST_FAILED;
        return false;
    }

    //try to close candle snapshot and wait snapshot data for order 
    if (!dxf_close_snapshot(candle_snapshot)) {
        process_last_error();
        dxf_close_connection(connection);
        PRINT_TEST_FAILED;
        return false;
    }
    candle_snapshot = NULL;
    reset_thread_terminate(g_listener_thread_data);
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
    const dxf_event_data_t* data, int data_count,
    const dxf_event_params_t* event_params, void* user_data) {

    if (event_type == g_event_subscription_type) {
        inc_event_subscription_counter();
    }
}

bool create_event_subscription(dxf_connection_t connection, OUT dxf_subscription_t* res_subscription) {
    dxf_subscription_t subscription = NULL;

    if (!dxf_create_subscription(connection, g_event_subscription_type, &subscription)) {
        process_last_error();

        return false;
    };

    if (!dxf_add_symbol(subscription, g_default_symbol)) {
        process_last_error();

        return false;
    };

    if (!dxf_attach_event_listener(subscription, events_listener, NULL)) {
        process_last_error();

        return false;
    };

    *res_subscription = subscription;
    return true;
}

bool check_event_subscription_test() {
    int timestamp = dx_millisecond_timestamp();
    while (get_event_subscription_counter() == 0) {
        if (is_thread_terminate(g_listener_thread_data)) {
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

bool snapshot_subscription_and_events_test(void) {
    dxf_connection_t connection;
    dxf_snapshot_t order_snapshot;
    dxf_subscription_t event_subscription;

    if (!dxf_create_connection(g_dxfeed_host, snapshot_tests_on_thread_terminate, NULL, NULL, NULL, &connection)) {
        process_last_error();
        PRINT_TEST_FAILED;
        return false;
    }

    reset_thread_terminate(g_listener_thread_data);
    snapshot_order_data_reset();
    drop_event_subscription_counter();

    //create snapshot
    if (!create_order_snapshot(connection, &order_snapshot)) {
        dxf_close_connection(connection);
        PRINT_TEST_FAILED;
        return false;
    }

    //create event
    if (!create_event_subscription(connection, &event_subscription)) {
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

bool snapshot_all_test(void) {
    bool res = true;
    init_listener_thread_data(&g_listener_thread_data);
    snapshot_order_data_init();
    snapshot_candle_data_init();
    init_event_subscription_counter();

    if (!snapshot_initialization_test() ||
        !snapshot_duplicates_test() ||
        !snapshot_subscription_test() /*||
        !snapshot_multiply_subscription_test() ||
        !snapshot_subscription_and_events_test()*/) {

        res = false;
    }

    free_subscription_event_counter();
    snapshot_candle_data_free();
    snapshot_order_data_free();
    free_listener_thread_data(g_listener_thread_data);
    g_listener_thread_data = NULL;
    return res;
}
