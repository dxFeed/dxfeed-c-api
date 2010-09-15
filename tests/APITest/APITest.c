

#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "Logger.h"
#include <stdio.h>
#include <Windows.h>

const char dxfeed_host[] = "demo.dxfeed.com:7300";
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
void first_listener(int event_type, dx_const_string_t symbol_name,const dx_event_data_t* data, int data_count){
		dx_int_t i = 0;

		wprintf(L"First listener. Event: %s Symbol: %s\n",dx_event_type_to_string(event_type), symbol_name);

		if (event_type == DX_ET_TRADE){
			dxf_trade_t* trade = (dxf_trade_t*)data;

			for ( ; i < data_count ; ++i )
				wprintf(L"Last.Exchange=%C Last.Time=%i  Last.Price=%f Last.Size=%i Last.Tick=%i Last.Change=%f Volume=%f\n" , 
				trade[i].last_exchange,trade[i].last_time,trade[i].last_price,trade[i].last_size,trade[i].last_tick,trade[i].last_change,trade[i].volume);
		}
	
}
void second_listener(int event_type, dx_const_string_t symbol_name,const dx_event_data_t* data, int data_count){
		dx_int_t i = 0;

		wprintf(L"Second listener. Event: %s Symbol: %s\n",dx_event_type_to_string(event_type), symbol_name);
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
	


int main (int argc, char* argv[]) {
    dxf_subscription_t subscription;

	dx_string_t symbols_to_add[] = { {L"MSFT"}, {L"YHOO"}, {L"C"} };
	dx_int_t symbols_to_add_size = sizeof (symbols_to_add) / sizeof (symbols_to_add[0]);
   
	dx_logger_initialize( "log.log", true, true, true );
	
	printf("API test started.\n");    
    printf("Connecting to host %s...\n", dxfeed_host);
    
    if (!dxf_connect_feed(dxfeed_host)) {
        process_last_error();
        return -1;
    }

    printf("Connection successful!\n");
	
	printf("Creating subscription to: Trade\n");
	if (!dxf_create_subscription(DX_ET_TRADE /*| DX_ET_QUOTE | DX_ET_MARKET_MAKER | DX_ET_FUNDAMENTAL | DX_ET_PROFILE*/, &subscription )) {
        process_last_error();
        
        return -1;
    };
	
	printf("Symbol %s added\n", "IBM");
	if (!dxf_add_symbol(subscription, L"IBM")) {
        process_last_error();
        
        return -1;
    }; 
	printf("First listener attached\n");
	if (!dxf_attach_event_listener(subscription, first_listener)) {
        process_last_error();
        
        return -1;
    };
    printf("Second listener attached\n");
	if (!dxf_attach_event_listener(subscription, second_listener)) {
        process_last_error();
        
        return -1;
    };
	Sleep (5000); 

	printf("Adding symbols: %S %S %S \n", symbols_to_add[0], symbols_to_add[1], symbols_to_add[2] );
	if (!dxf_add_symbols(subscription, symbols_to_add, symbols_to_add_size)) {
        process_last_error();
        
        return -1;
    }; 
	Sleep (5000); 
	
	printf("Removing second listener \n" );
	if (!dxf_detach_event_listener(subscription, second_listener)) {
        process_last_error();
        
        return -1;
    }; 
	Sleep (5000); 


    Sleep(1000000);
    
    printf("Disconnecting from host...\n");
    
    if (!dxf_disconnect_feed()) {
        process_last_error();
        
        return -1;
    }
    
    printf("Disconnect successful!\n"
           "Connection test completed successfully!\n");
           
    return 0;
}
