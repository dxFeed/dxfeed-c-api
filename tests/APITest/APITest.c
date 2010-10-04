

#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "Logger.h"
#include <stdio.h>
#include <Windows.h>

const char dxfeed_host[] = "demo.dxfeed.com:7300";

dx_const_string_t dx_event_type_to_string (int event_type) {
	switch (event_type){
	case DXF_ET_TRADE: return L"Trade"; 
	case DXF_ET_QUOTE: return L"Quote"; 
	case DXF_ET_SUMMARY: return L"Summary"; 
	case DXF_ET_PROFILE: return L"Profile"; 
	case DXF_ET_ORDER: return L"Order";
	case DXF_ET_TIME_AND_SALE: return L"Time&Sale";
	default: return L"";
	}	
}

/* -------------------------------------------------------------------------- */

void first_listener (int event_type, dx_const_string_t symbol_name,const dx_event_data_t* data, int data_count) {
	dx_int_t i = 0;

	wprintf(L"First listener. Event: %s Symbol: %s\n",dx_event_type_to_string(event_type), symbol_name);

	if (event_type == DXF_ET_TRADE){
		dxf_trade_t* trade = (dxf_trade_t*)data;

		for (; i < data_count ; ++i) {
			wprintf(/*L"time=%li, exchange code=%C, */L"price=%lf\n"/*, size=%li volume=%li\n"*/, 
			        /*trade[i].time, trade[i].exchange_code, */trade[i].price/*, trade[i].size, trade[i].day_volume*/);
		}
	}	
}

/* -------------------------------------------------------------------------- */

void second_listener (int event_type, dx_const_string_t symbol_name,const dx_event_data_t* data, int data_count) {
	dx_int_t i = 0;

	wprintf(L"Second listener. Event: %s Symbol: %s\n",dx_event_type_to_string(event_type), symbol_name);
}

/* -------------------------------------------------------------------------- */

void process_last_error () {
    int subsystem_id = dx_sc_invalid_subsystem;
    int error_code = DX_INVALID_ERROR_CODE;
    dx_const_string_t error_descr = NULL;
    int res;
    
    res = dxf_get_last_error(&subsystem_id, &error_code, &error_descr);
    
    if (res == DXF_SUCCESS) {
        if (subsystem_id == dx_sc_invalid_subsystem && error_code == DX_INVALID_ERROR_CODE) {
            wprintf(L"WTF - no error information is stored");
            
            return;
        }
        
        wprintf(L"Error occurred and successfully retrieved:\n"
                L"subsystem code = %d, error code = %d, description = \"%s\"\n",
               subsystem_id, error_code, error_descr);
        return;
    }
    
    wprintf(L"An error occurred but the error subsystem failed to initialize\n");
}

/* -------------------------------------------------------------------------- */

int main (int argc, char* argv[]) {
    dxf_connection_t connection;
    dxf_subscription_t subscription;

	dx_string_t symbols_to_add[] = { L"MSFT", L"YHOO", L"C" };
	dx_int_t symbols_to_add_size = sizeof (symbols_to_add) / sizeof (symbols_to_add[0]);

	dx_string_t symbols_to_remove[] = { {L"MSFT"}, {L"YHOO"}, {L"IBM"} };
	dx_int_t symbols_to_remove_size = sizeof (symbols_to_remove) / sizeof (symbols_to_remove[0]);

	dx_string_t symbols_to_set[] = { {L"MSFT"}, {L"YHOO"}};
	dx_int_t symbols_to_set_size = sizeof (symbols_to_set) / sizeof (symbols_to_set[0]);
 
	dx_int_t get_event_types;

	dx_string_t* get_symbols;
	dx_int_t get_symbols_size;


	dxf_initialize_logger( "log.log", true, true, true );
	
	printf("API test started.\n");    
    printf("Connecting to host %s...\n", dxfeed_host);
    
    if (!dxf_create_connection(dxfeed_host, NULL, &connection)) {
        process_last_error();
        return -1;
    }

    printf("Connection successful!\n");
	
	printf("Creating subscription to: Trade\n");
	
	if (!dxf_create_subscription(connection, DXF_ET_TRADE/* | DXF_ET_QUOTE | DXF_ET_ORDER | DXF_ET_SUMMARY | DXF_ET_PROFILE */, &subscription )) {
        process_last_error();
        
        return -1;
    };
	
	printf("Symbol %s added\n", "IBM");
	
	if (!dxf_add_symbol(subscription, L"IBM")) {
        process_last_error();
        
        return -1;
    }
	
	printf("First listener attached\n");
	
	if (!dxf_attach_event_listener(subscription, first_listener)) {
        process_last_error();
        
        return -1;
    }

    printf("Second listener attached\n");
	
	if (!dxf_attach_event_listener(subscription, second_listener)) {
        process_last_error();
        
        return -1;
    }
	
	Sleep (5000); 

	printf("Adding symbols: %S %S %S \n", symbols_to_add[0], symbols_to_add[1], symbols_to_add[2]);
	
	if (!dxf_add_symbols(subscription, symbols_to_add, symbols_to_add_size)) {
        process_last_error();
        
        return -1;
    }
	
	Sleep (5000); 
	
	printf("Removing second listener \n");
	
	if (!dxf_detach_event_listener(subscription, second_listener)) {
        process_last_error();
        
        return -1;
    }
	
	Sleep (5000); 

	printf("Clear symbols\n");
	if (!dxf_subscription_clear_symbols(subscription)) {
        process_last_error();
        
        return -1;
    }; 
	Sleep (5000); 

	printf("Removing symbols: %S %S \n", symbols_to_remove[0], symbols_to_remove[1] );
	if (!dxf_remove_symbols(subscription, symbols_to_remove, symbols_to_remove_size)) {
        process_last_error();
        
        return -1;
    }; 
	Sleep (5000); 



	printf("Set symbols: %S %S \n", symbols_to_set[0], symbols_to_set[1] );
	if (!dxf_set_symbols(subscription, symbols_to_set, symbols_to_set_size)) {
        process_last_error();
        
        return -1;
    }; 
	Sleep (5000);

	printf("Pause subscription\n");
	if (!dxf_remove_subscription(subscription)) {
        process_last_error();
        
        return -1;
    }; 
	Sleep (5000); 

	printf("Subscription events: \n");
	if (!dxf_get_subscription_event_types(subscription, & get_event_types)) {
        process_last_error();
        
        return -1;
	}else{
		dx_event_id_t eid = dx_eid_begin;
			
		for (; eid < dx_eid_count; ++eid) 
			if (get_event_types & DX_EVENT_BIT_MASK(eid)) 
				printf("%S ", dx_event_type_to_string(DX_EVENT_BIT_MASK(eid)));

		printf ("\n");
	}
		



    Sleep(1000000);
    
    printf("Disconnecting from host...\n");
    
    if (!dxf_close_connection(connection)) {
        process_last_error();
        
        return -1;
    }
    
    printf("Disconnect successful!\n"
           "Connection test completed successfully!\n");
           
    return 0;
}

