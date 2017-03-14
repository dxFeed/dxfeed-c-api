#ifndef TEST_HELPER_H_INCLUDED
#define TEST_HELPER_H_INCLUDED


#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "PrimitiveTypes.h"

#define SIZE_OF_ARRAY(static_array) sizeof(static_array) / sizeof(static_array[0])

/* -------------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------- */

typedef void* dxf_listener_thread_data_t;

void init_listener_thread_data(OUT dxf_listener_thread_data_t* data);
void free_listener_thread_data(dxf_listener_thread_data_t data);
bool is_thread_terminate(dxf_listener_thread_data_t data);
void on_reader_thread_terminate(dxf_listener_thread_data_t data, dxf_connection_t connection, void* user_data);
void reset_thread_terminate(dxf_listener_thread_data_t data);

/* -------------------------------------------------------------------------- */

void process_last_error();
bool create_event_subscription(dxf_connection_t connection, int event_type,
                               dxf_const_string_t symbol,
                               dxf_event_listener_t event_listener,
                               OUT dxf_subscription_t* res_subscription);

/* -------------------------------------------------------------------------- */
/* Event counter data */
typedef struct {
    const char *counter_name;
    dxf_uint_t event_counter;
    CRITICAL_SECTION event_counter_guard;
} event_counter_data_t, *event_counter_data_ptr_t;

/* Event counter functions */
void init_event_counter(event_counter_data_ptr_t counter_data);
void init_event_counter2(event_counter_data_ptr_t counter_data, const char *name);
void free_event_counter(event_counter_data_ptr_t counter_data);
void inc_event_counter(event_counter_data_ptr_t counter_data);
dxf_uint_t get_event_counter(event_counter_data_ptr_t counter_data);
void drop_event_counter(event_counter_data_ptr_t counter_data);
const char *get_event_counter_name(event_counter_data_ptr_t counter_data);

/* -------------------------------------------------------------------------- */

#define PRINT_TEST_FAILED printf("%s failed! File: %s, line: %d\n", __FUNCTION__, __FILE__, __LINE__);

#define DX_IS_EQUAL_FUNCTION_DECLARATION(type) bool dx_is_equal_##type##(type expected, type actual)
#define DX_IS_GREATER_OR_EQUAL_FUNCTION_DECLARATION(type) bool dx_ge_##type##(type actual, type param)

DX_IS_EQUAL_FUNCTION_DECLARATION(bool);
DX_IS_EQUAL_FUNCTION_DECLARATION(int);
DX_IS_EQUAL_FUNCTION_DECLARATION(ERRORCODE);
DX_IS_EQUAL_FUNCTION_DECLARATION(dxf_const_string_t);
DX_IS_EQUAL_FUNCTION_DECLARATION(dxf_string_t);
DX_IS_EQUAL_FUNCTION_DECLARATION(dxf_uint_t);
DX_IS_EQUAL_FUNCTION_DECLARATION(dxf_long_t);
DX_IS_EQUAL_FUNCTION_DECLARATION(dxf_ulong_t);
DX_IS_EQUAL_FUNCTION_DECLARATION(double);
DX_IS_EQUAL_FUNCTION_DECLARATION(size_t);

DX_IS_GREATER_OR_EQUAL_FUNCTION_DECLARATION(dxf_uint_t);

bool dx_is_not_null(void* actual);
bool dx_is_null(void* actual);

bool dx_is_equal_ptr(void* expected, void* actual);

#endif //TEST_HELPER_H_INCLUDED