#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "Logger.h"
#include <stdio.h>
#include <Windows.h>

// To fix problem with MS implementation of swprintf
#define swprintf _snwprintf

static char dxfeed_host_default[] = "demo.dxfeed.com:7300";
//const char dxfeed_host[] = "localhost:5678";
HANDLE g_out_console;

dxf_const_string_t dx_event_type_to_string(int event_type) {
    switch (event_type) {
        case DXF_ET_TRADE: return L"Trade";
        case DXF_ET_QUOTE: return L"Quote";
        case DXF_ET_SUMMARY: return L"Summary";
        case DXF_ET_PROFILE: return L"Profile";
        case DXF_ET_ORDER: return L"Order";
        case DXF_ET_TIME_AND_SALE: return L"Time&Sale";
        case DXF_ET_CANDLE: return L"Candle";
        default: return L"";
    }
}

#define SYMBOLS_COUNT 4
static const dxf_const_string_t g_symbols[] = { {L"IBM"}, {L"MSFT"}, {L"YHOO"}, {L"C"} };
#define MAIN_LOOP_SLEEP_MILLIS 100

/* -------------------------------------------------------------------------- */
void trade_listener(int event_type, dxf_const_string_t symbol_name,
                    const dxf_event_data_t* data, int data_count, void* user_data);
void quote_listener(int event_type, dxf_const_string_t symbol_name,
                    const dxf_event_data_t* data, int data_count, void* user_data);
void summary_listener(int event_type, dxf_const_string_t symbol_name,
                      const dxf_event_data_t* data, int data_count, void* user_data);
void profile_listener(int event_type, dxf_const_string_t symbol_name,
                      const dxf_event_data_t* data, int data_count, void* user_data);
void order_listener(int event_type, dxf_const_string_t symbol_name,
                    const dxf_event_data_t* data, int data_count, void* user_data);
void time_and_sale_listener(int event_type, dxf_const_string_t symbol_name,
                            const dxf_event_data_t* data, int data_count, void* user_data);
void candle_listener(int event_type, dxf_const_string_t symbol_name,
                     const dxf_event_data_t* data, int data_count, void* user_data);
void snapshot_listener(const dxf_snapshot_data_ptr_t snapshot_data, void* user_data);

struct event_info_t {
    dx_event_id_t       id;
    int                 event_type;
    COORD               coord;
    dxf_event_listener_t listener;
    unsigned int        total_data_count[SYMBOLS_COUNT];
};

static struct event_info_t event_info[dx_eid_count] = {
    { dx_eid_trade, DXF_ET_TRADE, { 0, 0 }, trade_listener, 0 },
    { dx_eid_quote, DXF_ET_QUOTE, { 0, 6 }, quote_listener, 0 },
    { dx_eid_summary, DXF_ET_SUMMARY, { 0, 12 }, summary_listener, 0 },
    { dx_eid_profile, DXF_ET_PROFILE, { 0, 18 }, profile_listener, 0 },
    { dx_eid_order, DXF_ET_ORDER, { 0, 24 }, order_listener, 0 },
    { dx_eid_time_and_sale, DXF_ET_TIME_AND_SALE, { 0, 30 }, time_and_sale_listener, 0 },
    { dx_eid_candle, DXF_ET_CANDLE,{ 0, 36 }, candle_listener, 0 }
};

struct snapshot_info_t {
    dx_event_id_t           id;
    int                     event_type;
    const char              source[100];
    COORD                   coord;
    dxf_snapshot_listener_t listener;
    unsigned int            total_data_count[SYMBOLS_COUNT];
    dxf_snapshot_t          obj[SYMBOLS_COUNT];
};

#define SNAPSHOT_COUNT 1
static struct snapshot_info_t snapshot_info[SNAPSHOT_COUNT] = {
    { dx_eid_order, DXF_ET_ORDER, "NTV", {0, 42}, snapshot_listener, 0 }
};

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

void print_at(COORD c, wchar_t* str) {
    DWORD dummy;
    SetConsoleCursorPosition(g_out_console, c);
    WriteConsoleW(g_out_console, str, (DWORD)wcslen(str), &dummy, NULL);

}

void on_reader_thread_terminate (const char* host, void* user_data) {
    EnterCriticalSection(&listener_thread_guard);
    is_listener_thread_terminated = true;
    LeaveCriticalSection(&listener_thread_guard);

    printf("\nTerminating listener thread, host: %s\n", host);
}

