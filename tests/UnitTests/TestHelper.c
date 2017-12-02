#include <math.h>
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

bool create_event_subscription(dxf_connection_t connection, int event_type,
							dxf_const_string_t symbol,
							dxf_event_listener_t event_listener,
							OUT dxf_subscription_t* res_subscription) {
	dxf_subscription_t subscription = NULL;

	if (!dxf_create_subscription(connection, event_type, &subscription)) {
		process_last_error();
		return false;
	};

	if (!dxf_add_symbol(subscription, symbol)) {
		process_last_error();
		dxf_close_subscription(subscription);
		return false;
	};

	if (!dxf_attach_event_listener(subscription, event_listener, NULL)) {
		process_last_error();
		dxf_close_subscription(subscription);
		return false;
	};

	*res_subscription = subscription;
	return true;
}

/* -------------------------------------------------------------------------- */

/* Event counter functions */

void init_event_counter(event_counter_data_ptr_t counter_data) {
	InitializeCriticalSection(&counter_data->event_counter_guard);
	counter_data->event_counter = 0;
	counter_data->counter_name = NULL;
}

void init_event_counter2(event_counter_data_ptr_t counter_data, const char *name) {
	InitializeCriticalSection(&counter_data->event_counter_guard);
	counter_data->event_counter = 0;
	counter_data->counter_name = name;
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

const char *get_event_counter_name(event_counter_data_ptr_t counter_data) {
	const char * value = 0;
	EnterCriticalSection(&counter_data->event_counter_guard);
	value = counter_data->counter_name;
	LeaveCriticalSection(&counter_data->event_counter_guard);
	return value;
}

/* -------------------------------------------------------------------------- */

#define DX_TOLLERANCE 0.000001

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

#define DX_IS_EQUAL_ANSI_FUNCTION_DEFINITION(type, tmpl, alias) \
DX_IS_EQUAL_FUNCTION_DECLARATION_A(type, alias) { \
	if (expected == NULL && actual == NULL) { \
		return true; \
	} else if (expected == NULL) { \
		printf("%s failed: expected=NULL, but was=" tmpl "\n", __FUNCTION__, actual); \
		return false; \
	} else if (actual == NULL) { \
		printf("%s failed: expected=" tmpl ", but was=NULL\n", __FUNCTION__, expected); \
		return false; \
	} \
	if (strcmp(expected, actual) != 0) { \
		printf("%s failed: expected=" tmpl ", but was=" tmpl "\n", __FUNCTION__, expected, actual); \
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
DX_IS_EQUAL_FUNCTION_DEFINITION(dxf_long_t, L"%lld")
DX_IS_EQUAL_FUNCTION_DEFINITION(dxf_ulong_t, L"%llu")

DX_IS_EQUAL_ANSI_FUNCTION_DEFINITION(char*, "%s", ansi)
DX_IS_EQUAL_ANSI_FUNCTION_DEFINITION(const char*, "%s", const_ansi)

DX_IS_EQUAL_FUNCTION_DECLARATION(double) {
	double abs_expected = fabs(expected);
	double max = fabs(actual);
	double rel_dif;
	max = abs_expected > max ? abs_expected : max;
	rel_dif = (max == 0.0 ? 0.0 : fabs(expected - actual) / max);
	if (rel_dif > DX_TOLLERANCE) {
		wprintf(L"%ls failed: expected=%f, but was=%f\n", __FUNCTIONW__, expected, actual);
		return false;
	}
	return true;
}

DX_IS_EQUAL_FUNCTION_DECLARATION(size_t) {
	return dx_is_equal_dxf_ulong_t((dxf_ulong_t)expected, actual);
}

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

bool dx_is_true(bool actual) {
	if (actual != true) {
		wprintf(L"%ls failed: expected 'true', but was 'false'\n", __FUNCTIONW__);
		return false;
	}
	return true;
}

bool dx_is_false(bool actual) {
	if (actual != false) {
		wprintf(L"%ls failed: expected 'false', but was 'true'\n", __FUNCTIONW__);
		return false;
	}
	return true;
}

bool dx_is_equal_ptr(void* expected, void* actual)
{
	if (expected != actual) {
		wprintf(L"%ls failed: expected=%p, but was=%p\n", __FUNCTIONW__, expected, actual);
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
DX_IS_GREATER_OR_EQUAL_FUNCTION_DEFINITION(dxf_long_t, L"%lld")
DX_IS_GREATER_OR_EQUAL_FUNCTION_DEFINITION(double, L"%f")
