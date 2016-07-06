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
#elif __cplusplus
    #define DXFEED_API extern "C"
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

typedef void (*dxf_conn_termination_notifier_t) (dxf_connection_t connection, void* user_data);

/* the low level callback types, required in case some thread-specific initialization must be performed
   on the client side on the thread creation/destruction */

typedef int (*dxf_socket_thread_creation_notifier_t) (dxf_connection_t connection, void* user_data);
typedef void (*dxf_socket_thread_destruction_notifier_t) (dxf_connection_t connection, void* user_data);

/* -------------------------------------------------------------------------- */
/*
 *	DXFeed C API functions
 
 *  All functions return DXF_SUCCESS on success and DXF_FAILURE if some error
 *  has occurred. Use 'dxf_get_last_error' to retrieve the error code
 *  and description.
 */
/* -------------------------------------------------------------------------- */

/*
 *	Creates connection with the specified parameters.
 
 *  address - "[host[:port],]host:port"
 *  notifier - the callback to inform the client side that the connection has stumbled upon and error and will go reconnecting
 *  stcn - the callback for informing the client side about the socket thread creation;
           may be set to NULL if no specific action is required to perform on the client side on a new thread creation
 *  shdn - the callback for informing the client side about the socket thread destruction;
           may be set to NULL if no specific action is required to perform on the client side on a thread destruction
 *  user_data - the user defined value passed to the termination notifier callback along with the connection handle; may be set
                to whatever value
 *  OUT connection - the handle of the created connection
 */
DXFEED_API ERRORCODE dxf_create_connection (const char* address,
                                            dxf_conn_termination_notifier_t notifier,
                                            dxf_socket_thread_creation_notifier_t stcn,
                                            dxf_socket_thread_destruction_notifier_t stdn,
                                            void* user_data,
                                            OUT dxf_connection_t* connection);

/*
 *	Closes a connection.
 
 *  connection - a handle of a previously created connection
 */
DXFEED_API ERRORCODE dxf_close_connection (dxf_connection_t connection);

/*
 *	Creates a subscription with the specified parameters.
 
 *  connection - a handle of a previously created connection which the subscription will be using
 *  event_types - a bitmask of the subscription event types. See 'dx_event_id_t' and 'DX_EVENT_BIT_MASK'
 *                for information on how to create an event type bitmask
 *  OUT subscription - a handle of the created subscription
 */
DXFEED_API ERRORCODE dxf_create_subscription (dxf_connection_t connection, int event_types,
                                              OUT dxf_subscription_t* subscription);

/*
 *	Creates a subscription with the specified parameters.

 *  connection - a handle of a previously created connection which the subscription will be using
 *  event_types - a bitmask of the subscription event types. See 'dx_event_id_t' and 'DX_EVENT_BIT_MASK'
 *                for information on how to create an event type bitmask
 *  time - time in the past (unix time in milliseconds)
 *  OUT subscription - a handle of the created subscription
 */
DXFEED_API ERRORCODE dxf_create_subscription_timed(dxf_connection_t connection, int event_types, dxf_long_t time,
                                                   OUT dxf_subscription_t* subscription);

/*
 *	Closes a subscription.
 *  All the data associated with it will be freed.
 
 *  subscription - a handle of the subscription to close
 */
DXFEED_API ERRORCODE dxf_close_subscription (dxf_subscription_t subscription);

/*
 *	Adds a single symbol to the subscription.
 
 *  subscription - a handle of the subscription to which a symbol is added
 *  symbol - the symbol to add
 */
DXFEED_API ERRORCODE dxf_add_symbol (dxf_subscription_t subscription, dxf_const_string_t symbol);

/*
 *	Adds several symbols to the subscription.
 *  No error occurs if the symbol is attempted to add for the second time.
 
 *  subscription - a handle of the subscription to which the symbols are added
 *  symbols - the symbols to add
 *  symbol_count - a number of symbols
 */
