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
#include "parser.h"
#include "Logger.h"
#include "DXAlgorithms.h"

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
 *	Event subscription helper stuff
 */
/* -------------------------------------------------------------------------- */

typedef enum {
    dx_at_add_subscription,
    dx_at_remove_subscription,
    
    /* add new action types above this line */
    
    dx_at_count
} dx_action_type_t;

dx_message_type_t dx_get_subscription_message_type (dx_action_type_t action_type, dx_subscription_type_t subscr_type) {
    static dx_message_type_t subscr_message_types[dx_at_count][dx_st_count] = {
        { MESSAGE_TICKER_ADD_SUBSCRIPTION, MESSAGE_STREAM_ADD_SUBSCRIPTION, MESSAGE_HISTORY_ADD_SUBSCRIPTION },
        { MESSAGE_TICKER_REMOVE_SUBSCRIPTION, MESSAGE_STREAM_REMOVE_SUBSCRIPTION, MESSAGE_HISTORY_REMOVE_SUBSCRIPTION }
    };
    
    return subscr_message_types[action_type][subscr_type];
}

/* -------------------------------------------------------------------------- */
/*
 *	Temporary internal stuff
 */
/* -------------------------------------------------------------------------- */

bool data_receiver (const void* buffer, unsigned buflen) {
    printf("Internal data receiver stub. Data received: %d bytes\n", buflen);
    
    dx_logging_info(L"Data received: %d bytes",  buflen);

	return (dx_parse(buffer, buflen) == R_SUCCESSFUL);
}

/* -------------------------------------------------------------------------- */

bool dx_update_record_description (void) {
    dx_byte_t* msg_buffer;
    dx_int_t msg_length;
    bool res = false;    
    
    dx_logging_info(L"Update record description");

    if (dx_compose_description_message(&msg_buffer, &msg_length) != R_SUCCESSFUL) {
        return false;
    }
    
    res = dx_send_data(msg_buffer, msg_length);
    
    dx_free(msg_buffer);

    return res;
}

/* -------------------------------------------------------------------------- */

