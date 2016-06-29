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

#define CANDLE_DEFAULT_SYMBOL L"AAPL"
#define CANDLE_USER_EXCHANGE L'A'
#define CANDLE_USER_PERIOD_VALUE 2

//Timeout in milliseconds for waiting some events is 2 minutes.
#define EVENTS_TIMEOUT 120000
#define EVENTS_LOOP_SLEEP_TIME 100

#define EPSILON 0.000001

typedef struct {
    dxf_uint_t event_counter;
    CRITICAL_SECTION event_counter_guard;
} event_counter_data_t, *event_counter_data_ptr_t;

typedef struct {
    dxf_const_string_t symbol;
    dxf_char_t exchange_code;
    dxf_double_t period_value;
    dxf_candle_type_period_attribute_t period_type;
    dxf_candle_price_attribute_t price;
    dxf_candle_session_attribute_t session;
    dxf_candle_alignment_attribute_t alignment;

    dxf_const_string_t expected;
    int line;
} candle_attribute_test_case_t;

static const char dxfeed_host[] = "mddqa.in.devexperts.com:7400";

candle_attribute_test_case_t g_candle_attribute_cases[] = {
    { CANDLE_DEFAULT_SYMBOL, DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT, 
    dxf_ctpa_default, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL{}", __LINE__ }, 
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
    dxf_ctpa_default, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{}", __LINE__ },

    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
    dxf_ctpa_second, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=s}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
    dxf_ctpa_minute, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=m}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
    dxf_ctpa_hour, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=h}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
    dxf_ctpa_day, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=d}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
    dxf_ctpa_week, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=w}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
    dxf_ctpa_month, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=mo}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
    dxf_ctpa_optexp, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=o}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
    dxf_ctpa_year, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=y}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
    dxf_ctpa_volume, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=v}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
    dxf_ctpa_price, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=p}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
    dxf_ctpa_price_momentum, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=pm}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
    dxf_ctpa_price_renko, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=pr}", __LINE__ },

    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_default, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2t}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_second, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2s}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_minute, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2m}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_hour, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2h}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_day, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2d}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_week, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2w}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_month, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2mo}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_optexp, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2o}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_year, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2y}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_volume, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2v}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_price, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2p}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_price_momentum, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2pm}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_price_renko, dxf_cpa_default, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2pr}", __LINE__ },

    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_day, dxf_cpa_bid, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2d,price=bid}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_day, dxf_cpa_ask, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2d,price=ask}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_day, dxf_cpa_mark, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2d,price=mark}", __LINE__ },
    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_day, dxf_cpa_settlement, dxf_csa_default, dxf_caa_default, L"AAPL&A{=2d,price=s}", __LINE__ },

    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_day, dxf_cpa_mark, dxf_csa_regular, dxf_caa_default, L"AAPL&A{=2d,price=mark,tho=true}", __LINE__ },

    { CANDLE_DEFAULT_SYMBOL, CANDLE_USER_EXCHANGE, CANDLE_USER_PERIOD_VALUE,
    dxf_ctpa_day, dxf_cpa_mark, dxf_csa_regular, dxf_caa_session, L"AAPL&A{=2d,a=s,price=mark,tho=true}", __LINE__ }
};

static event_counter_data_t g_aapl_candle_event_counter_data;
static event_counter_data_t g_ibm_candle_event_counter_data;
static event_counter_data_t g_order_event_counter_data;
static event_counter_data_ptr_t g_aapl_candle_data = &g_aapl_candle_event_counter_data;
static event_counter_data_ptr_t g_ibm_candle_data = &g_ibm_candle_event_counter_data;
static event_counter_data_ptr_t g_order_data = &g_order_event_counter_data;
static dxf_candle_t g_last_candle = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* -------------------------------------------------------------------------- */

//TODO: rem
void process_last_error_c() {
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
static bool is_listener_thread_terminated = false;
CRITICAL_SECTION listener_thread_guard;

bool is_thread_terminate_c() {
    bool res;
    EnterCriticalSection(&listener_thread_guard);
    res = is_listener_thread_terminated;
    LeaveCriticalSection(&listener_thread_guard);

    return res;
}

/* -------------------------------------------------------------------------- */

void on_reader_thread_terminate_c(const char* host, void* user_data) {
    EnterCriticalSection(&listener_thread_guard);
    is_listener_thread_terminated = true;
    LeaveCriticalSection(&listener_thread_guard);

    printf("\nTerminating listener thread, host: %s\n", host);
}

/* -------------------------------------------------------------------------- */

void init_event_counter(event_counter_data_ptr_t counter_data) {
    InitializeCriticalSection(&counter_data->event_counter_guard);
    counter_data->event_counter = 0;
}

void free_event_counter(event_counter_data_ptr_t counter_data) {
    DeleteCriticalSection(&counter_data->event_counter_guard);
}

void inc_event_counter(event_counter_data_ptr_t counter_data) {
    EnterCriticalSection(&counter_data->event_counter_guard);
    counter_data->event_counter++;
    LeaveCriticalSection(&counter_data->event_counter_guard);
}

dxf_uint_t get_event_counter(event_counter_data_ptr_t counter_data) {
    dxf_uint_t value = 0;
    EnterCriticalSection(&counter_data->event_counter_guard);
    value = counter_data->event_counter;
    LeaveCriticalSection(&counter_data->event_counter_guard);
    return value;
}

void drop_event_counter(event_counter_data_ptr_t counter_data) {
    EnterCriticalSection(&counter_data->event_counter_guard);
    counter_data->event_counter = 0;
    LeaveCriticalSection(&counter_data->event_counter_guard);
}

/* -------------------------------------------------------------------------- */

void aapl_candle_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data,
                          dxf_event_flags_t flags, int data_count, void* user_data) {
    dxf_candle_t* candles = NULL;

    /* symbol hardcoded */
    if (event_type != DXF_ET_CANDLE || wcscmp(symbol_name, L"AAPL{=d,price=mark}") != 0 || data_count < 1)
        return;
    inc_event_counter(g_aapl_candle_data);
    candles = (dxf_candle_t*)data;
    memcpy(&g_last_candle, &(candles[data_count - 1]), sizeof(dxf_candle_t));
}

