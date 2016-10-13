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

#define DXFEED_HOST "mddqa.in.devexperts.com:7400"

#define SIZE_OF_ARRAY(counter_function_atatic_array) sizeof(counter_function_atatic_array) / sizeof(counter_function_atatic_array[0])

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

/*Test*/
bool mixed_order_source_test() {
    dxf_connection_t connection = NULL;
    dxf_subscription_t subscription = NULL;
    dxf_const_string_t symbols[] = { { L"IBM" }, { L"MSFT" }, { L"YHOO" }, { L"C" }, { L"AAPL" } };
    dxf_int_t symbol_count = sizeof(symbols) / sizeof(symbols[0]);

    drop_event_counter(g_ntv_counter);
    drop_event_counter(g_dex_counter);
    drop_event_counter(g_dea_counter);

    if (!dxf_create_connection(DXFEED_HOST, order_source_tests_on_thread_terminate, NULL, NULL, NULL, &connection) ||
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
    Sleep(1000);
    drop_event_counter(g_ntv_counter);
    drop_event_counter(g_dex_counter);
    drop_event_counter(g_dea_counter);
    if (!ost_wait_events(ost_get_ntv_counter) ||
        !dx_is_equal_int(0, ost_get_dex_counter()) ||
        !dx_is_equal_int(0, ost_get_dea_counter())) {

        printf("    Expected: ntv=%5s, dex=%5s, dea=%5s; \n", ">0", "0", "0");
        printf("    But was:  ntv=%5d, dex=%5d, dea=%5d.\n\n", 
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
    Sleep(1000);
    drop_event_counter(g_ntv_counter);
    drop_event_counter(g_dex_counter);
    drop_event_counter(g_dea_counter);
    if (!ost_wait_two_events(ost_get_ntv_counter, ost_get_dex_counter) ||
        !dx_is_equal_int(0, ost_get_dea_counter())) {
        
        printf("    Expected: ntv=%5s, dex=%5s, dea=%5s; \n", ">0", ">0", "0");
        printf("    But was:  ntv=%5d, dex=%5d, dea=%5d.\n\n",
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

/*Test*/
bool set_order_source_test() {
    dxf_connection_t connection = NULL;
    dxf_subscription_t subscription = NULL;
    dxf_const_string_t symbols[] = { { L"IBM" }, { L"MSFT" }, { L"YHOO" }, { L"C" }, { L"AAPL" } };
    dxf_int_t symbol_count = sizeof(symbols) / sizeof(symbols[0]);

    drop_event_counter(g_ntv_counter);
    drop_event_counter(g_dex_counter);
    drop_event_counter(g_dea_counter);

    if (!dxf_create_connection(DXFEED_HOST, order_source_tests_on_thread_terminate, NULL, NULL, NULL, &connection) ||
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
    Sleep(1000);
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
    Sleep(1000);
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

/*Test*/
bool add_order_source_test() {
    dxf_connection_t connection = NULL;
    dxf_subscription_t subscription = NULL;
    dxf_const_string_t symbols[] = { L"IBM", L"MSFT", L"YHOO", L"C", L"AAPL", L"XBT/USD" };
    dxf_int_t symbol_count = sizeof(symbols) / sizeof(symbols[0]);

    drop_event_counter(g_ntv_counter);
    drop_event_counter(g_dex_counter);
    drop_event_counter(g_dea_counter);

    if (!dxf_create_connection(DXFEED_HOST, order_source_tests_on_thread_terminate, NULL, NULL, NULL, &connection) ||
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
    Sleep(1000);
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
    Sleep(1000);
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


bool order_source_configuration_test(void) {
    bool res = true;

    dxf_initialize_logger("log.log", true, true, true);

    init_listener_thread_data(&g_ost_listener_thread_data);
    init_event_counter(g_ntv_counter);
    init_event_counter(g_dex_counter);
    init_event_counter(g_dea_counter);

    if (!add_order_source_test() ||
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
