#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <string.h>
#include <wctype.h>
#include <stdlib.h>
#define stricmp strcasecmp
#endif

#include "TestHelper.h"

#ifdef _WIN32
typedef struct {
    bool is_listener_thread_terminated;
    CRITICAL_SECTION listener_thread_guard;
} dx_listener_thread_data_t;

void init_listener_thread_data(OUT dxf_listener_thread_data_t* data) {
    dx_listener_thread_data_t* internal_data = calloc(1, sizeof(dx_listener_thread_data_t));
    internal_data->is_listener_thread_terminated = false;
    InitializeCriticalSection(&internal_data->listener_thread_guard);
    *data = (dxf_listener_thread_data_t)internal_data;
}

void free_listener_thread_data(dxf_listener_thread_data_t data) {
    dx_listener_thread_data_t* internal_data = (dx_listener_thread_data_t*)data;
    DeleteCriticalSection(&internal_data->listener_thread_guard);
    free(data);
}

bool is_thread_terminate(dxf_listener_thread_data_t data) {
    bool res;
    dx_listener_thread_data_t* internal_data = (dx_listener_thread_data_t*)data;
    EnterCriticalSection(&internal_data->listener_thread_guard);
    res = internal_data->is_listener_thread_terminated;
    LeaveCriticalSection(&internal_data->listener_thread_guard);
    return res;
}

void on_reader_thread_terminate(dxf_listener_thread_data_t data, dxf_connection_t connection, void* user_data) {
    dx_listener_thread_data_t* internal_data = (dx_listener_thread_data_t*)data;
    EnterCriticalSection(&internal_data->listener_thread_guard);
    internal_data->is_listener_thread_terminated = true;
    LeaveCriticalSection(&internal_data->listener_thread_guard);

    wprintf(L"\nTerminating listener thread\n");
}

void reset_thread_terminate(dxf_listener_thread_data_t data) {
    dx_listener_thread_data_t* internal_data = (dx_listener_thread_data_t*)data;
    EnterCriticalSection(&internal_data->listener_thread_guard);
    internal_data->is_listener_thread_terminated = false;
    LeaveCriticalSection(&internal_data->listener_thread_guard);
}

#else
typedef struct {
    volatile bool is_listener_thread_terminated;
} dx_listener_thread_data_t;

void init_listener_thread_data(OUT dxf_listener_thread_data_t* data) {
    dx_listener_thread_data_t* internal_data = calloc(1, sizeof(dx_listener_thread_data_t));
    internal_data->is_listener_thread_terminated = false;
    *data = (dxf_listener_thread_data_t)internal_data;
}

void free_listener_thread_data(dxf_listener_thread_data_t data) {
    free(data);
}

bool is_thread_terminate(dxf_listener_thread_data_t data) {
    bool res;
    dx_listener_thread_data_t* internal_data = (dx_listener_thread_data_t*)data;
    res = internal_data->is_listener_thread_terminated;
    return res;
}

void on_reader_thread_terminate(dxf_connection_t connection, void* user_data) {
    dx_listener_thread_data_t* internal_data = (dx_listener_thread_data_t*)data;
    internal_data->is_listener_thread_terminated = true;
    wprintf(L"\nTerminating listener thread\n");
}
#endif

/* -------------------------------------------------------------------------- */

void process_last_error() {
    int error_code = dx_ec_success;
    dxf_const_string_t error_descr = NULL;
    int res;

    res = dxf_get_last_error(&error_code, &error_descr);

    if (res == DXF_SUCCESS) {
        if (error_code == dx_ec_success) {
            wprintf(L"no error information is stored");
            return;
        }

        wprintf(L"Error occurred and successfully retrieved:\n"
            L"error code = %d, description = \"%ls\"\n",
            error_code, error_descr);
        return;
    }

    wprintf(L"An error occurred but the error subsystem failed to initialize\n");
}

/* -------------------------------------------------------------------------- */

#define DX_IS_EQUAL_FUNCTION_DEFINITION(type, tmpl) \
DX_IS_EQUAL_FUNCTION_DECLARATION(type) { \
    if (expected != actual) { \
        wprintf(L"%ls failed: expected=" tmpl L", but was=" tmpl L"\n", __FUNCTIONW__, expected, actual); \
        return false; \
            } \
    return true; \
}

#define DX_IS_EQUAL_STRING_FUNCTION_DEFINITION(type, tmpl) \
DX_IS_EQUAL_FUNCTION_DECLARATION(type) { \
    if (wcscmp(expected, actual) != 0) { \
        wprintf(L"%ls failed: expected=" tmpl L", but was=" tmpl L"\n", __FUNCTIONW__, expected, actual); \
        return false; \
                } \
    return true; \
}

DX_IS_EQUAL_FUNCTION_DEFINITION(bool, L"%d")
DX_IS_EQUAL_FUNCTION_DEFINITION(int, L"%d")
DX_IS_EQUAL_FUNCTION_DEFINITION(ERRORCODE, L"%d")
DX_IS_EQUAL_STRING_FUNCTION_DEFINITION(dxf_const_string_t, L"%ls")
DX_IS_EQUAL_STRING_FUNCTION_DEFINITION(dxf_string_t, L"%ls")
DX_IS_EQUAL_FUNCTION_DEFINITION(dxf_uint_t, L"%u")

bool dx_is_not_null(void* actual) {
    if (actual == NULL) {
        wprintf(L"%ls failed: expected is not NULL, but was NULL\n", __FUNCTIONW__);
        return false;
    }
    return true;
}

bool dx_is_null(void* actual) {
    if (actual != NULL) {
        wprintf(L"%ls failed: expected is NULL, but was not NULL\n", __FUNCTIONW__);
        return false;
    }
    return true;
}

#define DX_IS_GREATER_OR_EQUAL_FUNCTION_DEFINITION(type, tmpl) \
DX_IS_GREATER_OR_EQUAL_FUNCTION_DECLARATION(type) { \
    if (actual < param) { \
        wprintf(L"%ls failed: expected greater or equal to " tmpl L", but was=" tmpl L"\n", __FUNCTIONW__, param, actual); \
        return false; \
                } \
    return true; \
}

DX_IS_GREATER_OR_EQUAL_FUNCTION_DEFINITION(dxf_uint_t, L"%u")