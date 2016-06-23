
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
#include <stdio.h>
#include <time.h>

typedef int bool;

#define true 1
#define false 0


/* -------------------------------------------------------------------------- */
#ifdef _WIN32
static bool is_listener_thread_terminated = false;
CRITICAL_SECTION listener_thread_guard;

bool is_thread_terminate() {
    bool res;
    EnterCriticalSection(&listener_thread_guard);
    res = is_listener_thread_terminated;
    LeaveCriticalSection(&listener_thread_guard);

    return res;
}
#else
static volatile bool is_listener_thread_terminated = false;
bool is_thread_terminate() {
    bool res;
    res = is_listener_thread_terminated;
    return res;
}
#endif


/* -------------------------------------------------------------------------- */

#ifdef _WIN32
void on_reader_thread_terminate(dxf_connection_t connection, void* user_data) {
    EnterCriticalSection(&listener_thread_guard);
    is_listener_thread_terminated = true;
    LeaveCriticalSection(&listener_thread_guard);

    wprintf(L"\nTerminating listener thread\n");
}
#else
void on_reader_thread_terminate(dxf_connection_t connection, void* user_data) {
    is_listener_thread_terminated = true;
    wprintf(L"\nTerminating listener thread\n");
}
#endif

void print_timestamp(dxf_long_t timestamp){
		wchar_t timefmt[80];
		
		struct tm * timeinfo;
		time_t tmpint = (time_t)(timestamp /1000);
		timeinfo = localtime ( &tmpint );
		wcsftime(timefmt,80, L"%Y%m%d-%H%M%S" ,timeinfo);
		wprintf(L"%ls",timefmt);
}
/* -------------------------------------------------------------------------- */

void listener(int event_type, dxf_const_string_t symbol_name,
    const dxf_event_data_t* data, int data_count,
    const dxf_event_params_t* event_params, void* user_data) {
    dxf_int_t i = 0;

	//wprintf(L"%ls{symbol=%ls, flags=%d, ",dx_event_type_to_string(event_type), symbol_name, flags);
	
}
/* -------------------------------------------------------------------------- */

void process_last_error () {
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

/* -------------------------------------------------------------------------- */
dxf_string_t ansi_to_unicode (const char* ansi_str) {
#ifdef _WIN32
    size_t len = strlen(ansi_str);
    dxf_string_t wide_str = NULL;

    // get required size
    int wide_size = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, ansi_str, (int)len, wide_str, 0);

    if (wide_size > 0) {
        wide_str = calloc(wide_size + 1, sizeof(dxf_char_t));
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, ansi_str, (int)len, wide_str, wide_size);
    }

    return wide_str;
#else /* _WIN32 */
    dxf_string_t wide_str = NULL;
    size_t wide_size = mbstowcs(NULL, ansi_str, 0);
    if (wide_size > 0) {
        wide_str = calloc(wide_size + 1, sizeof(dxf_char_t));
        mbstowcs(wide_str, ansi_str, wide_size + 1);
    }

    return wide_str; /* todo */
#endif /* _WIN32 */
}

int main (int argc, char* argv[]) {
    dxf_connection_t connection;
    dxf_subscription_t subscription;
    int loop_counter = 100000;
    int event_type = DXF_ET_CANDLE;
    dxf_candle_attributes_t candle_attributes;
    dxf_string_t symbol = L"AAPL";
    char* dxfeed_host = "mddqa.in.devexperts.com:7400";
    dxf_string_t dxfeed_host_u = L"mddqa.in.devexperts.com:7400";
    struct tm time_struct;// = {/*seconds*/ 0, /*minutes*/ 0, /*hours*/0,
                            // /*day*/1, /*month*/0, /*year*/ 116}; 
    time_t time;
    time_struct.tm_sec = 0;
    time_struct.tm_min = 0;
    time_struct.tm_hour = 0;
    time_struct.tm_mday = 1;
    time_struct.tm_mon = 0;
    time_struct.tm_year = 2016 - 1900;
    time = mktime(&time_struct);

    dxf_initialize_logger( "log.log", true, true, true );

    wprintf(L"Sample test started.\n");
    wprintf(L"Connecting to host %ls...\n", dxfeed_host_u);

#ifdef _WIN32
    InitializeCriticalSection(&listener_thread_guard);
#endif

    if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, &connection)) {
        process_last_error();
        return -1;
    }

    wprintf(L"Connection successful!\n");

    if (!dxf_create_subscription_timed(connection, event_type, time, &subscription)) {
        process_last_error();
        return -1;
    };

    if (!dxf_initialize_candle_symbol_attributes(symbol, DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT, DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT, dxf_ctpa_day, dxf_cpa_last, dxf_csa_any, dxf_caa_midnight, &candle_attributes)) {
        process_last_error();
        return -1;
    }

    if (!dxf_add_candle_symbol(subscription, candle_attributes)) {
        process_last_error();
        return -1;
    };

    if (!dxf_attach_event_listener(subscription, listener, NULL)) {
        process_last_error();

        return -1;
    };
    wprintf(L"Subscription successful!\n");

    while (!is_thread_terminate() && loop_counter--) {
#ifdef _WIN32
        Sleep(100);
#else
		sleep(1);
#endif
    }
    
    wprintf(L"Disconnecting from host...\n");

    dxf_delete_candle_symbol_attributes(candle_attributes);

    if (!dxf_close_connection(connection)) {
        process_last_error();

        return -1;
    }

    wprintf(L"Disconnect successful!\nConnection test completed successfully!\n");
    wprintf(L"loops remain:%d\n", loop_counter);

#ifdef _WIN32
    DeleteCriticalSection(&listener_thread_guard);
#endif
    
    return 0;
}
