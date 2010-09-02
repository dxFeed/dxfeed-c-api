

#include "DXFeed.h"
#include "DXErrorCodes.h"
#include <stdio.h>
#include <Windows.h>

const char dxfeed_host[] = "demo.dxfeed.com:7300";
void MM_listener(int event_type, dx_const_string_t symbol_name,const dx_event_data_t* data, int data_count){
		dx_int_t i = 0;
		struct dxf_market_maker* mm = (struct dxf_market_maker*)data;

		wprintf(L"Record: %i \n",event_type);
		wprintf(L"Symbol: %s\n", symbol_name);

		for ( ; i < data_count ; ++i )
			wprintf(L"MMExchange=%C MMID=%i  MMBid.Price=%f MMBid.Size=%i MMAsk.Price=%f MMAsk.Size=%i\n" , 
			mm[i].mm_exchange,mm[i].mm_id,mm[i].mmbid_price ,mm[i].mmbid_size ,mm[i].mmask_price ,mm[i].mmask_size);
}
void quote_listener(int event_type, dx_const_string_t symbol_name,const dx_event_data_t* data, int data_count){
		dx_int_t i = 0;
		struct dxf_quote_t* quotes = (struct dxf_quote_t*)data;

		wprintf(L"Record: %i \n",event_type);
		wprintf(L"Symbol: %s\n", symbol_name);

		for ( ; i < data_count ; ++i )
			wprintf(L"Bid.Exchange=%C Bid.Price=%f Bid.Size=%i Ask.Exchange=%C Ask.Price=%f Ask.Size=%i Bid.Time=%i Ask.Time=%i \n" , 
			quotes[i].bid_exchange,quotes[i].bid_price,quotes[i].bid_size,quotes[i].ask_exchange,quotes[i].ask_price,quotes[i].ask_size,quotes[i].bid_time,quotes[i].ask_time);


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
	
	printf("Sample test started.\n");    
    printf("Connecting to host %s...\n", dxfeed_host);
    
    if (!dxf_connect_feed(dxfeed_host)) {
        process_last_error();
        return -1;
    }

    printf("Connection successful!\n");
    //dxf_add_subscription(DX_ET_QUOTE, &subscription );
	dxf_add_subscription(DX_ET_MARKET_MAKER, &subscription );
	dxf_add_symbol(subscription, L"IBM"); 
	dxf_attach_event_listener(subscription, MM_listener);
    Sleep(1000000);
    
  //  printf("Disconnecting from host...\n");
    
 //   if (!dxf_disconnect_feed()) {
  //      process_last_error();
        
  //      return -1;
  //  }
    
  //  printf("Disconnect successful!\n"
  //         "Connection test completed successfully!\n");
           
    return 0;
}

