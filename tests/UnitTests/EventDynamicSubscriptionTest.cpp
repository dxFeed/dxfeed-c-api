#include <stdio.h>
#include <time.h>
#include <Windows.h>
#include "EventDynamicSubscriptionTest.h"
#include "DXAlgorithms.h"
#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "Logger.h"

//Timeout in milliseconds for waiting some events is 1 minutes.
#define EVENTS_TIMEOUT 60000
#define EVENTS_LOOP_SLEEP_TIME 100

const char dxfeed_host[] = "demo.dxfeed.com:7300";

#define SYMBOLS_COUNT 4
static dxf_const_string_t g_symbols[] = { { L"IBM" }, { L"MSFT" }, { L"YHOO" }, { L"C" } };

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
static bool is_listener_thread_terminated = false;
CRITICAL_SECTION listener_thread_guard;

bool is_thread_terminate() {
    bool res;
    EnterCriticalSection(&listener_thread_guard);
    res = is_listener_thread_terminated;
    LeaveCriticalSection(&listener_thread_guard);

    return res;
}

/* -------------------------------------------------------------------------- */

void on_reader_thread_terminate(const char* host, void* user_data) {
    EnterCriticalSection(&listener_thread_guard);
    is_listener_thread_terminated = true;
    LeaveCriticalSection(&listener_thread_guard);

    printf("\nTerminating listener thread, host: %s\n", host);
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

void trade_listener(int event_type, dxf_const_string_t symbol_name,
                    const dxf_event_data_t* data, int data_count,
                    const dxf_event_params_t* event_params, void* user_data) {
    wprintf(L"%s{symbol=%s}\n", dx_event_type_to_string(event_type), symbol_name);

    inc_trade_counter();
}

void order_listener(int event_type, dxf_const_string_t symbol_name,
                    const dxf_event_data_t* data, int data_count,
                    const dxf_event_params_t* event_params, void* user_data) {
    wprintf(L"%s{symbol=%s}\n", dx_event_type_to_string(event_type), symbol_name);

    inc_order_counter();
}
/* -------------------------------------------------------------------------- */

void process_last_error() {
    int error_code = dx_ec_success;
    dxf_const_string_t error_descr = NULL;
    int res;

    res = dxf_get_last_error(&error_code, &error_descr);

    if (res == DXF_SUCCESS) {
        if (error_code == dx_ec_success) {
            printf("WTF - no error information is stored");

            return;
        }

        wprintf(L"Error occurred and successfully retrieved:\n"
            L"error code = %d, description = \"%s\"\n",
            error_code, error_descr);
        return;
    }

    printf("An error occurred but the error subsystem failed to initialize\n");
}
/* -------------------------------------------------------------------------- */

bool subscribe_to_event(dxf_connection_t connection, dxf_subscription_t* subscription, int event_type, dxf_event_listener_t event_listener) {
    if (!dxf_create_subscription(connection, event_type, subscription)) {
        process_last_error();

        return false;
    };

    if (!dxf_add_symbols(*subscription, g_symbols, SYMBOLS_COUNT)) {
        process_last_error();

        return false;
    };

    if (!dxf_attach_event_listener(*subscription, event_listener, NULL)) {
        process_last_error();

        return false;
    };
    wprintf(L"Subscription on %ls is successful!\n", dx_event_type_to_string(event_type));
    return true;
}

/* -------------------------------------------------------------------------- */

bool event_dynamic_subscription_test(void) {
    dxf_connection_t connection = NULL;
    dxf_subscription_t trade_subscription = NULL;
    dxf_subscription_t order_subscription = NULL;
    int loop_counter = EVENTS_TIMEOUT / EVENTS_LOOP_SLEEP_TIME;
    int timestamp = 0;

    dxf_initialize_logger("log.log", true, true, true);
    InitializeCriticalSection(&listener_thread_guard);
    InitializeCriticalSection(&g_order_counter_guard);
    InitializeCriticalSection(&g_trade_counter_guard);

    printf("EventDynamicSubscription test started.\n");
    printf("Connecting to host %s...\n", dxfeed_host);

    if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, &connection)) {
        process_last_error();
        return false;
    }

    printf("Connection successful!\n");

    //Subscribe on trade events
    if (!subscribe_to_event(connection, &trade_subscription, DXF_ET_TRADE, trade_listener)) {
        return false;
    }

    //Wait trade events
    printf("Wait events from trade...\n");
    timestamp = dx_millisecond_timestamp();
    while (get_trade_counter() == 0) {
        if (is_thread_terminate()) {
            printf("Error: Thread was terminated!\n");
            return false;
        }
        if (dx_millisecond_timestamp_diff(dx_millisecond_timestamp(), timestamp) > EVENTS_TIMEOUT) {
            printf("Test failed: timeout is elapsed!");
            return false;
        }
        Sleep(EVENTS_LOOP_SLEEP_TIME);
    }

    //Subscribe on order events
    if (!subscribe_to_event(connection, &order_subscription, DXF_ET_ORDER, order_listener)) {
        return false;
    }

    drop_order_counter();
    drop_trade_counter();

    //Check trade and order events
    printf("Wait events from both trade and order...\n");
    timestamp = dx_millisecond_timestamp();
    while (get_trade_counter() == 0 || get_order_counter() == 0) {
        if (is_thread_terminate()) {
            printf("Error: Thread was terminated!\n");
            return false;
        }
        if (dx_millisecond_timestamp_diff(dx_millisecond_timestamp(), timestamp) > EVENTS_TIMEOUT) {
            printf("Test failed: timeout is elapsed!");
            return false;
        }
        Sleep(EVENTS_LOOP_SLEEP_TIME);
    }

    //Unsubscribe trade events
    dxf_detach_event_listener(trade_subscription, trade_listener);
    drop_order_counter();
    drop_trade_counter();

    //Check order events
    printf("Wait events from order event only...\n");
    while (loop_counter--) {
        if (is_thread_terminate()) {
            printf("Error: Thread was terminated!\n");
            return false;
        }
        Sleep(EVENTS_LOOP_SLEEP_TIME);
    }
    if (get_order_counter() == 0 || get_trade_counter() > 0) {
        printf("Test failed: no order events or trade events is occur!");
        return false;
    }

    DeleteCriticalSection(&listener_thread_guard);
    DeleteCriticalSection(&g_order_counter_guard);
    DeleteCriticalSection(&g_trade_counter_guard);

    dxf_detach_event_listener(order_subscription, order_listener);

    dxf_clear_symbols(trade_subscription);
    dxf_clear_symbols(order_subscription);

    dxf_close_subscription(trade_subscription);
    dxf_close_subscription(order_subscription);

    printf("Disconnecting from host...\n");
    if (!dxf_close_connection(connection)) {
        process_last_error();

        return false;
    }

    printf("Disconnect successful!\n"
        "Connection test completed successfully!\n");

    return true;
}