void ibm_candle_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data,
                         dxf_event_flags_t flags, int data_count, void* user_data) {
    /* symbol hardcoded */
    if (event_type != DXF_ET_CANDLE || wcscmp(symbol_name, L"IBM{=d,price=mark}") != 0 || data_count < 1)
        return;
    inc_event_counter(g_ibm_candle_data);
}

void order_event_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data,
                    dxf_event_flags_t flags, int data_count, void* user_data) {
    if (event_type != DXF_ET_ORDER || data_count < 1)
        return;
    inc_event_counter(g_order_data);
}

/* -------------------------------------------------------------------------- */

bool create_event_subscription(dxf_connection_t connection, int event_type, 
                               dxf_event_listener_t event_listener, 
                               OUT dxf_subscription_t* res_subscription) {
    dxf_subscription_t subscription = NULL;

    if (!dxf_create_subscription(connection, event_type, &subscription)) {
        process_last_error_c();

        return false;
    };

    if (!dxf_add_symbol(subscription, CANDLE_DEFAULT_SYMBOL)) {
        process_last_error_c();
        dxf_close_subscription(subscription);
        return false;
    };

    if (!dxf_attach_event_listener(subscription, event_listener, NULL)) {
        process_last_error_c();
        dxf_close_subscription(subscription);
        return false;
    };
    
    *res_subscription = subscription;
    return true;
}

bool create_candle_subscription(dxf_connection_t connection, dxf_const_string_t symbol, 
                                dxf_event_listener_t event_listener,
                                OUT dxf_subscription_t* res_subscription) {
    dxf_subscription_t subscription = NULL;
    dxf_candle_attributes_t candle_attributes = NULL;

    if (!dxf_create_subscription_timed(connection, DXF_ET_CANDLE, 0, &subscription)) {
        process_last_error_c();
        return false;
    };

    if (!dxf_initialize_candle_symbol_attributes(symbol, DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT,
        DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT, dxf_ctpa_day,
        dxf_cpa_mark, dxf_csa_default, dxf_caa_default, &candle_attributes)) {

        process_last_error_c();
        dxf_close_subscription(subscription);
        return false;
    }

    if (!dxf_add_candle_symbol(subscription, candle_attributes)) {
        process_last_error_c();
        dxf_delete_candle_symbol_attributes(candle_attributes);
        dxf_close_subscription(subscription);
        return false;
    };

    if (!dxf_attach_event_listener(subscription, event_listener, NULL)) {
        process_last_error_c();
        dxf_delete_candle_symbol_attributes(candle_attributes);
        dxf_close_subscription(subscription);
        return false;
    };

    dxf_delete_candle_symbol_attributes(candle_attributes);
    *res_subscription = subscription;
    return true;
}

