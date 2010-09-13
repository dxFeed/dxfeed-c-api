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

#ifndef _DXFEED_API_H_INCLUDED_
#define _DXFEED_API_H_INCLUDED_

#ifdef DXFEED_EXPORTS
    #define DXFEED_API __declspec(dllexport)
#elif  DXFEED_IMPORTS
    #define DXFEED_API __declspec(dllimport)
#else
    #define DXFEED_API
#endif

#ifndef OUT
#define OUT
#endif // OUT

#include "DXTypes.h"
#include "EventData.h"
/* -------------------------------------------------------------------------- */
/*
 *	DXFeed API function return value codes
 */
/* -------------------------------------------------------------------------- */

#define DXF_SUCCESS 1
#define DXF_FAILURE 0

/* -------------------------------------------------------------------------- */
/*
 *	DXFeed C API functions
 */
/* -------------------------------------------------------------------------- */

// creates a connection to the specified host
// the function returns the success/error code of the operation,
// if the function succeeds it returns the handle of the opened
// connection via the function's out-parameter

DXFEED_API int dxf_connect_feed (const char* host);

// disconnect a connection, previously created by 'connect'
// returns the success/error code of the operation

DXFEED_API int dxf_disconnect_feed();

DXFEED_API int dxf_get_last_error (int* subsystem_id, int* error_code, const char** error_descr);

// subscribes to the selected type of events on the given connection
// returns the success/error code of the operation
// if the function succeeds it returns the handle of the created subscription
// via the function's out-parameter

DXFEED_API ERRORCODE dxf_create_subscription (int event_types, OUT dxf_subscription_t* subscription);
DXFEED_API ERRORCODE dxf_add_symbols (dxf_subscription_t subscription, dx_string_t* symbols, int symbol_count);
DXFEED_API ERRORCODE dxf_add_symbol (dxf_subscription_t subscription, dx_string_t symbol);
DXFEED_API ERRORCODE dxf_attach_event_listener (dxf_subscription_t subscription, dx_event_listener_t event_listener);
DXFEED_API ERRORCODE dxf_subscription_clear_symbols (dxf_subscription_t subscription);
DXFEED_API ERRORCODE dxf_detach_event_listener (dxf_subscription_t subscription, dx_event_listener_t event_listener);
DXFEED_API ERRORCODE dxf_remove_subscription (dxf_subscription_t subscription); // do not erase subscription data!
DXFEED_API ERRORCODE dxf_add_subscription (dxf_subscription_t subscription); // see next line
DXFEED_API ERRORCODE dxf_close_subscription (dxf_subscription_t subscription); // erase data

// TODO: not implemented
DXFEED_API ERRORCODE dxf_get_last_event (int event_type, dx_string_t symbol, OUT void* data);
DXFEED_API ERRORCODE dxf_get_symbols (dxf_subscription_t subscription, OUT dx_string_t* symbols, int* symbol_count);
DXFEED_API ERRORCODE dxf_get_subscription_event_types (dxf_subscription_t subscription, OUT int* event_types);
DXFEED_API ERRORCODE dxf_remove_symbols (dxf_subscription_t subscription, dx_string_t* symbols, int symbol_count);
DXFEED_API ERRORCODE dxf_set_symbols (dxf_subscription_t subscription, dx_string_t* symbols, int symbol_count);



#endif // _DXFEED_API_H_INCLUDED_
