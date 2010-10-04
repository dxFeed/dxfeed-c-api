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

#include <Windows.h>
#include <stdio.h>

#include "DXFeed.h"
#include "DXNetwork.h"
#include "DXErrorHandling.h"
#include "DXErrorCodes.h"
#include "DXMemory.h"
#include "Subscription.h"
#include "SymbolCodec.h"
#include "EventSubscription.h"
#include "DataStructures.h"
#include "parser.h"
#include "Logger.h"
#include "DXAlgorithms.h"
#include "ConnectionContextData.h"

BOOL APIENTRY DllMain (HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call) {
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
 *	Auxiliary internal functions
 */
/* -------------------------------------------------------------------------- */

bool dx_subscribe_to (dxf_connection_t connection,
                      dx_const_string_t* symbols, int symbol_count, int event_types, bool unsubscribe) {
	int i = 0;

	dx_byte_t* sub_buffer = NULL;
	dx_int_t out_len = 1000;

	for (; i < symbol_count; ++i){
		dx_event_id_t eid = dx_eid_begin;

		for (; eid < dx_eid_count; ++eid) {
			if (event_types & DX_EVENT_BIT_MASK(eid)) {
			    const dx_event_subscription_param_t* subscr_params = NULL;
			    int j = 0;
			    int param_count = dx_get_event_subscription_params(eid, &subscr_params);
			    
			    for (; j < param_count; ++j) {
			        const dx_event_subscription_param_t* cur_param = subscr_params + j;
			        dx_message_type_t msg_type = dx_get_subscription_message_type(unsubscribe ? dx_at_remove_subscription : dx_at_add_subscription, cur_param->subscription_type);
			        
                    if (dx_create_subscription(msg_type, symbols[i], dx_encode_symbol_name(symbols[i]),
                                               cur_param->record_id,
                                               &sub_buffer, &out_len) != R_SUCCESSFUL) {
                        return false;
                    }

                    if (!dx_send_data(connection, sub_buffer, out_len)) {
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

bool dx_subscribe (dxf_connection_t connection, dx_const_string_t* symbols, int symbols_count, int event_types) {
	return dx_subscribe_to(connection, symbols,symbols_count, event_types, false);
}

/* -------------------------------------------------------------------------- */

bool dx_unsubscribe (dxf_connection_t connection, dx_const_string_t* symbols, int symbols_count, int event_types) {
	return dx_subscribe_to(connection, symbols,symbols_count, event_types, true);
}

/* -------------------------------------------------------------------------- */
/*
 *	DXFeed API implementation
 */
/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_create_connection (const char* host, dxf_conn_termination_notifier_t notifier,
                                            OUT dxf_connection_t* connection) {
    dx_connection_context_t cc;
    
    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
    
    if (connection == NULL) {
        return DXF_FAILURE;
    }
    
    if ((*connection = dx_init_connection()) == NULL) {
        return DXF_FAILURE;
    }

    {
        dx_string_t w_host = dx_ansi_to_unicode(host);
        
        if (w_host != NULL) {
            dx_logging_info(L"Connecting to host: %s", w_host);
            dx_free(w_host);
        }
    }
    
    cc.receiver = dx_socket_data_receiver;
    cc.notifier = notifier;
    
    if (!dx_bind_to_host(*connection, host, &cc) ||
        !dx_update_record_description(*connection)) {
        dx_deinit_connection(*connection);
        
        *connection = NULL;
        
        return DXF_FAILURE;
    }
    
    return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_close_connection (dxf_connection_t connection) {
    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
    
    dx_logging_info(L"Disconnect");
    
    return (dx_deinit_connection(connection) ? DXF_SUCCESS : DXF_FAILURE);
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_get_last_error (int* subsystem_id, int* error_code, dx_const_string_t* error_descr) {
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
            *error_descr = L"No error occurred";
        }
    case efr_success:
        return DXF_SUCCESS;
    }
    
    return DXF_FAILURE;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_create_subscription (dxf_connection_t connection, int event_types, OUT dxf_subscription_t* subscription){
	static bool symbol_codec_initialized = false;

    dx_logging_info(L"Create subscription, event types: %x", event_types);

    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	// initialization of penta codec. do not remove!
	if (!symbol_codec_initialized) {
		dx_init_symbol_codec();
		symbol_codec_initialized = 1; 
	}
	
	*subscription = dx_create_event_subscription(connection, event_types);

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
	dxf_connection_t connection;
	
    dx_logging_info(L"Adding symbol %s", symbol);

    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (!dx_get_subscription_connection(subscription, &connection)) {
	    return DXF_FAILURE;
	}
	
	if (!dx_get_event_subscription_event_types(subscription, &events)) {
		return DXF_FAILURE;
    }
			
	if (!dx_add_symbols(subscription, &symbol, 1)) {
		return DXF_FAILURE;
	}
	
	if (!dx_subscribe(connection, &symbol, 1, events)) {
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
	dxf_connection_t connection;
    dx_int_t count = 0;

    dx_string_t* symbols;
	dx_string_t* symbol_array;

	int symbols_count;
	int iter = 0;

    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (!dx_get_subscription_connection(subscription, &connection)) {
	    return DXF_FAILURE;
	}
	
	if (!dx_get_event_subscription_event_types(subscription, &events)) {
		return DXF_FAILURE;
	}
	
	if (!dx_get_event_subscription_symbols(subscription, &symbols, &symbols_count)) {
		return DXF_FAILURE;
	}

    symbol_array = dx_calloc(symbols_count, sizeof(dx_const_string_t*));
	
	for (; count < symbols_count; ++count){
		symbol_array[count] = dx_create_string_src(symbols[count]);
	}

	if (!dx_remove_symbols(subscription, symbols, symbols_count)) {
		return DXF_FAILURE; 
    }

    if (!dx_unsubscribe(connection, symbol_array, symbols_count, events))
		return DXF_FAILURE;

	
	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_remove_symbols (dxf_subscription_t subscription, dx_const_string_t* symbols, int symbols_count) {
	dxf_connection_t connection;
	dx_int_t events; 
	
	dx_string_t* existing_symbols;
	int existing_symbols_count;

	int i = 0;
	int j = 0;

    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (!dx_get_subscription_connection(subscription, &connection)) {
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

	if (!dx_unsubscribe(connection, symbols, symbols_count, events)) {
		return DXF_FAILURE;
	}
	
	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_set_symbols (dxf_subscription_t subscription, dx_const_string_t* symbols, int symbols_count) {	
    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (dxf_subscription_clear_symbols(subscription) == DXF_FAILURE) {
		return DXF_FAILURE;
    }

	if (dxf_add_symbols(subscription, symbols, symbols_count) == DXF_FAILURE) {
		return DXF_FAILURE;
	}

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
	dxf_connection_t connection;
	dx_int_t events;
	
	dx_const_string_t* symbols;
	int symbols_count;

    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (!dx_get_subscription_connection(subscription, &connection)) {
	    return DXF_FAILURE;
	}
	
	if (!dx_get_event_subscription_event_types(subscription, &events)) {
		return DXF_FAILURE;
	}
	
	if (!dx_get_event_subscription_symbols(subscription, &symbols, &symbols_count)) 
		return DXF_FAILURE;
	
	if (!dx_unsubscribe(connection, symbols, symbols_count, events))
		return DXF_FAILURE;

	if (!dx_close_event_subscription(subscription))
		return DXF_FAILURE;
		
	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_remove_subscription (dxf_subscription_t subscription) {
	dxf_connection_t connection;
	dx_int_t events;
		
	dx_const_string_t* symbols;
	int symbols_count;

    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (!dx_get_subscription_connection(subscription, &connection)) {
	    return DXF_FAILURE;
	}
	
	if (!dx_get_event_subscription_event_types(subscription, &events)) 
		return DXF_FAILURE;
	
	if (!dx_get_event_subscription_symbols(subscription, &symbols, &symbols_count)) 
		return DXF_FAILURE;
	
	if (!dx_unsubscribe(connection, symbols, symbols_count, events))
		return DXF_FAILURE;

	if (!dx_mute_event_subscription(subscription))
		return DXF_FAILURE;	

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_add_subscription (dxf_subscription_t subscription) {
	dxf_connection_t connection;
	dx_int_t events; 
	
	dx_const_string_t* symbols;
	int symbols_count;

    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
	
	if (!dx_get_subscription_connection(subscription, &connection)) {
	    return DXF_FAILURE;
	}
	
	if (!dx_get_event_subscription_event_types(subscription, &events)) 
		return DXF_FAILURE;

	if (!dx_get_event_subscription_symbols(subscription, &symbols, &symbols_count)) 
		return DXF_FAILURE;

	if (!dx_unmute_event_subscription(subscription))
		return DXF_FAILURE;	
	
	if (!dx_subscribe(connection, symbols, symbols_count, events))
		return DXF_FAILURE;
	
	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_get_symbols (dxf_subscription_t subscription, OUT dx_const_string_t* symbols, int* symbols_count){
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

DXFEED_API ERRORCODE dxf_get_last_event (dxf_connection_t connection, int event_type, dx_const_string_t symbol, OUT void** data) {
    if (!dx_get_last_symbol_event(connection, symbol, event_type, data)) {
        return DXF_FAILURE;
    }

    return DXF_SUCCESS;
}