/* -------------------------------------------------------------------------- */

int get_symbol_index(dxf_const_string_t symbol_name) {
    int i = 0;
    for (; i < SYMBOLS_COUNT; ++i) {
        if (wcsicmp(symbol_name, g_symbols[i]) == 0) {
            return i;
        }
    }

    return -1;
}

/* -------------------------------------------------------------------------- */

void trade_listener(int event_type, dxf_const_string_t symbol_name,
                    const dxf_event_data_t* data, int data_count, void* user_data) {
    dxf_int_t i = 0;
    wchar_t str[200];
    int ind;
    COORD coord = event_info[dx_eid_trade].coord;

    if (event_type != DXF_ET_TRADE) {
        swprintf(str, 200, L"Error: event: %s Symbol: %s, expected event: Trade\n", dx_event_type_to_string(event_type), symbol_name);
        print_at( coord, str);
        return;
    }

    print_at( coord, L"Event Trade:                           ");
    ind = get_symbol_index(symbol_name);
    if (ind == -1) {
        return;
    }

    event_info[dx_eid_trade].total_data_count[ind] += data_count;

    coord.X += 5;
    coord.Y += ind + 1;
    swprintf(str, 200, L"Symbol: \"%s\" Data count: %d, Total data count: %d            ", symbol_name, data_count, event_info[dx_eid_trade].total_data_count[ind]);
    print_at( coord, str);
    //swprintf(str, sizeof(str), L"Event: %s Symbol: %s Data count: %d, Total data count: %d            ",dx_event_type_to_string(event_type), symbol_name, data_count, event_info[dx_eid_trade].total_data_count);
    //print_at( event_info[dx_eid_trade].coord, str);
}

/* -------------------------------------------------------------------------- */

void quote_listener(int event_type, dxf_const_string_t symbol_name,
                    const dxf_event_data_t* data, int data_count, void* user_data) {
    dxf_int_t i = 0;
    wchar_t str[200];
    int ind;
    COORD coord = event_info[dx_eid_quote].coord;

    if (event_type != DXF_ET_QUOTE) {
        swprintf(str, sizeof(str), L"Error: event: %s Symbol: %s, expected event: Quote\n",dx_event_type_to_string(event_type), symbol_name);
        print_at( coord, str);
        return;
    }

    print_at( coord, L"Event Quote:                           ");
    ind = get_symbol_index(symbol_name);
    if (ind == -1) {
        return;
    }

    event_info[dx_eid_quote].total_data_count[ind] += data_count;

    coord.X += 5;
    coord.Y += ind + 1;
    swprintf(str, sizeof(str), L"Symbol: \"%s\" Data count: %d, Total data count: %d            ", symbol_name, data_count, event_info[dx_eid_quote].total_data_count[ind]);
    print_at( coord, str);

    //event_info[dx_eid_quote].total_data_count += data_count;
    //swprintf(str, sizeof(str), L"Event: %s Symbol: %s Data count: %d, Total data count: %d            ",dx_event_type_to_string(event_type), symbol_name, data_count, event_info[dx_eid_quote].total_data_count);
    //print_at( event_info[dx_eid_quote].coord, str);
}

/* -------------------------------------------------------------------------- */

void summary_listener(int event_type, dxf_const_string_t symbol_name,
                      const dxf_event_data_t* data, int data_count, void* user_data) {
    dxf_int_t i = 0;
    wchar_t str[200];
    int ind;
    COORD coord = event_info[dx_eid_summary].coord;

    if (event_type != DXF_ET_SUMMARY) {
        swprintf(str, sizeof(str), L"Error: event: %s Symbol: %s, expected event: Summary\n",dx_event_type_to_string(event_type), symbol_name);
        print_at( coord, str);
        return;
    }

    print_at( coord, L"Event Summary:                           ");
    ind = get_symbol_index(symbol_name);
    if (ind == -1) {
        return;
    }

    event_info[dx_eid_summary].total_data_count[ind] += data_count;

    coord.X += 5;
    coord.Y += ind + 1;
    swprintf(str, sizeof(str), L"Symbol: \"%s\" Data count: %d, Total data count: %d            ", symbol_name, data_count, event_info[dx_eid_summary].total_data_count[ind]);
    print_at( coord, str);
    //event_info[dx_eid_summary].total_data_count += data_count;
    //swprintf(str, sizeof(str), L"Event: %s Symbol: %s Data count: %d, Total data count: %d            ",dx_event_type_to_string(event_type), symbol_name, data_count, event_info[dx_eid_summary].total_data_count);
    //print_at( event_info[dx_eid_summary].coord, str);
}

