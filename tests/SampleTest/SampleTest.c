

#include "DXFeed.h"
#include "DXErrorCodes.h"
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
void listener(int event_type, dx_const_string_t symbol_name,const dx_event_data_t* data, int data_count){
		dx_int_t i = 0;

		wprintf(L"Record: %s Symbol: %s\n",dx_event_type_to_string(event_type), symbol_name);

		if (event_type == DX_ET_QUOTE){
			dxf_quote_t* quotes = (dxf_quote_t*)data;

			for ( ; i < data_count ; ++i )
				wprintf(L"Bid.Exchange=%C Bid.Price=%f Bid.Size=%i Ask.Exchange=%C Ask.Price=%f Ask.Size=%i Bid.Time=%i Ask.Time=%i \n" , 
				quotes[i].bid_exchange,quotes[i].bid_price,quotes[i].bid_size,quotes[i].ask_exchange,quotes[i].ask_price,quotes[i].ask_size,quotes[i].bid_time,quotes[i].ask_time);
		}
		if (event_type == DX_ET_MARKET_MAKER){
			dxf_market_maker* mm = (dxf_market_maker*)data;
		
			for ( ; i < data_count ; ++i )
				wprintf(L"MMExchange=%C MMID=%i  MMBid.Price=%f MMBid.Size=%i MMAsk.Price=%f MMAsk.Size=%i\n" , 
				mm[i].mm_exchange,mm[i].mm_id,mm[i].mmbid_price ,mm[i].mmbid_size ,mm[i].mmask_price ,mm[i].mmask_size);
		}
		if (event_type == DX_ET_TRADE){
			dxf_trade_t* trade = (dxf_trade_t*)data;

			for ( ; i < data_count ; ++i )
				wprintf(L"Last.Exchange=%C Last.Time=%i  Last.Price=%f Last.Size=%i Last.Tick=%i Last.Change=%f Volume=%f\n" , 
				trade[i].last_exchange,trade[i].last_time,trade[i].last_price,trade[i].last_size,trade[i].last_tick,trade[i].last_change,trade[i].volume);
		}
		if (event_type == DX_ET_FUNDAMENTAL){
			dxf_fundamental_t* fundamental = (dxf_fundamental_t*)data;

			for ( ; i < data_count ; ++i )
				wprintf(L"High.Price=%f Low.Price=%f  Open.Price=%f Close.Price=%f OpenInterest=%i\n"  , 
				fundamental[i].high_price, fundamental[i].low_price, fundamental[i].open_price, fundamental[i].close_price, fundamental[i].open_interest);
		}
		if (event_type == DX_ET_PROFILE){
			dxf_profile_t* p = (dxf_profile_t*)data;
		
			for (; i < data_count ; ++i) {
				wprintf(L"Beta=%f Eps=%f DivFreq=%i ExdDiv.Amount=%f ExdDiv.Date=%i "
				        L"52High.Price=%f 52Low.Price=%f Shares=%f IsIndex=%i Description=%s\n",
				        p[i].beta, p[i].eps, p[i].div_freq, p[i].exd_div_amount, p[i].exd_div_date,
				        p[i].price_52_high, p[i].price_52_low, p[i].shares, p[i].is_index, p[i].description);
		    }
		}	
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
 
	if (!dxf_add_subscription(/*DX_ET_TRADE | DX_ET_QUOTE | DX_ET_MARKET_MAKER | DX_ET_FUNDAMENTAL |*/ DX_ET_PROFILE, &subscription )) {
        process_last_error();
        
        return -1;
    };
	
	if (!dxf_add_symbol(subscription, L"IBM")) {
        process_last_error();
        
        return -1;
    }; 

	if (!dxf_attach_event_listener(subscription, listener)) {
        process_last_error();
        
        return -1;
    };
    printf("Subscription successful!\n");
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

