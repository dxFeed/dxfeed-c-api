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


const char dxfeed_host_[] = "demo.dxfeed.com:7300";
dxf_int_t count_NTV = 0;
dxf_int_t count_DEX = 0;
dxf_int_t count_other = 0;

/* -------------------------------------------------------------------------- */
#ifdef _WIN32
static bool is_listener_thread_terminated = false;
CRITICAL_SECTION listener_thread_guard;

bool is_thread_terminate_() {
    bool res;
    EnterCriticalSection(&listener_thread_guard);
    res = is_listener_thread_terminated;
    LeaveCriticalSection(&listener_thread_guard);

    return res;
}

void on_reader_thread_terminate_(dxf_connection_t connection, void* user_data) {
    EnterCriticalSection(&listener_thread_guard);
    is_listener_thread_terminated = true;
    LeaveCriticalSection(&listener_thread_guard);

    wprintf(L"\nTerminating listener thread\n");
}
#endif

void process_last_error_() {
    int error_code = dx_ec_success;
    dxf_const_string_t error_descr = NULL;
    int res;

    res = dxf_get_last_error(&error_code, &error_descr);

    if (res == DXF_SUCCESS) {
        if (error_code == dx_ec_success) {
            wprintf(L"WTF - no error information is stored");
            return;
        }

        wprintf(L"Error occurred and successfully retrieved:\n"
            L"error code = %d, description = \"%ls\"\n",
            error_code, error_descr);
        return;
    }

    wprintf(L"An error occurred but the error subsystem failed to initialize\n");
}

void print_timestamp_(dxf_long_t timestamp) {
    wchar_t timefmt[80];

    struct tm * timeinfo;
    time_t tmpint = (time_t)(timestamp / 1000);
    timeinfo = localtime(&tmpint);
    wcsftime(timefmt, 80, L"%Y%m%d-%H%M%S", timeinfo);
    wprintf(L"%ls", timefmt);
}


void listener(int event_type, dxf_const_string_t symbol_name,
              const dxf_event_data_t* data, int data_count, void* user_data) {
    dxf_int_t i = 0;
    dxf_order_t* orders = (dxf_order_t*)data;

    wprintf(L"%ls{symbol=%ls, ", dx_event_type_to_string(event_type), symbol_name);

    for (; i < data_count; ++i) {
        wprintf(L"index=0x%llX, side=%i, level=%i, time=", orders[i].index, orders[i].side, orders[i].level);
        print_timestamp_(orders[i].time);
        wprintf(L", exchange code=%c, market maker=%ls, price=%f, size=%lld", 
            orders[i].exchange_code, orders[i].market_maker, orders[i].price, orders[i].size);
        if (wcslen(orders[i].source) > 0)
            wprintf(L", source=%s", orders[i].source);
        wprintf(L"}\n");
        if (dx_compare_strings(orders[i].source, L"NTV") == 0)
            ++count_NTV;
        else if (dx_compare_strings(orders[i].source, L"DEX") == 0)
            ++count_DEX;
        else
            ++count_other;

    }
    wprintf(L"NTV count: %i, DEX count: %i, others count: %i\n", count_NTV, count_DEX, count_other);
}

bool order_source_configuration_test(void)
{
    dxf_connection_t connection = NULL;
    dxf_subscription_t subscription = NULL;
    int loop_counter = 10000;


    dxf_initialize_logger("OrderTest.log", true, true, true);

#ifdef _WIN32
    InitializeCriticalSection(&listener_thread_guard);
#endif

    if (!dxf_create_connection(dxfeed_host_, on_reader_thread_terminate_, NULL, NULL, NULL, &connection)) {
        process_last_error_();
    }

    if (!dxf_create_subscription(connection, DXF_ET_ORDER, &subscription)) {
        process_last_error_();
        return false;
    }

    {
        dxf_const_string_t symbols[] = { { L"IBM" }, { L"MSFT" }, { L"YHOO" }, { L"C" }, { L"AAPL"} };
        dxf_int_t symbol_count = 5;
        if (!dxf_add_symbols(subscription, symbols, symbol_count)) {
            process_last_error_();
            return false;
        }
    }
    //if (!dxf_set_order_source(subscription, "NTV")) {
    //    process_last_error_();
    //    return false;
    //}

    if (!dxf_attach_event_listener(subscription, listener, NULL)) {
        process_last_error_();
        return false;
    }

    Sleep(10000);

    if (!dxf_set_order_source(subscription, "NTV")) {
        process_last_error_();
        return false;
    }

    wprintf(L"\n\n\n\n\nSubscription successful!\n\n\n\n\n");

    //Sleep(10000);

    //if (!dxf_add_order_source(subscription, "DEX")) {
    //    process_last_error_();
    //    return false;
    //}

    while (!is_thread_terminate_() && loop_counter--) {
#ifdef _WIN32
        Sleep(100);
#else
        sleep(1);
#endif
    }

    return true;
}