/* -------------------------------------------------------------------------- */

void profile_listener(int event_type, dxf_const_string_t symbol_name,
                      const dxf_event_data_t* data, int data_count, void* user_data) {
    dxf_int_t i = 0;
    wchar_t str[200];
    int ind;
    COORD coord = event_info[dx_eid_profile].coord;

    if (event_type != DXF_ET_PROFILE) {
        swprintf(str, sizeof(str), L"Error: event: %s Symbol: %s, expected event: Profile\n",dx_event_type_to_string(event_type), symbol_name);
        print_at( coord, str);
        return;
    }

    print_at( coord, L"Event Profile:                           ");
    ind = get_symbol_index(symbol_name);
    if (ind == -1) {
        return;
    }

    event_info[dx_eid_profile].total_data_count[ind] += data_count;

    coord.X += 5;
    coord.Y += ind + 1;
    swprintf(str, sizeof(str), L"Symbol: \"%s\" Data count: %d, Total data count: %d            ", symbol_name, data_count, event_info[dx_eid_profile].total_data_count[ind]);
    print_at( coord, str);
    //event_info[dx_eid_profile].total_data_count += data_count;
    //swprintf(str, sizeof(str), L"Event: %s Symbol: %s Data count: %d, Total data count: %d            ",dx_event_type_to_string(event_type), symbol_name, data_count, event_info[dx_eid_profile].total_data_count);
    //print_at( event_info[dx_eid_profile].coord, str);
}

/* -------------------------------------------------------------------------- */

void order_listener(int event_type, dxf_const_string_t symbol_name,
                    const dxf_event_data_t* data, int data_count, void* user_data) {
    dxf_int_t i = 0;
    wchar_t str[200];
    int ind;
    COORD coord = event_info[dx_eid_order].coord;

    if (event_type != DXF_ET_ORDER) {
        swprintf(str, sizeof(str), L"Error: event: %s Symbol: %s, expected event: Order\n",dx_event_type_to_string(event_type), symbol_name);
        print_at( coord, str);
        return;
    }

    print_at( coord, L"Event Order:                           ");
    ind = get_symbol_index(symbol_name);
    if (ind == -1) {
        return;
    }

    event_info[dx_eid_order].total_data_count[ind] += data_count;

    coord.X += 5;
    coord.Y += ind + 1;
    swprintf(str, sizeof(str), L"Symbol: \"%s\" Data count: %d, Total data count: %d            ", symbol_name, data_count, event_info[dx_eid_order].total_data_count[ind]);
    print_at( coord, str);
    //event_info[dx_eid_order].total_data_count += data_count;
    //swprintf(str, sizeof(str), L"Event: %s Symbol: %s Data count: %d, Total data count: %d            ",dx_event_type_to_string(event_type), symbol_name, data_count, event_info[dx_eid_order].total_data_count);
    //print_at( event_info[dx_eid_order].coord, str);
}

/* -------------------------------------------------------------------------- */

void time_and_sale_listener(int event_type, dxf_const_string_t symbol_name,
                            const dxf_event_data_t* data, int data_count, void* user_data) {
    dxf_int_t i = 0;
    wchar_t str[200];
    int ind;
    COORD coord = event_info[dx_eid_time_and_sale].coord;

    if (event_type != DXF_ET_TIME_AND_SALE) {
        swprintf(str, sizeof(str), L"Error: event: %s Symbol: %s, expected event: Time&Sale\n",dx_event_type_to_string(event_type), symbol_name);
        print_at( coord, str);
        return;
    }

    print_at( coord, L"Event Time&Sale:                           ");
    ind = get_symbol_index(symbol_name);
    if (ind == -1) {
        return;
    }

    event_info[dx_eid_time_and_sale].total_data_count[ind] += data_count;

    coord.X += 5;
    coord.Y += ind + 1;
    swprintf(str, sizeof(str), L"Symbol: \"%s\" Data count: %d, Total data count: %d            ", symbol_name, data_count, event_info[dx_eid_time_and_sale].total_data_count[ind]);
    print_at( coord, str);
    //event_info[dx_eid_time_and_sale].total_data_count += data_count;
    //swprintf(str, sizeof(str), L"Event: %s Symbol: %s Data count: %d, Total data count: %d            ",dx_event_type_to_string(event_type), symbol_name, data_count, event_info[dx_eid_time_and_sale].total_data_count);
    //print_at( event_info[dx_eid_time_and_sale].coord, str);
}

