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

#ifndef DXFEED_API_H_INCLUDED
#define DXFEED_API_H_INCLUDED

#ifdef DXFEED_EXPORTS
    #define DXFEED_API __declspec(dllexport)
#elif  DXFEED_IMPORTS
    #define DXFEED_API __declspec(dllimport)
#else
    #define DXFEED_API
#endif

#ifndef OUT
    #define OUT
#endif /* OUT */

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
 *	DXFeed API generic types
 */
/* -------------------------------------------------------------------------- */

typedef void* dxf_connection_t;
typedef void (*dxf_conn_termination_notifier_t) (const char* host);

/* -------------------------------------------------------------------------- */
/*
 *	DXFeed C API functions
 */
/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_create_connection (const char* host, dxf_conn_termination_notifier_t notifier,
                                            OUT dxf_connection_t* connection);
DXFEED_API ERRORCODE dxf_close_connection (dxf_connection_t connection);

DXFEED_API ERRORCODE dxf_create_subscription (dxf_connection_t connection, int event_types,
                                              OUT dxf_subscription_t* subscription);
DXFEED_API ERRORCODE dxf_remove_subscription (dxf_subscription_t subscription); // do not erase subscription data!
DXFEED_API ERRORCODE dxf_add_subscription (dxf_subscription_t subscription); // see next line
DXFEED_API ERRORCODE dxf_close_subscription (dxf_subscription_t subscription); // erase data

DXFEED_API ERRORCODE dxf_add_symbol (dxf_subscription_t subscription, dx_const_string_t symbol);
DXFEED_API ERRORCODE dxf_add_symbols (dxf_subscription_t subscription, dx_const_string_t* symbols, int symbol_count);
DXFEED_API ERRORCODE dxf_remove_symbols (dxf_subscription_t subscription, dx_const_string_t* symbols, int symbol_count);
DXFEED_API ERRORCODE dxf_get_symbols (dxf_subscription_t subscription, OUT dx_const_string_t* symbols, OUT int* symbol_count);
DXFEED_API ERRORCODE dxf_set_symbols (dxf_subscription_t subscription, dx_const_string_t* symbols, int symbol_count);
DXFEED_API ERRORCODE dxf_subscription_clear_symbols (dxf_subscription_t subscription);

DXFEED_API ERRORCODE dxf_attach_event_listener (dxf_subscription_t subscription, dx_event_listener_t event_listener);
DXFEED_API ERRORCODE dxf_detach_event_listener (dxf_subscription_t subscription, dx_event_listener_t event_listener);

DXFEED_API ERRORCODE dxf_get_subscription_event_types (dxf_subscription_t subscription, OUT int* event_types);
DXFEED_API ERRORCODE dxf_get_last_event (dxf_connection_t connection, int event_type, dx_const_string_t symbol,
                                         OUT const dx_event_data_t* event_data);

DXFEED_API ERRORCODE dxf_get_last_error (int* subsystem_id, int* error_code, dx_const_string_t* error_descr);

DXFEED_API ERRORCODE dxf_initialize_logger (const char* file_name, bool rewrite_file, bool show_timezone_info, bool verbose);

#endif /* DXFEED_API_H_INCLUDED */