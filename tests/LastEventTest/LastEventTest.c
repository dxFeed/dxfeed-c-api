

#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "Logger.h"
#include "pthread.h"
#include "DXMemory.h"
#include "DXThreads.h"
#include <stdio.h>
#include <Windows.h>
#include <math.h>

const char dxfeed_host[] = "demo.dxfeed.com:7300";

static pthread_mutex_t g_event_data_guard;
//static dx_const_string_t g_symbol = L"IBM";
static dx_const_string_t g_symbols[] = { {L"IBM"}, {L"MSFT"}, {L"YHOO"}, {L"C"} };
static const dx_int_t g_symbols_size = sizeof (g_symbols) / sizeof (g_symbols[0]);
static const int g_event_type = DX_ET_TRADE;
static int g_iteration_count = 10;

static dx_event_data_t g_event_data[dx_eid_count];
static const int g_event_data_sizes[dx_eid_count] = {sizeof(dxf_trade_t), sizeof(dxf_quote_t), sizeof(dxf_fundamental_t),
                                                     sizeof(dxf_profile_t), sizeof(dxf_market_maker)};

static bool mutex_create (pthread_mutex_t* mutex) {
    int res = pthread_mutex_init(mutex, NULL);

    dx_logging_verbose_info(L"Create mutex %#010x", mutex);

    switch (res) {
    case EAGAIN:
        return false;
        //dx_set_last_error(dx_sc_threads, dx_tec_not_enough_sys_resources); break;
    case ENOMEM:
        return false;
        //dx_set_last_error(dx_sc_threads, dx_tec_not_enough_memory); break;
    case EPERM:
        return false;
        //dx_set_last_error(dx_sc_threads, dx_tec_permission_denied); break;
    case EBUSY:
        return false;
        //dx_set_last_error(dx_sc_threads, dx_tec_resource_busy); break;
    case EINVAL:
        return false;
        //dx_set_last_error(dx_sc_threads, dx_tec_invalid_resource_id); break;
    default:
        return false;
        //dx_set_last_error(dx_sc_threads, dx_tec_generic_error); break;
    case 0:
        return true;
    }

    return false;
}

/* -------------------------------------------------------------------------- */

static bool mutex_lock (pthread_mutex_t* mutex) {
    int res = pthread_mutex_lock(mutex);

    dx_logging_verbose_info(L"Lock mutex %#010x", mutex);

    switch (res) {
    case EINVAL:
        return false;
        //dx_set_last_error(dx_sc_threads, dx_tec_invalid_resource_id); break;
    case EAGAIN:
        return false;
        //dx_set_last_error(dx_sc_threads, dx_tec_invalid_res_operation); break;
    case EDEADLK:
        return false;
        //dx_set_last_error(dx_sc_threads, dx_tec_deadlock_detected); break;
    default:
        return false;
        //dx_set_last_error(dx_sc_threads, dx_tec_generic_error); break;
    case 0:
        return true;
    }

    return false;
}

/* -------------------------------------------------------------------------- */

static bool mutex_unlock (pthread_mutex_t* mutex) {

    int res = pthread_mutex_unlock(mutex);

    dx_logging_verbose_info(L"Unlock mutex %#010x", mutex);

    switch (res) {
    case EINVAL:
        return false;
        //dx_set_last_error(dx_sc_threads, dx_tec_invalid_resource_id); break;
    case EAGAIN:
    case EPERM:
        return false;
        //dx_set_last_error(dx_sc_threads, dx_tec_invalid_res_operation); break;
    default:
        return false;
        //dx_set_last_error(dx_sc_threads, dx_tec_generic_error); break;
    case 0:
        return true;
    }

    return false;
}

/* -------------------------------------------------------------------------- */

dx_const_string_t dx_event_type_to_string(int event_type){
    switch (event_type){
        case DX_ET_TRADE: return L"Trade"; 
        case DX_ET_QUOTE: return L"Quote"; 
        case DX_ET_FUNDAMENTAL: return L"Fundamental"; 
        case DX_ET_PROFILE: return L"Profile"; 
        case DX_ET_MARKET_MAKER: return L"MarketMaker"; 
        default: return L"";
    }	
}

bool compare_event_data(int event_type, const dx_event_data_t data1, const dx_event_data_t data2) {
    //if (!mutex_lock(&g_event_data_guard)) {
    //    return false;
    //}

    if ( !data1 || !data2) {
        //mutex_unlock(&g_event_data_guard);
        return false;
    }

    if (event_type == DX_ET_TRADE) {
        dxf_trade_t* trade1 = (dxf_trade_t*)data1;
        dxf_trade_t* trade2 = (dxf_trade_t*)data2;
        //bool res = fabs(trade1->last_change - trade2->last_change) < 0.00001
        //           && trade1->last_exchange == trade2->last_exchange
        //           && fabs(trade1->last_price - trade2->last_price) < 0.00001
        //           && trade1->last_size == trade2->last_size
        //           && trade1->last_tick == trade2->last_tick
        //           && trade1->last_time == trade2->last_time
        //           && fabs(trade1->volume - trade2->volume) < 0.00001;
        bool res = fabs(trade1->last_change - trade2->last_change) < 0.00001;
        res  = res && trade1->last_exchange == trade2->last_exchange;
        res  = res && fabs(trade1->last_price - trade2->last_price) < 0.00001;
        res  = res && trade1->last_size == trade2->last_size;
        res  = res && trade1->last_tick == trade2->last_tick;
        res  = res && trade1->last_time == trade2->last_time;
        res  = res && fabs(trade1->volume - trade2->volume) < 0.00001;

        //if (!mutex_unlock(&g_event_data_guard)) {
        //    wprintf(L"Error in mutex_unlock");
        //    return false;
        //}

        return res;
    }

    //mutex_unlock(&g_event_data_guard);
    return false;
}