DXFEED_API ERRORCODE dxf_add_symbols (dxf_subscription_t subscription, dxf_const_string_t* symbols, int symbol_count);

/*
 *	Adds a candle symbol to the subscription.
 
 *  subscription - a handle of the subscription to which a symbol is added
 *  candle_attributes - pointer to the candle struct
 */
DXFEED_API ERRORCODE dxf_add_candle_symbol(dxf_subscription_t subscription, dxf_candle_attributes_t candle_attributes);

/*
 *	Removes a single symbol from the subscription.

 *  subscription - a handle of the subscription from which a symbol is removed
 *  symbol - the symbol to remove
 */
DXFEED_API ERRORCODE dxf_remove_symbol (dxf_subscription_t subscription, dxf_const_string_t symbol);

/*
 *	Removes several symbols from the subscription.
 *  No error occurs if it's attempted to remove symbols that weren't added beforehand.

 *  subscription - a handle of the subscription to which the symbols are added
 *  symbols - the symbols to remove
 *  symbol_count - a number of symbols
 */
DXFEED_API ERRORCODE dxf_remove_symbols (dxf_subscription_t subscription, dxf_const_string_t* symbols, int symbol_count);

/*
 *	Retrieves the list of symbols currently added to the subscription.
 *  The memory for the resulting list is allocated internally, so no actions to free it are required.
 *  The symbol name buffer is guaranteed to be valid until either the subscription symbol list is changed or a new call
 *  of this function is performed.
 
 *  subscription - a handle of the subscriptions whose symbols are to retrieve
 *  OUT symbols - a pointer to the string array object to which the symbol list is to be stored
 *  OUT symbol_count - a pointer to the variable to which the symbol count is to be stored
 */
DXFEED_API ERRORCODE dxf_get_symbols (dxf_subscription_t subscription, OUT dxf_const_string_t** symbols, OUT int* symbol_count);

/*f lj
 *	Sets the symbols for the subscription.
 *  The difference between this function and 'dxf_add_symbols' is that all the previously added symbols
 *  that do not belong to the symbol list passed to this function will be removed.
 
 *  subscription - a handle of the subscription whose symbols are to be set
 *  symbols - the symbol list to set
 *  symbol_count - the symbol count
 */
DXFEED_API ERRORCODE dxf_set_symbols (dxf_subscription_t subscription, dxf_const_string_t* symbols, int symbol_count);

/*
 *	Removes all the symbols from the subscription.
 
 *  subscription - a handle of the subscription whose symbols are to be cleared
 */
DXFEED_API ERRORCODE dxf_clear_symbols (dxf_subscription_t subscription);

/*
 *	Attaches a listener callback to the subscription.
 *  This callback will be invoked when the new event data for the subscription symbols arrives.
 *  No error occurs if it's attempted to attach the same listener twice or more.
 
 *  subscription - a handle of the subscription to which a listener is to be attached
 *  event_listener - a listener callback function pointer
 */
DXFEED_API ERRORCODE dxf_attach_event_listener (dxf_subscription_t subscription, dxf_event_listener_t event_listener,
                                                void* user_data);

/*
 *	Detaches a listener from the subscription.
 *  No error occurs if it's attempted to detach a listener which wasn't previously attached.
 
 *  subscription - a handle of the subscription from which a listener is to be detached
 *  event_listener - a listener callback function pointer
 */
DXFEED_API ERRORCODE dxf_detach_event_listener (dxf_subscription_t subscription, dxf_event_listener_t event_listener);

/*
*  Attaches a extended listener callback to the subscription.
*  This callback will be invoked when the new event data for the subscription symbols arrives.
*  No error occurs if it's attempted to attach the same listener twice or more.
* 
*  This listener differs with extend number of callback parameters.
*
*  subscription - a handle of the subscription to which a listener is to be attached
*  event_listener - a listener callback function pointer
*  user_data - if there isn't user data pass NULL
*/
DXFEED_API ERRORCODE dxf_attach_event_listener_v2(dxf_subscription_t subscription, 
                                                  dxf_event_listener_v2_t event_listener,
                                                  void* user_data);

