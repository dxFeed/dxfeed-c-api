/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is Devexperts LLC.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 */

#include "DXFeed.h"
#include "DXNetwork.h"
#include "DXErrorHandling.h"
#include "DXErrorCodes.h"
#include "DXMemory.h"
#include "Subscription.h"
#include <Windows.h>
#include <stdio.h>
#include "SymbolCodec.h"
#include "EventSubscription.h"
#include "DataStructures.h"

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
    return TRUE;
}

/* -------------------------------------------------------------------------- */
/*
 *	Temporary internal stuff
 */
/* -------------------------------------------------------------------------- */

void data_receiver (const void* buffer, unsigned buflen) {
    printf("Internal data receiver stub. Data received: %d bytes\n", buflen);
	// TODO: process errors 
	dx_parse(buffer, buflen);
}

/* -------------------------------------------------------------------------- */

bool dx_update_record_description (void) {
    dx_byte_t* msg_buffer;
    dx_int_t msg_length;
    bool res = false;    
    
    if (dx_compose_description_message(&msg_buffer, &msg_length) != R_SUCCESSFUL) {
        return false;
    }
    
    res = dx_send_data(msg_buffer, msg_length);
    
    dx_free(msg_buffer);

    return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	DXFeed API implementation
 */
/* -------------------------------------------------------------------------- */

DXFEED_API int dxf_connect_feed (const char* host) {
    struct dx_connection_context_t cc;
    
    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
    
    cc.receiver = data_receiver;
    
    dx_clear_record_description_state();
    
    if (!dx_create_connection(host, &cc) ||
        !dx_update_record_description()) {
        return DXF_FAILURE;
    }
    
    return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API int dxf_disconnect_feed () {
    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
    
    return dx_close_connection();
}

/* -------------------------------------------------------------------------- */

DXFEED_API int dxf_get_last_error (int* subsystem_id, int* error_code, const char** error_descr) {
    int res = dx_get_last_error(subsystem_id, error_code, error_descr);
    
    switch (res) {
    case efr_no_error_stored:
        if (subsystem_id != NULL) {
            *subsystem_id = dx_sc_invalid_subsystem;
        }
        
        if (error_code != NULL) {
            *error_code = DX_INVALID_ERROR_CODE;
        }
        
        if (error_descr != NULL) {
            *error_descr = "No error occurred";
        }
    case efr_success:
        return DXF_SUCCESS;
    }
    
    return DXF_FAILURE;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_create_subscription (int event_types, OUT dxf_subscription_t* subscription){
	static int symbol_codec_initialized = 0;
		
    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	// initialization of penta codec. do not remove!
	if (symbol_codec_initialized == 0){
		dx_init_symbol_codec();
		symbol_codec_initialized = 1; 
	}
	*subscription = dx_create_event_subscription(event_types);

	if (*subscription == dx_invalid_subscription)
		return DXF_FAILURE;

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_add_symbols (dxf_subscription_t subscription, dx_string_t* symbols, int symbol_count) {
	dx_int_t i;

    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (symbol_count < 0) {
		return DXF_FAILURE; //TODO: set_last_error 
    }
    
    for ( i = 0; i < symbol_count; ++i) {
		if (dxf_add_symbol(subscription, symbols[i]) == DXF_FAILURE) {
			return DXF_FAILURE;
		}
	}

    return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_add_symbol (dxf_subscription_t subscription, dx_string_t symbol) {
	dx_event_id_t eid = dx_eid_begin;

	dx_byte_t* sub_buffer = NULL;
	dx_int_t out_len = 1000;
	
	dx_int_t events; 
	
    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (!dx_get_event_subscription_event_types(subscription, &events)) {
		return DXF_FAILURE;
	}
	
    for (; eid < dx_eid_count; ++eid) {
		if (events & DX_EVENT_BIT_MASK(eid)) {
            if ( !dx_create_subscription(&sub_buffer, &out_len, 
                                         MESSAGE_TICKER_ADD_SUBSCRIPTION,
                                         dx_encode_symbol_name(symbol),
                                         symbol, eid) ) {
                return DXF_FAILURE;
            }

			if (!dx_send_data(sub_buffer, out_len)) {
			    dx_free(sub_buffer);
			    
			    return DXF_FAILURE;
			}
			
			dx_free(sub_buffer);
		}
	}
	
	if (!dx_add_symbols(subscription, &symbol, 1)) {
		return DXF_FAILURE;
    }
	
	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_attach_event_listener (dxf_subscription_t subscription, dx_event_listener_t event_listener) {
    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (!dx_add_listener (subscription, event_listener)) {
		return DXF_FAILURE;
	}
	
	return DXF_SUCCESS;
}

DXFEED_API ERRORCODE dxf_subscription_clear_symbols (dxf_subscription_t subscription){

}