/* -------------------------------------------------------------------------- */

void process_last_error () {
    int subsystem_id = dx_sc_invalid_subsystem;
    int error_code = DX_INVALID_ERROR_CODE;
    const char* error_descr = NULL;
    int res;

    res = dxf_get_last_error(&subsystem_id, &error_code, &error_descr);

    if (res == DXF_SUCCESS) {
        if (subsystem_id == dx_sc_invalid_subsystem && error_code == DX_INVALID_ERROR_CODE) {
            printf("WTF - no error information is stored");

            return;
        }

        printf("Error occurred and successfully retrieved:\n"
            "subsystem code = %d, error code = %d, description = \"%s\"\n",
            subsystem_id, error_code, error_descr);
        return;
    }

    printf("An error occurred but the error subsystem failed to initialize\n");
}

/* -------------------------------------------------------------------------- */

void listener(int event_type, dx_const_string_t symbol_name,const dx_event_data_t* data, int data_count){
    dx_int_t i = 0;
    //dx_event_data_t event_data;

    wprintf(L"First listener. Event: %s Symbol: %s\n",dx_event_type_to_string(event_type), symbol_name);

    if (event_type == DX_ET_TRADE){
        dxf_trade_t* trade = (dxf_trade_t*)data;

        for ( ; i < data_count ; ++i )
            wprintf(L"Last.Exchange=%C Last.Time=%i  Last.Price=%f Last.Size=%i Last.Tick=%i Last.Change=%f Volume=%f\n" , 
            trade[i].last_exchange,trade[i].last_time,trade[i].last_price,trade[i].last_size,trade[i].last_tick,trade[i].last_change,trade[i].volume);
    }

    // copy event data
    if (!mutex_lock(&g_event_data_guard)) {
        wprintf(L"Error in mutex_lock");
        return;
    }

    memcpy(g_event_data[event_type], data, g_event_data_sizes[event_type]);

    if (!mutex_unlock(&g_event_data_guard)) {
        wprintf(L"Error in mutex_unlock");
        return;
    }
}


/* -------------------------------------------------------------------------- */

int main (int argc, char* argv[]) {
    dxf_subscription_t subscription;
    int i = 0;
    bool test_result = true;
    //g_event_data = dx_calloc(dx_eid_count, sizeof(dx_event_data_t));
    if (g_event_data == NULL) {
        process_last_error();
        return -1;
    }

    for(; i < dx_eid_count; ++i) {
        g_event_data[i] = calloc(1, g_event_data_sizes[i]);
        if (g_event_data[i] == NULL) {
            process_last_error();
            return -1;
        }
    }

    dx_logger_initialize( "log.log", true, true, true );

    if (!mutex_create(&g_event_data_guard)) {
        wprintf(L"Error in mutex_create");
        return -1;
    }

    wprintf(L"LastEvent test started.\n");    
    printf("Connecting to host %s...\n", dxfeed_host);

    if (!dxf_connect_feed(dxfeed_host)) {
        process_last_error();
        return -1;
    }

    wprintf(L"Connection successful!\n");

    wprintf(L"Creating subscription to: Trade\n");
    if (!dxf_create_subscription(g_event_type /*| DX_ET_QUOTE | DX_ET_MARKET_MAKER | DX_ET_FUNDAMENTAL | DX_ET_PROFILE*/, &subscription )) {
        process_last_error();

        return -1;
    };

    //wprintf(L"Symbol %s added\n", g_symbol);
    //if (!dxf_add_symbol(subscription, g_symbol)) {
    //    process_last_error();

    //    return -1;
    //}; 

    wprintf(L"Adding symbols:");
    for (i = 0; i < g_symbols_size; ++i) {
        wprintf(L" %S", g_symbols[i]);
    }
    wprintf(L"\n");

    if (!dxf_add_symbols(subscription, g_symbols, g_symbols_size)) {
        process_last_error();

        return -1;
    }; 

    wprintf(L"Listener attached\n");
    if (!dxf_attach_event_listener(subscription, listener)) {
        process_last_error();

        return -1;
    };

    //Sleep (5000); 


    //Sleep (5000); 

    while (g_iteration_count--) {
        // todo: deadlock here
        if (!mutex_lock(&g_event_data_guard)) {
            wprintf(L"Error in mutex_lock");
            return -1;
        }
        for (i = 0; i < g_symbols_size; ++i) {
            dx_event_data_t data;
            bool compare_result;
            if (!dxf_get_last_event(g_event_type, g_symbols[i], &data)) {
                mutex_unlock(&g_event_data_guard);
                return -1;
            }

            compare_result = compare_event_data(g_event_type, data, g_event_data[g_event_type]);
            wprintf(L"\n Compare result : %s \n", compare_result ? L"true" : L"false");
            
            test_result = test_result && compare_result;
        }

        if (!mutex_unlock(&g_event_data_guard)) {
            wprintf(L"Error in mutex_unlock");
            return -1;
        }
        Sleep (5000); 
    }

    //Sleep(1000000);

    wprintf(L"\n Test %s \n", test_result ? L"complete successfully" : L"failed");

    wprintf(L"Disconnecting from host...\n");

    if (!dxf_disconnect_feed()) {
        process_last_error();

        return -1;
    }

    wprintf(L"Disconnect successful!\n"
        L"Connection test completed successfully!\n");

    Sleep(5000);

    return 0;
}

