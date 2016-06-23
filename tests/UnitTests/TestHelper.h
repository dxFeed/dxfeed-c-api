#ifndef TEST_HELPER_H_INCLUDED
#define TEST_HELPER_H_INCLUDED


#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "PrimitiveTypes.h"

typedef void* dxf_listener_thread_data_t;

void init_listener_thread_data(OUT dxf_listener_thread_data_t* data);
void free_listener_thread_data(dxf_listener_thread_data_t data);
bool is_thread_terminate(dxf_listener_thread_data_t data);
void on_reader_thread_terminate(dxf_listener_thread_data_t data, dxf_connection_t connection, void* user_data);
void reset_thread_terminate(dxf_listener_thread_data_t data);

/* -------------------------------------------------------------------------- */

void process_last_error();

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

DX_IS_GREATER_OR_EQUAL_FUNCTION_DECLARATION(dxf_uint_t);

bool dx_is_not_null(void* actual);
bool dx_is_null(void* actual);

#endif //TEST_HELPER_H_INCLUDED