bool wait_events(int (*get_counter_function)()) {
    int timestamp = dx_millisecond_timestamp();
    while (get_counter_function() == 0) {
        if (is_thread_terminate_c()) {
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

bool dx_is_non_zero(dxf_long_t actual) {
    if (actual == 0) {
        wprintf(L"%ls failed: expected non-zero, but was 0\n", __FUNCTIONW__);
        return false;
    }
    return true;
}

bool dx_is_greater(double a, double b) {
    bool res = (a - b) > ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * DBL_EPSILON);
    if (!res) {
        wprintf(L"%ls failed: %f > %f\n", __FUNCTIONW__, a, b);
        return false;
    }
    return true;
}

/* -------------------------------------------------------------------------- */

bool candle_attributes_test(void) {
    int candle_attribute_cases_size = sizeof(g_candle_attribute_cases) / sizeof(g_candle_attribute_cases[0]);
    int i;
    for (i = 0; i < candle_attribute_cases_size; i++) {
        dxf_candle_attributes_t attributes;
        candle_attribute_test_case_t* params = &g_candle_attribute_cases[i];
        dxf_string_t attributes_string = NULL;
        bool res = true;
        dxf_initialize_candle_symbol_attributes(params->symbol, params->exchange_code,
            params->period_value, params->period_type, params->price, params->session, 
            params->alignment, &attributes);
        if (!dx_candle_symbol_to_string(attributes, &attributes_string)) {
            process_last_error_c();
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

bool candle_subscription_test(void) {
    dxf_connection_t connection = NULL;
    dxf_subscription_t subscription = NULL;

    if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate_c, NULL, NULL, NULL, &connection)) {
        process_last_error_c();
        return false;
    }

    drop_event_counter(g_aapl_candle_data);

    if (!create_candle_subscription(connection, CANDLE_DEFAULT_SYMBOL, aapl_candle_listener, &subscription)) {
        dxf_close_connection(connection);
        return false;
    }

    if (!wait_events(get_aapl_candle_counter)) {
        dxf_close_subscription(subscription);
        dxf_close_connection(connection);
        return false;
    }

    if (!dx_is_non_zero(g_last_candle.time) ||
        !dx_is_greater(g_last_candle.count, 0.0) ||
        !dx_is_greater(g_last_candle.open, 0.0) ||
        !dx_is_greater(g_last_candle.high, 0.0) ||
        !dx_is_greater(g_last_candle.low, 0.0) ||
        !dx_is_greater(g_last_candle.close, 0.0)) {

        dxf_close_subscription(subscription);
        dxf_close_connection(connection);
        return false;
    }

    if (!dxf_close_subscription(subscription)) {
        process_last_error_c();
        dxf_close_connection(connection);
        return false;
    }

    if (!dxf_close_connection(connection)) {
        process_last_error_c();
        return false;
    }

    return true;
}

bool candle_multiply_subscription_test(void) {
    dxf_connection_t connection = NULL;
    dxf_subscription_t aapl_candle_subscription = NULL;
    dxf_subscription_t ibm_candle_subscription = NULL;
    dxf_subscription_t order_subscription = NULL;

    if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate_c, NULL, NULL, NULL, &connection)) {
        process_last_error_c();
        return false;
    }

    drop_event_counter(g_aapl_candle_data);
    if (!create_candle_subscription(connection, CANDLE_DEFAULT_SYMBOL, aapl_candle_listener, &aapl_candle_subscription)) {
        dxf_close_connection(connection);
        return false;
    }
    drop_event_counter(g_ibm_candle_data);
    if (!create_candle_subscription(connection, L"IBM", ibm_candle_listener, &ibm_candle_subscription)) {
        dxf_close_subscription(aapl_candle_subscription);
        dxf_close_connection(connection);
        return false;
    }
    drop_event_counter(g_order_data);
    if (!create_event_subscription(connection, DXF_ET_ORDER, order_event_listener, &order_subscription)) {
        dxf_close_subscription(ibm_candle_subscription);
        dxf_close_subscription(aapl_candle_subscription);
        dxf_close_connection(connection);
        return false;
    }

    if (!wait_events(get_aapl_candle_counter) ||
        !wait_events(get_ibm_candle_counter) ||
        !wait_events(get_order_event_counter)) {

        dxf_close_subscription(order_subscription);
        dxf_close_subscription(ibm_candle_subscription);
        dxf_close_subscription(aapl_candle_subscription);
        dxf_close_connection(connection);
        return false;
    }

    //close one candle subscription
    if (!dxf_close_subscription(ibm_candle_subscription)) {
        dxf_close_subscription(order_subscription);
        dxf_close_subscription(aapl_candle_subscription);
        dxf_close_connection(connection);
    }

    //wait events from left subscriptions
    drop_event_counter(g_aapl_candle_data);
    drop_event_counter(g_ibm_candle_data);
    drop_event_counter(g_order_data);
    if (!wait_events(get_aapl_candle_counter) ||
        !wait_events(get_order_event_counter)) {

        dxf_close_subscription(order_subscription);
        dxf_close_subscription(aapl_candle_subscription);
        dxf_close_connection(connection);
        return false;
    }

    if (!dxf_close_subscription(order_subscription) ||
        !dxf_close_subscription(aapl_candle_subscription)) {

        process_last_error_c();
        dxf_close_connection(connection);
        return false;
    }

    if (!dxf_close_connection(connection)) {
        process_last_error_c();
        return false;
    }

    return true;
}

bool candle_all_tests(void) {
    bool res = true;

    dxf_initialize_logger("log.log", true, true, true);

    InitializeCriticalSection(&listener_thread_guard);
    init_event_counter(g_aapl_candle_data);
    init_event_counter(g_ibm_candle_data);
    init_event_counter(g_order_data);

    if (!candle_attributes_test() ||
        !candle_subscription_test() ||
        !candle_multiply_subscription_test()) {

        res = false;
    }

    free_event_counter(g_order_data);
    free_event_counter(g_ibm_candle_data);
    free_event_counter(g_aapl_candle_data);
    DeleteCriticalSection(&listener_thread_guard);

    return res;
}