void candle_listener(int event_type, dxf_const_string_t symbol_name,
                     const dxf_event_data_t* data, int data_count, void* user_data) {
    dxf_int_t i = 0;
    wchar_t str[200];
    int ind;
    COORD coord = event_info[dx_eid_candle].coord;

    if (event_type != DXF_ET_CANDLE) {
        swprintf(str, sizeof(str), L"Error: event: %s Symbol: %s, expected event: Candle\n", dx_event_type_to_string(event_type), symbol_name);
        print_at(coord, str);
        return;
    }

    print_at(coord, L"Event Candle:                              ");
    ind = get_symbol_index(symbol_name);
    if (ind == -1) {
        return;
    }

    event_info[dx_eid_candle].total_data_count[ind] += data_count;

    coord.X += 5;
    coord.Y += ind + 1;
    swprintf(str, sizeof(str), L"Symbol: \"%s\" Data count: %d, Total data count: %d            ", symbol_name, data_count, event_info[dx_eid_candle].total_data_count[ind]);
    print_at(coord, str);
}

void snapshot_listener(const dxf_snapshot_data_ptr_t snapshot_data, void* user_data) {
    dxf_int_t i = 0;
    wchar_t str[200];
    int ind;
    struct snapshot_info_t *info = user_data;
    COORD coord = info->coord;

    if (snapshot_data->event_type != info->event_type) {
        swprintf(str, sizeof(str), L"Error: event: %s Symbol: %s, expected event: %s\n", 
                 dx_event_type_to_string(snapshot_data->event_type), snapshot_data->symbol, 
                 dx_event_type_to_string(info->event_type));
        print_at(coord, str);
        return;
    }

    swprintf(str, sizeof(str), L"Snapshot %s#%S:                           ", 
             dx_event_type_to_string(info->event_type), info->source);
    print_at(coord, str);
    ind = get_symbol_index(snapshot_data->symbol);
    if (ind == -1) {
        return;
    }

    info->total_data_count[ind] += snapshot_data->records_count;

    coord.X += 5;
    coord.Y += ind + 1;
    swprintf(str, sizeof(str), L"Symbol: \"%s\" Data count: %d, Total data count: %d            ", 
             snapshot_data->symbol, snapshot_data->records_count, info->total_data_count[ind]);
    print_at(coord, str);
}

/* -------------------------------------------------------------------------- */