bool dx_subscribe_to (dx_const_string_t* symbols, int symbols_count, int event_types, bool unsubscribe) {
	int i = 0;

	dx_event_id_t eid = dx_eid_begin;

	dx_byte_t* sub_buffer = NULL;
	dx_int_t out_len = 1000;

	for (; i < symbols_count; ++i){
		for (; eid < dx_eid_count; ++eid) {
			if (event_types & DX_EVENT_BIT_MASK(eid)) {
			    const dx_event_subscription_param_t* subscr_params = NULL;
			    int j = 0;
			    int param_count = dx_get_event_subscription_params(eid, &subscr_params);
			    
			    for (; j < param_count; ++j) {
			        const dx_event_subscription_param_t* cur_param = subscr_params + j;
			        dx_message_type_t msg_type = dx_get_subscription_message_type(unsubscribe ? dx_at_remove_subscription : dx_at_add_subscription, cur_param->subscription_type);
			        
                    if (dx_create_subscription(&sub_buffer, &out_len, 
                                               msg_type,
                                               dx_encode_symbol_name(symbols[i]),
                                               symbols[i], cur_param->record_id) != R_SUCCESSFUL) {
                        return false;
                    }

                    if (!dx_send_data(sub_buffer, out_len)) {
                        dx_free(sub_buffer);

                        return false;
                    }

                    dx_free(sub_buffer);
			    }
			}
		}
	}
	
	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_subscribe (dx_const_string_t* symbols, int symbols_count, int event_types) {
	return dx_subscribe_to(symbols,symbols_count, event_types, false);
}

/* -------------------------------------------------------------------------- */

bool dx_unsubscribe (dx_string_t* symbols, int symbols_count, int event_types) {
	return dx_subscribe_to(symbols,symbols_count, event_types, true);
}

/* -------------------------------------------------------------------------- */
/*
 *	DXFeed API implementation
 */
/* -------------------------------------------------------------------------- */

DXFEED_API int dxf_connect_feed (const char* host, dx_on_reader_thread_terminate_t term) {
    struct dx_connection_context_t cc;

    {
        dx_string_t w_host = dx_ansi_to_unicode(host);
        
        if (w_host != NULL) {
            dx_logging_info(L"Connecting to host: %s", w_host);
            dx_free(w_host);
        }
    }
    
    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
    
    cc.receiver = data_receiver;
    cc.terminator = term;
    
    dx_clear_records_server_support_states();
    
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
    
    dx_logging_info(L"Disconnect");
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

    dx_logging_info(L"Create subscription, event types: %x", event_types);

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

DXFEED_API ERRORCODE dxf_add_symbols (dxf_subscription_t subscription, dx_const_string_t* symbols, int symbols_count) {
	dx_int_t i;

    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (symbols_count < 0) {
		return DXF_FAILURE; //TODO: set_last_error 
    }
    
    for ( i = 0; i < symbols_count; ++i) {
		if (dxf_add_symbol(subscription, symbols[i]) == DXF_FAILURE) {
			return DXF_FAILURE;
		}
	}

    return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_add_symbol (dxf_subscription_t subscription, dx_const_string_t symbol) {
	dx_int_t events; 
	
    dx_logging_info(L"Adding symbol %s", symbol);

    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (!dx_get_event_subscription_event_types(subscription, &events)) {
		return DXF_FAILURE;
    }
			
	if (!dx_add_symbols(subscription, &symbol, 1)) {
		return DXF_FAILURE;
	}
	
	if (!dx_subscribe(&symbol, 1, events)) {
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

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_subscription_clear_symbols (dxf_subscription_t subscription) {
	dx_int_t events; 
	
	dx_string_t* symbols;

	int symbols_count;
	int iter = 0;

    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (!dx_get_event_subscription_event_types(subscription, &events)) {
		return DXF_FAILURE;
	}
	
	if (!dx_get_event_subscription_symbols(subscription, &symbols, &symbols_count)) {
		return DXF_FAILURE;
	}

	if (!dx_remove_symbols(subscription, symbols, symbols_count)) {
		return DXF_FAILURE; 
    }

	if (!dx_unsubscribe(symbols, symbols_count, events))
		return DXF_FAILURE;

	
	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_remove_symbols (dxf_subscription_t subscription, dx_string_t* symbols, int symbols_count) {
	dx_int_t events; 
	
	dx_string_t* existing_symbols;
	int existing_symbols_count;

	int i = 0;
	int j = 0;

    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (!dx_get_event_subscription_event_types(subscription, &events)) {
		return DXF_FAILURE;
	}
	
	if (!dx_get_event_subscription_symbols(subscription, &existing_symbols, &existing_symbols_count)) {
		return DXF_FAILURE;
	}

	for (; i < symbols_count; ++i) {
		for ( j = 0; j < existing_symbols_count; ++j) {
			if (symbols[i] == existing_symbols[j]) {
				if (!dx_remove_symbols(subscription, symbols, symbols_count)) {
					return DXF_FAILURE; 
			    }
			}
		}
	}

	if (!dx_unsubscribe(symbols, symbols_count, events)) {
		return DXF_FAILURE;
	}
	
	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_set_symbols (dxf_subscription_t subscription, dx_string_t* symbols, int symbols_count){
	
    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (dxf_subscription_clear_symbols(subscription) == DXF_FAILURE)
		return DXF_FAILURE;

	if (dxf_add_symbols(subscription, symbols, symbols_count) == DXF_FAILURE)
		return DXF_FAILURE;

	return DXF_SUCCESS;
}


/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_detach_event_listener (dxf_subscription_t subscription, dx_event_listener_t event_listener) {
	if (!dx_remove_listener(subscription, event_listener))
		return DXF_FAILURE;
		
	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_close_subscription (dxf_subscription_t subscription) {
	dx_int_t events; 
	
	dx_string_t* symbols;
	int symbols_count;

    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (!dx_get_event_subscription_event_types(subscription, &events)) {
		return DXF_FAILURE;
	}
	
	if (!dx_get_event_subscription_symbols(subscription, &symbols, &symbols_count)) 
		return DXF_FAILURE;
	
	if (!dx_unsubscribe(symbols, symbols_count, events))
		return DXF_FAILURE;

	if (!dx_close_event_subscription(subscription))
		return DXF_FAILURE;
		
	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_remove_subscription (dxf_subscription_t subscription) {
	dx_int_t events; 
		
	dx_string_t* symbols;
	int symbols_count;

    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (!dx_get_event_subscription_event_types(subscription, &events)) 
		return DXF_FAILURE;
	
	if (!dx_get_event_subscription_symbols(subscription, &symbols, &symbols_count)) 
		return DXF_FAILURE;
	
	if (!dx_unsubscribe(symbols, symbols_count, events))
		return DXF_FAILURE;

	if (!dx_mute_event_subscription(subscription))
		return DXF_FAILURE;	

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_add_subscription (dxf_subscription_t subscription) {
	dx_int_t events; 
	
	dx_string_t* symbols;
	int symbols_count;

    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (!dx_get_event_subscription_event_types(subscription, &events)) 
		return DXF_FAILURE;

	if (!dx_get_event_subscription_symbols(subscription, &symbols, &symbols_count)) 
		return DXF_FAILURE;

	if (!dx_unmute_event_subscription(subscription))
		return DXF_FAILURE;	
	
	if (!dx_subscribe(symbols, symbols_count, events))
		return DXF_FAILURE;
	
	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_get_symbols (dxf_subscription_t subscription, OUT dx_string_t* symbols, int* symbols_count){
	if (!dx_get_event_subscription_symbols(subscription, &symbols, symbols_count)) 
		return DXF_FAILURE;

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_get_subscription_event_types (dxf_subscription_t subscription, OUT int* event_types) {
	if (!dx_get_event_subscription_event_types(subscription, event_types)) 
		return DXF_FAILURE;

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_get_last_event( int event_type, dx_const_string_t symbol, OUT void** data ) {
    if (!dx_get_last_symbol_event(symbol, event_type, data)) {
        return DXF_FAILURE;
    }

    return DXF_SUCCESS;
}