/*
*  Detaches a extended listener from the subscription.
*  No error occurs if it's attempted to detach a listener which wasn't previously attached.

*  subscription - a handle of the subscription from which a listener is to be detached
*  event_listener - a listener callback function pointer
*/
DXFEED_API ERRORCODE dxf_detach_event_listener_v2(dxf_subscription_t subscription, 
                                                  dxf_event_listener_v2_t event_listener);

/*
 *	Retrieves the subscription event types.
 
 *  subscription - a handle of the subscription whose event types are to be retrieved
 *  OUT event_types - a pointer to the variable to which the subscription event types bitmask is to be stored
 */
DXFEED_API ERRORCODE dxf_get_subscription_event_types (dxf_subscription_t subscription, OUT int* event_types);

/*
 *	Retrieves the last event data of the specified symbol and type for the connection.
 
 *  connection - a handle of the connection whose data is to be retrieved
 *  event_type - an event type bitmask defining a single event type
 *  symbol - a symbol name
 *  OUT event_data - a pointer to the variable to which the last data buffer pointer is stored; if there was no
                     data for this connection/event type/symbol, NULL will be stored
 */
DXFEED_API ERRORCODE dxf_get_last_event (dxf_connection_t connection, int event_type, dxf_const_string_t symbol,
                                         OUT dxf_event_data_t* event_data);
/*
 *	Retrieves the last error info.
 *  The error is stored on per-thread basis. If the connection termination notifier callback was invoked, then
 *  to retrieve the connection's error code call this function right from the callback function.
 *  If an error occurred within the error storage subsystem itself, the function will return DXF_FAILURE.
 
 *  OUT error_code - a pointer to the variable where the error code is to be stored
 *  OUT error_descr - a pointer to the variable where the human-friendly error description is to be stored;
                      may be NULL if the text representation of error is not required
 */
DXFEED_API ERRORCODE dxf_get_last_error (OUT int* error_code, OUT dxf_const_string_t* error_descr);

/*
 *	Initializes the internal logger.
 *  Various actions and events, including the errors, are being logged throughout the library. They may be stored
 *  into the file.
 *
 *  file_name - a full path to the file where the log is to be stored
 *  rewrite_file - a flag defining the file open mode; if it's nonzero then the log file will be rewritten
 *  show_timezone_info - a flag defining the time display option in the log file; if it's nonzero then
                         the time will be displayed with the timezone suffix
 *  verbose - a flag defining the logging mode; if it's nonzero then the verbose logging will be enabled
 */
DXFEED_API ERRORCODE dxf_initialize_logger (const char* file_name, int rewrite_file, int show_timezone_info, int verbose);

/*
 *  Clear current sources and add new one to subscription
 *  Warning: you must configure order source before dxf_add_symbols/dxf_add_symbol call
 *
 *  subscription - a handle of the subscription where source will be changed
 *  source - source of order to set, 4 symbols maximum length
 */
DXFEED_API ERRORCODE dxf_set_order_source(dxf_subscription_t subscription, const char* source);

/*
 *  Add a new source to subscription
 *  Warning: you must configure order source before dxf_add_symbols/dxf_add_symbol call
 *
 *  subscription - a handle of the subscription where source will be changed
 *  source - source of order event to add, 4 symbols maximum length
 */
DXFEED_API ERRORCODE dxf_add_order_source(dxf_subscription_t subscription, const char* source);

/*
 *	API that allows user to create candle symbol attributes
 *
 *  base_symbol - the symbols to add
 *  exchange_code - exchange attribute of this symbol
 *  period_value -  aggregation period value of this symbol
 *  period_type - aggregation period type of this symbol
 *  price - price type attribute of this symbol
 *  session - session attribute of this symbol
 *  alignment - alignment attribute of this symbol
 *  candle_attributes - pointer to the configured candle attributes struct
 */
