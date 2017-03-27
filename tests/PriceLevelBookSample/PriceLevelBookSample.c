// PriceLevelBookSample.cpp : Defines the entry point for the console application.
//

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

#define RECORDS_PRINT_LIMIT 7
#define MAX_SOURCE_SIZE 20

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
    time_t tmpint = (time_t)(timestamp / 1000);
    timeinfo = localtime(&tmpint);
    wcsftime(timefmt, 80, L"%Y%m%d-%H%M%S", timeinfo);
    wprintf(L"%ls", timefmt);
}

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

dxf_string_t ansi_to_unicode(const char* ansi_str) {
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

/* -------------------------------------------------------------------------- */

void listener(const dxf_price_level_book_data_ptr_t book_data, void* user_data) {
    size_t i = 0;
    wprintf(L"New Price Level Order Book for %s:\n", book_data->symbol);
    /* Time is 4 + 2 + 2 + 1 + 2 + 2 + 2 = 15 */
    wprintf(L" %-7s %-8s %-15s |  %-7s %-8s %-15s\n", L"Ask", L"Size", L"Time", L"Bid", L"Size", L"Time");
    for (; i < max(book_data->asks_count, book_data->bids_count); i++) {
        if (i < book_data->asks_count) {
            wprintf(L"$%-7.2f %-8lld ", book_data->asks[i].price, book_data->asks[i].size);
            print_timestamp(book_data->asks[i].time);
        } else {
            wprintf(L" %-7s %-8s %-15s", L"", L"", L"");
        }
        wprintf(L" | ");
        if (i < book_data->bids_count) {
            wprintf(L"$%-7.2f %-8lld ", book_data->bids[i].price, book_data->bids[i].size);
            print_timestamp(book_data->bids[i].time);
        }
        wprintf(L"\n");
    }
}

static const char *s_Default_Sources[] = { "BZX", "DEX", NULL };

/* -------------------------------------------------------------------------- */

int main(int argc, char* argv[]) {
    dxf_connection_t connection;
    dxf_price_level_book_t book;
    dxf_string_t base_symbol = NULL;
    dxf_string_t dxfeed_host_u;
    char* dxfeed_host = NULL;
    char **order_sources;
    size_t string_len = 0;

    if (argc < 3) {
        wprintf(L"DXFeed Price Level Book command line sample.\n"
            L"Usage: PriceLevelBookSample <server address> <symbol> [order_source [order_soure [...]]]\n"
            L"  <server address> - a DXFeed server address, e.g. demo.dxfeed.com:7300\n"
            L"  <symbol> - a trade symbol, e.g. C, MSFT, YHOO, IBM\n"
            L"  [order_source] - One or more order sources, e.g.. NTV, BYX, BZX, DEA,\n"
            L"                   ISE, DEX, IST. Default is BZX DEX\n");
        return 0;
    }

    dxf_initialize_logger("price-level-book-api.log", true, true, true);

    dxfeed_host = argv[1];
    base_symbol = ansi_to_unicode(argv[2]);
    if (base_symbol == NULL) {
        return -1;
    }

    if (argc > 3) {
        /* Add NULL at end of list */
        order_sources = calloc(argc - 3 + 1, sizeof(order_sources[0]));
        for (int i = 3; i < argc; i++)
            order_sources[i - 3] = argv[i];
    } else {
        order_sources = s_Default_Sources;
    }

    wprintf(L"PriceLevelBookSample test started.\n");
    dxfeed_host_u = ansi_to_unicode(dxfeed_host);
    wprintf(L"Connecting to host %ls...\n", dxfeed_host_u);
    free(dxfeed_host_u);

#ifdef _WIN32
    InitializeCriticalSection(&listener_thread_guard);
#endif

    if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, &connection)) {
        process_last_error();
        return -1;
    }

    wprintf(L"Connection successful!\n");
    
    if (!dxf_create_price_level_book(connection, base_symbol, order_sources, &book)) {
        process_last_error();
        dxf_close_connection(connection);
        return -1;
    }
    if (!dxf_attach_price_level_book_listener(book, &listener, NULL)) {
        process_last_error();
        dxf_close_connection(connection);
        return -1;
    }

    wprintf(L"Subscription successful!\n");
    if (argc > 3) {
        free(order_sources);
    }

    while (!is_thread_terminate()) {
#ifdef _WIN32
        Sleep(100);
#else
        sleep(1);
#endif
    }

    if (!dxf_close_price_level_book(book)) {
        process_last_error();
        dxf_close_connection(connection);
        return -1;
    }

    wprintf(L"Disconnecting from host...\n");
    if (!dxf_close_connection(connection)) {
        process_last_error();

        return -1;
    }
    wprintf(L"Disconnect successful!\nConnection test completed successfully!\n");

#ifdef _WIN32
    DeleteCriticalSection(&listener_thread_guard);
#endif

    return 0;
}