void process_last_error () {
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

dxf_subscription_t create_subscription(dxf_connection_t connection, int event_id) {
    dxf_subscription_t subscription;
    wchar_t str[100];
    const int event_type = event_info[event_id].event_type;
    int i;
    COORD coord = event_info[event_id].coord;

    swprintf(str, sizeof(str), L"There is no data for event: %s", dx_event_type_to_string(event_type));
    print_at( coord, str);

    if (!dxf_create_subscription(connection, event_type, &subscription)) {
        process_last_error();

        return NULL;
    };

    for (i = 0; i < SYMBOLS_COUNT; ++i) {
        int ind;
        COORD symbol_coord = coord;
        if (!dxf_add_symbol(subscription, g_symbols[i])) {
            process_last_error();

            return NULL;
        };

        swprintf(str, sizeof(str), L"Symbol %s: no data", g_symbols[i]);
        ind = get_symbol_index(g_symbols[i]);
        if (ind == -1) {
            return NULL;
        }
        symbol_coord.X += 5;
        symbol_coord.Y += ind + 1;
        print_at(symbol_coord, str);
    }

    if (!dxf_attach_event_listener(subscription, event_info[event_id].listener, NULL)) {
        process_last_error();

        return NULL;
    };

    return subscription;
}

void* create_snapshot_subscription(dxf_connection_t connection, struct snapshot_info_t *info) {
    wchar_t str[100];
    int i;
    COORD coord = info->coord;

    swprintf(str, sizeof(str), L"There is no data for snapshot event: %s", dx_event_type_to_string(info->event_type));
    print_at(coord, str);

    for (i = 0; i < SYMBOLS_COUNT; ++i) {
        int ind;
        COORD symbol_coord = coord;
        dxf_candle_attributes_t candle_attributes = NULL;

        if (info->id == dx_eid_candle) {
            if (!dxf_create_candle_symbol_attributes(g_symbols[i],
                DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT,
                DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT,
                dxf_ctpa_day, dxf_cpa_default, dxf_csa_default,
                dxf_caa_default, &candle_attributes)) {

                process_last_error();
                return NULL;
            }

            if (!dxf_create_candle_snapshot(connection, candle_attributes, 0, &(info->obj[i]))) {
                process_last_error();
                dxf_delete_candle_symbol_attributes(candle_attributes);
                return NULL;
            };
        } else if (info->id == dx_eid_order) {
            if (!dxf_create_order_snapshot(connection, g_symbols[i], info->source, 0, &(info->obj[i]))) {
                process_last_error();
                return NULL;
            };
        } else {
            return NULL;
        }

        swprintf(str, sizeof(str), L"Symbol %s: no data", g_symbols[i]);
        ind = get_symbol_index(g_symbols[i]);
        if (ind == -1) {
            return NULL;
        }
        symbol_coord.X += 5;
        symbol_coord.Y += ind + 1;
        print_at(symbol_coord, str);

        if (!dxf_attach_snapshot_listener(info->obj[i], info->listener, info)) {
            process_last_error();

            return NULL;
        };
    }

    return info;
}
/* -------------------------------------------------------------------------- */

bool initialize_console() {
    CONSOLE_SCREEN_BUFFER_INFO info;
    COORD c = {80, 40};
    SMALL_RECT rect = {0, 0, 79, 48};
    DWORD buffer_size, dummy;

    g_out_console = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_out_console == NULL) {
        return false;
    }

    if (!GetConsoleScreenBufferInfo(g_out_console, &info)) {
        DWORD err = GetLastError();
        return false;
    }

    if (info.dwSize.X < 80 || info.dwSize.Y < 40) {
        if (!SetConsoleScreenBufferSize(g_out_console, c)) {
            return false;
        }
    }

    if (!SetConsoleWindowInfo(g_out_console, TRUE, &rect)) {
        return false;
    }

    buffer_size = info.dwSize.X * info.dwSize.Y;

    c.X = 0;
    c.Y = 0;
    if (!FillConsoleOutputCharacter(g_out_console, (TCHAR)' ', buffer_size, c, &dummy)) {
        return false;
    }

    ///* get the current text attribute */
    //bSuccess = GetConsoleScreenBufferInfo( hConsole, &csbi );

    ///* now set the buffer's attributes accordingly */

    //bSuccess = FillConsoleOutputAttribute( hConsole, csbi.wAttributes,
    //    dwConSize, coordScreen, &cCharsWritten );

    // put the cursor at (0, 0)
    if (!SetConsoleCursorPosition(g_out_console, c)) {
        return false;
    }

    return true;
}

int main (int argc, char* argv[]) {
    dxf_connection_t connection;
    dxf_subscription_t subscriptions[dx_eid_count];
    int loop_counter = 10000;

    int i;
    char* dxfeed_host = NULL;
    
    if (argc < 2) {
        dxfeed_host = dxfeed_host_default;
    } else {
        dxfeed_host = argv[1];
    }

    if (argc >= 3) {
        //expect the second param in seconds
        //convert seconds time interval to loop number
        loop_counter = atoi(argv[2]) * 1000 / MAIN_LOOP_SLEEP_MILLIS;
    }

    dxf_initialize_logger( "log.log", true, true, true );

    if (!initialize_console()) {
        return -1;
    }

    if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, &connection)) {
        process_last_error();
        return -1;
    }

    // create subscriptions
    for (i = dx_eid_begin; i < dx_eid_count; ++i) {
        subscriptions[i] = create_subscription(connection, i);
        if (subscriptions[i] == NULL) {
            return -1;
        }
    }
    //create snapshots
    for (i = 0; i < SNAPSHOT_COUNT; ++i) {
        if (create_snapshot_subscription(connection, &(snapshot_info[i])) == NULL)
            return -1;
    }
    // main loop
    InitializeCriticalSection(&listener_thread_guard);
    while (!is_thread_terminate() && loop_counter--) {
        Sleep(MAIN_LOOP_SLEEP_MILLIS);
    }
    DeleteCriticalSection(&listener_thread_guard);

    printf("Disconnecting from host...\n");

    if (!dxf_close_connection(connection)) {
        process_last_error();

        return -1;
    }

    printf("Disconnect successful!\n"
        "Connection test completed successfully!\n");

    return 0;
}