DXFEED_API ERRORCODE dxf_create_candle_symbol_attributes(dxf_const_string_t base_symbol,
                                                         dxf_char_t exchange_code,
                                                         dxf_double_t period_value,
                                                         dxf_candle_type_period_attribute_t period_type,
                                                         dxf_candle_price_attribute_t price,
                                                         dxf_candle_session_attribute_t session,
                                                         dxf_candle_alignment_attribute_t alignment,
                                                         OUT dxf_candle_attributes_t* candle_attributes);

/*
 *	Free memory allocated by dxf_initialize_candle_symbol_attributes(...) function
 *
 *  candle_attributes - pointer to the candle attributes struct
 */
DXFEED_API ERRORCODE dxf_delete_candle_symbol_attributes(dxf_candle_attributes_t candle_attributes);

/*
 *  Creates Order snapshot with the specified parameters.
 *
 *  If source is NULL string subscription on Order event will be performed. You can specify order 
 *  source for Order event by passing suffix: "BYX", "BZX", "DEA", "DEX", "ISE", "IST", "NTV".
 *  If source is equal to "COMPOSITE_BID" or "COMPOSITE_ASK" subscription on MarketMaker event will 
 *  be performed.
 *
 *  connection - a handle of a previously created connection which the subscription will be using
 *  symbol - the symbol to add
 *  source - order source for Order event with 4 symbols maximum length OR keyword which can be 
 *           one of COMPOSITE_BID or COMPOSITE_ASK
 *  time - time in the past (unix time in milliseconds)
 *  OUT snapshot - a handle of the created snapshot
 */
DXFEED_API ERRORCODE dxf_create_order_snapshot(dxf_connection_t connection, 
                                               dxf_const_string_t symbol, const char* source,
                                               dxf_long_t time, OUT dxf_snapshot_t* snapshot);

/*
 *  Creates Candle snapshot with the specified parameters.
 *
 *  connection - a handle of a previously created connection which the subscription will be using
 *  candle_attributes - object specified symbol attributes of candle
 *  time - time in the past (unix time in milliseconds)
 *  OUT snapshot - a handle of the created snapshot
 */
DXFEED_API ERRORCODE dxf_create_candle_snapshot(dxf_connection_t connection, 
                                                dxf_candle_attributes_t candle_attributes, 
                                                dxf_long_t time, OUT dxf_snapshot_t* snapshot);

/*
 *  Closes a snapshot.
 *  All the data associated with it will be freed.
 *
 *  snapshot - a handle of the snapshot to close
 */
DXFEED_API ERRORCODE dxf_close_snapshot(dxf_snapshot_t snapshot);

/*
 *  Attaches a listener callback to the snapshot.
 *  This callback will be invoked when the new snapshot arrives or existing updates.
 *  No error occurs if it's attempted to attach the same listener twice or more.
 *
 *  snapshot - a handle of the snapshot to which a listener is to be attached
 *  snapshot_listener - a listener callback function pointer
 */
DXFEED_API ERRORCODE dxf_attach_snapshot_listener(dxf_snapshot_t snapshot, 
                                                  dxf_snapshot_listener_t snapshot_listener,
                                                  void* user_data);

/*
 *  Detaches a listener from the snapshot.
 *  No error occurs if it's attempted to detach a listener which wasn't previously attached.
 *
 *  snapshot - a handle of the snapshot to which a listener is to be detached
 *  snapshot_listener - a listener callback function pointer
 */
DXFEED_API ERRORCODE dxf_detach_snapshot_listener(dxf_snapshot_t snapshot, 
                                                  dxf_snapshot_listener_t snapshot_listener);


#endif /* DXFEED_API_H_INCLUDED */