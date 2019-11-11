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

/**
 * @file
 * @brief dxFeed C API functions declarations
 */

/**
 * @defgroup functions Functions
 * @brief DXFeed C API functions
 */
/**
 * @ingroup functions
 * @defgroup macros Defined macros
 * @brief macros
 */
/**
 * @ingroup functions
 * @defgroup callback_types API Events' Callbacks
 * @brief Event callbacks
 */
/**
 * @ingroup functions
 * @defgroup c-api-event-listener-functions Listeners
 * @brief API event listeners management functions
 */
/**
 * @ingroup functions
 * @defgroup c-api-common Common functions
 * @brief Common API functions
 */
/**
 * @ingroup functions
 * @defgroup c-api-connection-functions Connections
 * @brief Connection establishment/teardown functions
 */
/**
 * @ingroup functions
 * @defgroup c-api-basic-subscription-functions Subscriptions
 * @brief Subscription initiation/closing functions
 */
/**
 * @ingroup functions
 * @defgroup c-api-symbol-subscription-functions Symbols
 * @brief Symbol-related functions
 */
/**
 * @ingroup functions
 * @defgroup c-api-orders Orders
 * @brief Order-related functions
 */
/**
 * @ingroup functions
 * @defgroup c-api-candle-attributes Candle symbol attributes
 * @brief Candle attributes manipulation functions
 */
/**
 * @ingroup functions
 * @defgroup c-api-snapshots Snapshots
 * @brief Snapshot functions
 */
/**
 * @ingroup functions
 * @defgroup c-api-price-level-book Price level books
 * @brief Price level book functions
 */
/**
 * @ingroup functions
 * @defgroup c-api-regional-book Regional books
 * @brief Regional book functions
 */

#ifndef DXFEED_API_H_INCLUDED
#define DXFEED_API_H_INCLUDED

#ifndef DOXYGEN_SHOULD_SKIP_THIS
    #ifdef DXFEED_EXPORTS
        #define DXFEED_API __declspec(dllexport)
    #elif  DXFEED_IMPORTS
        #define DXFEED_API __declspec(dllimport)
    #elif __cplusplus
        #define DXFEED_API extern "C"
    #else
        #define DXFEED_API
    #endif
#endif // DOXYGEN_SHOULD_SKIP_THIS

#ifndef OUT
    #ifndef DOXYGEN_SHOULD_SKIP_THIS
        #define OUT
    #endif // DOXYGEN_SHOULD_SKIP_THIS
#endif /* OUT */

#include "DXTypes.h"
#include "EventData.h"

/* -------------------------------------------------------------------------- */
/*
 *	DXFeed API function return value codes
 */
/* -------------------------------------------------------------------------- */

/**
 * @ingroup  macros
 * @brief Successful API call
 * @details The value is returned on successful API call
 */
#define DXF_SUCCESS 1
/**
 * @ingroup macros
 * @brief Failed API call
 * @details The value is returned on failed API call
 */
#define DXF_FAILURE 0

/* -------------------------------------------------------------------------- */
/*
 *	DXFeed API generic types
 */
/* -------------------------------------------------------------------------- */

/**
 * @ingroup callback_types
 *
 * @brief Connection termination notification callback type
 *
 * @details Called whenever connection to dxFeed servers terminates
 */
typedef void (*dxf_conn_termination_notifier_t) (dxf_connection_t connection, void* user_data);

/**
 * @ingroup callback_types
 *
 * @brief connection Status notification callback type
 *
 * @details Called whenever connection status to dxFeed servers changes
 */
typedef void (*dxf_conn_status_notifier_t) (dxf_connection_t connection,
                                            dxf_connection_status_t old_status,
                                            dxf_connection_status_t new_status,
                                            void* user_data);

/* the low level callback types, required in case some thread-specific initialization must be performed
   on the client side on the thread creation/destruction */
/**
 * @ingroup callback_types
 *
 * @brief The low level callback type, required in case some thread-specific initialization must be performed
 *        on the client side on the thread creation/destruction
 *
 * @details Called whenever connection thread is being created
 */
typedef int (*dxf_socket_thread_creation_notifier_t) (dxf_connection_t connection, void* user_data);

/**
 * @ingroup callback_types
 *
 * @brief The low level callback type, required in case some thread-specific initialization must be performed
 *        on the client side on the thread creation/destruction
 *
 * @details Called whenever connection thread is being terminated
 */
typedef void (*dxf_socket_thread_destruction_notifier_t) (dxf_connection_t connection, void* user_data);

/* -------------------------------------------------------------------------- */
/*
 *	DXFeed C API functions
 
 *  All functions return DXF_SUCCESS on success and DXF_FAILURE if some error
 *  has occurred. Use 'dxf_get_last_error' to retrieve the error code
 *  and description.
 */
/* -------------------------------------------------------------------------- */

/**
 * @ingroup c-api-connection-functions
 *
 * @brief Creates connection with the specified parameters.
 *
 * @details
 *
 * @param[in] address              Connection string
 *                                   - the single address: ```host:port``` or just ```host```
 *                                   - address with credentials: ```host:port[username=xxx,password=yyy]```
 *                                   - multiple addresses: ```(host1:port1)(host2)(host3:port3[username=xxx,password=yyy])```
 *                                   - the data from file: ```/path/to/file``` on *nix and ```drive:\path\to\file```
 *                                     on Windows
 * @param[in] notifier             The callback to inform the client side that the connection has stumbled upon and
 *                                 error and will go reconnecting
 * @param[in] conn_status_notifier The callback to inform the client side that the connection status has changed
 * @param[in] stcn                 The callback for informing the client side about the socket thread creation;
 *                                 may be set to NULL if no specific action is required to perform on the client side
 *                                 on a new thread creation
 * @param[in] shdn                 The callback for informing the client side about the socket thread destruction;
 *                                 may be set to NULL if no specific action is required to perform on the client side
 *                                 on a thread destruction
 * @param[in] user_data            The user defined value passed to the termination notifier callback along with the
 *                                 connection handle; may be set to whatever value
 * @param[out] connection          The handle of the created connection
 *
 * @return DXF_SUCCESS on successful connection establishment or DXF_FAILURE on error.
 *         Created connection is returned via OUT parameter ```connection```.
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure.
 */
DXFEED_API ERRORCODE dxf_create_connection (const char* address,
                                            dxf_conn_termination_notifier_t notifier,
                                            dxf_conn_status_notifier_t conn_status_notifier,
                                            dxf_socket_thread_creation_notifier_t stcn,
                                            dxf_socket_thread_destruction_notifier_t stdn,
                                            void* user_data,
                                            OUT dxf_connection_t* connection);

/**
 * @ingroup c-api-connection-functions
 *
 * @brief Creates connection with the specified parameters and basic authorization.
 *
 * @details
 *
 * @param[in] address              Connection string
 *                                   - the single address: ```host:port``` or just ```host```
 *                                   - address with credentials: ```host:port[username=xxx,password=yyy]```
 *                                   - multiple addresses: ```(host1:port1)(host2)(host3:port3[username=xxx,password=yyy])```
 *                                   - the data from file: ```/path/to/file``` on *nix and ```drive:\path\to\file```
 *                                     on Windows
 * @param[in] user                 The user name;
 * @param[in] password             The user password;
 * @param[in] notifier             The callback to inform the client side that the connection has stumbled upon and
 *                                 error and will go reconnecting
 * @param[in] conn_status_notifier The callback to inform the client side that the connection status has changed
 * @param[in] stcn                 The callback for informing the client side about the socket thread creation;
 *                                 may be set to NULL if no specific action is required to perform on the client side
 *                                 on a new thread creation
 * @param[in] shdn                 The callback for informing the client side about the socket thread destruction;
 *                                 may be set to NULL if no specific action is required to perform on the client side
 *                                 on a thread destruction;
 * @param[in] user_data            The user defined value passed to the termination notifier callback along with the
 *                                 connection handle; may be set to whatever value;
 * @param[out] connection          The handle of the created connection.
 *
 * @return {@link DXF_SUCCESS} on successful connection establishment or {@link DXF_FAILURE} on error;
 *         created connection is returned via OUT parameter *connection*;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure.
 *
 * @warning The user and password data from argument have a higher priority than address credentials.
 */
DXFEED_API ERRORCODE dxf_create_connection_auth_basic(const char* address,
                                                      const char* user,
                                                      const char* password,
                                                      dxf_conn_termination_notifier_t notifier,
                                                      dxf_conn_status_notifier_t conn_status_notifier,
                                                      dxf_socket_thread_creation_notifier_t stcn,
                                                      dxf_socket_thread_destruction_notifier_t stdn,
                                                      void* user_data,
                                                      OUT dxf_connection_t* connection);

/**
 * @ingroup c-api-connection-functions
 *
 * @brief Creates connection with the specified parameters and token authorization.
 *
 * @detials
 *
 * @param[in] address              Connection string
 *                                   - the single address: "host:port" or just "host"
 *                                   - address with credentials: "host:port[username=xxx,password=yyy]"
 *                                   - multiple addresses: "(host1:port1)(host2)(host3:port3[username=xxx,password=yyy])"
 *                                   - the data from file: "/path/to/file" on *nix and "drive:\path\to\file" on Windows
 * @param[in] token                The authorization token;
 * @param[in] notifier             The callback to inform the client side that the connection has stumbled upon and
 *                                 error and will go reconnecting
 * @param[in] conn_status_notifier The callback to inform the client side that the connection status has changed
 * @param[in] stcn                 The callback for informing the client side about the socket thread creation;
 *                                 may be set to NULL if no specific action is required to perform on the client side
 *                                 on a new thread creation
 * @param[in] shdn                 The callback for informing the client side about the socket thread destruction;
 *                                 may be set to NULL if no specific action is required to perform on the client side
 *                                 on a thread destruction;
 * @param[in] user_data            The user defined value passed to the termination notifier callback along with the
 *                                 connection handle; may be set to whatever value;
 * @param[out] connection          The handle of the created connection.
 *
 * @return {@link DXF_SUCCESS} on successful connection establishment or {@link DXF_FAILURE} on error;
 *         created connection is returned via OUT parameter *connection*;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure.
 *
 * @warning The token data from argument have a higher priority than address credentials
 */
DXFEED_API ERRORCODE dxf_create_connection_auth_bearer(const char* address,
                                                       const char* token,
                                                       dxf_conn_termination_notifier_t notifier,
                                                       dxf_conn_status_notifier_t conn_status_notifier,
                                                       dxf_socket_thread_creation_notifier_t stcn,
                                                       dxf_socket_thread_destruction_notifier_t stdn,
                                                       void* user_data,
                                                       OUT dxf_connection_t* connection);

/**
 * @ingroup c-api-connection-functions
 *
 * @brief Creates connection with the specified parameters and custom described authorization.
 *
 * @details
 *
 * @param[in] address              Connection string
 *                                   - the single address: ```host:port``` or just ```host```
 *                                   - address with credentials: ```host:port[username=xxx,password=yyy]```
 *                                   - multiple addresses: ```(host1:port1)(host2)(host3:port3[username=xxx,password=yyy])```
 *                                   - the data from file: ```/path/to/file``` on *nix and ```drive:\path\to\file```
 *                                     on Windows
 * @param[in] authscheme           The authorization scheme
 * @param[in] authdata             The authorization data
 * @param[in] notifier             The callback to inform the client side that the connection has stumbled upon and
 *                                 error and will go reconnecting
 * @param[in] conn_status_notifier The callback to inform the client side that the connection status has changed
 * @param[in] stcn                 The callback for informing the client side about the socket thread creation;
                                   may be set to NULL if no specific action is required to perform on the client side
                                   on a new thread creation
 * @param[in] shdn                 The callback for informing the client side about the socket thread destruction;
                                   may be set to NULL if no specific action is required to perform on the client side
                                   on a thread destruction;
 * @param[in] user_data            The user defined value passed to the termination notifier callback along with the
 *                                 connection handle; may be set to whatever value;
 * @param[out] connection          The handle of the created connection.
 *
 * @return {@link DXF_SUCCESS} on successful connection establishment or {@link DXF_FAILURE} on error;
 *         created connection is returned via OUT parameter *connection*;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure.
 *
 * @warning The authscheme and authdata from argument have a higher priority than address credentials.
 */
DXFEED_API ERRORCODE dxf_create_connection_auth_custom(const char* address,
                                                       const char* authscheme,
                                                       const char* authdata,
                                                       dxf_conn_termination_notifier_t notifier,
                                                       dxf_conn_status_notifier_t conn_status_notifier,
                                                       dxf_socket_thread_creation_notifier_t stcn,
                                                       dxf_socket_thread_destruction_notifier_t stdn,
                                                       void* user_data,
                                                       OUT dxf_connection_t* connection);

/**
 * @ingroup c-api-connection-functions
 *
 * @brief Closes a connection
 *
 * @details
 *
 * @param[in] connection A handle of a previously created connection
 *
 * @return {@link DXF_SUCCESS} on successful connection closure or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure.
 */
DXFEED_API ERRORCODE dxf_close_connection (dxf_connection_t connection);

/**
 * @ingroup c-api-basic-subscription-functions
 *
 * @brief Creates a subscription with the specified parameters.
 *
 * @details
 *
 * @param[in] connection    A handle of a previously created connection which the subscription will be using
 * @param[in] event_types   A bitmask of the subscription event types. See {@link dx_event_id_t} and
 *                          {@link DX_EVENT_BIT_MASK} for information on how to create an event type bitmask
 *
 * @param[out] subscription A handle of the created subscription
 *
 * @return {@link DXF_SUCCESS} on successful subscription creation or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 *         a handle to newly created subscription is returned via ```subscription``` out parameter
 */
DXFEED_API ERRORCODE dxf_create_subscription (dxf_connection_t connection, int event_types,
                                              OUT dxf_subscription_t* subscription);

/**
 * @ingroup c-api-basic-subscription-functions
 *
 * @brief Creates a subscription with the specified parameters.
 *
 * @details
 *
 * @param[in] connection    A handle of a previously created connection which the subscription will be using
 * @param[in] event_types   A bitmask of the subscription event types. See {@link dx_event_id_t} and
 *                          {@link DX_EVENT_BIT_MASK} for information on how to create an event type bitmask
 * @param[in] time          UTC time in the past (unix time in milliseconds)
 * @param[out] subscription A handle of the created subscription
 *
 * @return {@link DXF_SUCCESS} on successful subscription creation or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 *         a handle to newly created subscription is returned via ```subscription``` out parameter
 */
DXFEED_API ERRORCODE dxf_create_subscription_timed(dxf_connection_t connection, int event_types, 
                                                   dxf_long_t time,
                                                   OUT dxf_subscription_t* subscription);

/**
 * @ingroup c-api-basic-subscription-functions
 *
 * @brief Closes a subscription.
 *
 * @details All the data associated with it will be disposed.
 *
 * @param[in] subscription A handle of the subscription to close
 *
 * @return {@link DXF_SUCCESS} on successful subscription closure or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_close_subscription (dxf_subscription_t subscription);

/**
 * @ingroup c-api-symbol-subscription-functions
 *
 * @brief Adds a single symbol to the subscription.
 *
 * @details A wildcard symbol "*" will replace all symbols: there will be an unsubscription from messages on all
 *          current symbols and a subscription to "*". The subscription type will be changed to {@link dx_st_stream}
 *          If there is already a subscription to "*", then nothing will happen
 *
 * @param[in] subscription A handle of the subscription to which a symbol is added
 * @param[in] symbol       The symbol to add
 *
 * @return {@link DXF_SUCCESS} if the operation succeed or {@link DXF_FAILURE} if the operation fails. The error code
 * can be obtained using the function {@link dxf_get_last_error}
 */
DXFEED_API ERRORCODE dxf_add_symbol (dxf_subscription_t subscription, dxf_const_string_t symbol);

/**
 * @ingroup c-api-symbol-subscription-functions
 *
 * @brief Adds several symbols to the subscription.
 *
 * @details No error occurs if the symbol is attempted to add for the second time.
 *          Empty symbols will be ignored. First met the "*" symbol (wildcard) will overwrite all other symbols:
 *          there will be an unsubscription from messages on all current symbols and a subscription to "*". The
 *          subscription type will be changed to {@link dx_st_stream}. If there is already a subscription to "*",
 *          then nothing will happen.
 *
 * @param[in] subscription A handle of the subscription to which the symbols are added
 * @param[in] symbols      The symbols to add
 * @param[in] symbol_count A number of symbols
 *
 * @return {@link DXF_SUCCESS} if the operation succeed or {@link DXF_FAILURE} if the operation fails. The error code
 * can be obtained using the function {@link dxf_get_last_error}
 */
DXFEED_API ERRORCODE dxf_add_symbols (dxf_subscription_t subscription, dxf_const_string_t* symbols, int symbol_count);

/**
 * @ingroup c-api-symbol-subscription-functions
 *
 * @brief Adds a candle symbol to the subscription.
 *
 * @details
 *
 * @param subscription      A handle of the subscription to which a symbol is added
 * @param candle_attributes Pointer to the candle struct
 *
 * @return {@link DXF_SUCCESS} on successful symbol addition or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_add_candle_symbol(dxf_subscription_t subscription, dxf_candle_attributes_t candle_attributes);

/**
 * @ingroup c-api-symbol-subscription-functions
 *
 * @brief Remove a candle symbol from the subscription.
 *
 * @details
 *
 * @param[in] subscription       a handle of the subscription from symbol will be removed
 * @param[in] candle_attributes  pointer to the candle structure
 *
 * @return  {@link DXF_SUCCESS} on successful symbol removal or {@link DXF_FAILURE} on error;
 *          {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_remove_candle_symbol(dxf_subscription_t subscription, dxf_candle_attributes_t candle_attributes);

/**
 * @ingroup c-api-symbol-subscription-functions
 *
 * @brief Removes a single symbol from the subscription.
 *
 * @details A wildcard symbol "*" will remove all symbols: there will be an unsubscription from messages on all current
 *          symbols. If there is already a subscription to "*" and the @a symbol to remove is not a "*", then nothing
 *          will happen.
 *
 * @param[in] subscription A handle of the subscription from which a symbol is removed
 * @param[in] symbol       The symbol to remove
 *
 * @return {@link DXF_SUCCESS} if the operation succeed or {@link DXF_FAILURE} if the operation fails. The error code
 *         can be obtained using the function {@link dxf_get_last_error}
 */
DXFEED_API ERRORCODE dxf_remove_symbol (dxf_subscription_t subscription, dxf_const_string_t symbol);

/**
 * @ingroup c-api-symbol-subscription-functions
 *
 * @brief Removes several symbols from the subscription.
 *
 * @details No error occurs if it's attempted to remove symbols that weren't added beforehand. First met the "*" symbol
 *          (wildcard) will remove all symbols: there will be an unsubscription from messages on all current symbols.
 *          If there is already a subscription to "*" and the @a symbols to remove are not contain a "*", then nothing
 *          will happen.
 *
 * @param[in] subscription A handle of the subscription from which symbols are removed
 * @param[in] symbols      The symbols to remove
 * @param[in] symbol_count A number of symbols
 *
 * @return {@link DXF_SUCCESS} if the operation succeed or {@link DXF_FAILURE} if the operation fails. The error code
 *         can be obtained using the function {@link dxf_get_last_error}
 */
DXFEED_API ERRORCODE dxf_remove_symbols (dxf_subscription_t subscription, dxf_const_string_t* symbols, int symbol_count);

/**
 * @ingroup c-api-symbol-subscription-functions
 *
 * @brief Retrieves the list of symbols currently added to the subscription.
 *
 * @detial The memory for the resulting list is allocated internally, so no actions to free it are required.
 *         The symbol name buffer is guaranteed to be valid until either the subscription symbol list is changed or
 *         a new call of this function is performed.
 *
 * @param[in] subscription  A handle of the subscriptions whose symbols are to retrieve
 * @param[out] symbols      A pointer to the string array object to which the symbol list is to be stored
 * @param[out] symbol_count A pointer to the variable to which the symbol count is to be stored
 *
 * @return {@link DXF_SUCCESS} on successful symbols retrieval or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_get_symbols (dxf_subscription_t subscription, OUT dxf_const_string_t** symbols, OUT int* symbol_count);

/**
 * @ingroup c-api-symbol-subscription-functions
 *
 * @brief Sets the symbols for the subscription.
 *
 * @details The difference between this function and {@link dxf_add_symbols} is that all the previously added symbols
 *          that do not belong to the symbol list passed to this function will be removed.
 *
 * @param[in] subscription A handle of the subscription whose symbols are to be set
 * @param[in] symbols      The symbol list to set
 * @param[in] symbol_count The symbol count
 *
 * @return {@link DXF_SUCCESS} if the operation succeed or {@link DXF_FAILURE} if the operation fails. The error code
 *         can be obtained using the function {@link dxf_get_last_error}
 */
DXFEED_API ERRORCODE dxf_set_symbols (dxf_subscription_t subscription, dxf_const_string_t* symbols, int symbol_count);

/**
 * @ingroup c-api-symbol-subscription-functions
 *
 * @brief Removes all the symbols from the subscription.
 *
 * @details
 *
 * @param[in] subscription A handle of the subscription whose symbols are to be cleared
 *
 * @return {@link DXF_SUCCESS} if the operation succeed or {@link DXF_FAILURE} if the operation fails. The error code
 *         can be obtained using the function {@link dxf_get_last_error}
 */
DXFEED_API ERRORCODE dxf_clear_symbols (dxf_subscription_t subscription);

/**
 * @ingroup c-api-event-listener-functions
 *
 * @brief Attaches a listener callback to the subscription.
 *
 * @details This callback will be invoked when the new event data for the subscription symbols arrives.
 *          No error occurs if it's attempted to attach the same listener twice or more.

 * @param[in] subscription   A handle of the subscription to which a listener is to be attached
 * @param[in] event_listener A listener callback function pointer
 * @param[in] user_data      Data to be passed to the callback function
 *
 * @return {@link DXF_SUCCESS} on successful event listener attachment or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_attach_event_listener (dxf_subscription_t subscription, dxf_event_listener_t event_listener,
                                                void* user_data);

/**
 * @ingroup c-api-event-listener-functions
 *
 * @brief Detaches a listener from the subscription.
 *
 * @details No error occurs if it's attempted to detach a listener which wasn't previously attached.
 *
 * @param[in] subscription   A handle of the subscription from which a listener is to be detached
 * @param[in] event_listener A listener callback function pointer
 *
 * @return {@link DXF_SUCCESS} on successful event listener detachment or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_detach_event_listener (dxf_subscription_t subscription, dxf_event_listener_t event_listener);

/**
 * @ingroup c-api-event-listener-functions
 *
 * @brief Attaches a extended listener callback to the subscription.
 *
 * @details This callback will be invoked when the new event data for the subscription symbols arrives.
 *          No error occurs if it's attempted to attach the same listener twice or more.
 *          This listener differs with extend number of callback parameters.
 *
 * @param[in] subscription   A handle of the subscription to which a listener is to be attached
 * @param[in] event_listener A listener callback function pointer
 * @param[in] user_data      Data to be passed to the callback function; if there isn't user data pass NULL
 *
 * @return {@link DXF_SUCCESS} on successful event listener attachment or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_attach_event_listener_v2(dxf_subscription_t subscription, 
                                                  dxf_event_listener_v2_t event_listener,
                                                  void* user_data);

/**
 * @ingroup c-api-event-listener-functions
 *
 * @brief Detaches a extended listener from the subscription.
 *
 * @details No error occurs if it's attempted to detach a listener which wasn't previously attached.
 *
 * @param[in] subscription   A handle of the subscription from which a listener is to be detached
 * @param[in] event_listener A listener callback function pointer
 *
 * @return {@link DXF_SUCCESS} on successful event listener detachment or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_detach_event_listener_v2(dxf_subscription_t subscription, 
                                                  dxf_event_listener_v2_t event_listener);

/**
 * @ingroup c-api-event-listener-functions
 *
 * @brief Retrieves the subscription event types.
 *
 * @details
 *
 * @param[in] subscription A handle of the subscription whose event types are to be retrieved
 * @param[out] event_types A pointer to the variable to which the subscription event types bitmask is to be stored
 *
 * @return {@link DXF_SUCCESS} on successful event types retrieval or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_get_subscription_event_types (dxf_subscription_t subscription, OUT int* event_types);

/**
 * @ingroup c-api-event-listener-functions
 *
 * @brief Retrieves the last event data of the specified symbol and type for the connection.
 *
 * @details
 *
 * @param[in] connection  A handle of the connection whose data is to be retrieved
 * @param[in] event_type  An event type bitmask defining a single event type
 * @param[in] symbol      A symbol name
 * @param[out] event_data A pointer to the variable to which the last data buffer pointer is stored; if there was no
 *                        data for this connection/event type/symbol, NULL will be stored
 *
 * @return {@link DXF_SUCCESS} on successful event retrieval or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_get_last_event (dxf_connection_t connection, int event_type, dxf_const_string_t symbol,
                                         OUT dxf_event_data_t* event_data);
/**
 * @ingroup c-api-common
 *
 * @brief Retrieves the last error info.
 *
 *
 * @details The error is stored on per-thread basis. If the connection termination notifier callback was invoked, then
 *          to retrieve the connection's error code call this function right from the callback function.
 *          If an error occurred within the error storage subsystem itself, the function will return DXF_FAILURE.
 *
 * @param[out] error_code  A pointer to the variable where the error code is to be stored
 * @param[out] error_descr A pointer to the variable where the human-friendly error description is to be stored;
 *                         may be NULL if the text representation of error is not required
 * @return {@link DXF_SUCCESS} on successful error retrieval or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 *         @a error_code and @a error_descr are returned as output parameters of this function
 */
DXFEED_API ERRORCODE dxf_get_last_error (OUT int* error_code, OUT dxf_const_string_t* error_descr);

/**
 * @ingroup c-api-common
 *
 * @brief Initializes the internal logger.
 *
 * @details Various actions and events, including the errors, are being logged
 *          throughout the library. They may be stored into the file.
 *
 * @param[in] file_name          A full path to the file where the log is to be stored
 * @param[in] rewrite_file       A flag defining the file open mode; if it's nonzero then the log file will be rewritten
 * @param[in] show_timezone_info A flag defining the time display option in the log file; if it's nonzero then
 *                               the time will be displayed with the timezone suffix
 * @param[in] verbose            A flag defining the logging mode; if it's nonzero then the verbose logging will be
 *                               enabled
 *
 * @return {@link DXF_SUCCESS} on successful logger initialization or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_initialize_logger (const char* file_name, int rewrite_file, int show_timezone_info, int verbose);

/**
 * @ingroup c-api-orders
 *
 * @brief Clear current sources and add new one to subscription
 *
 * @details
 *
 * @warning You must configure order source before {@link dxf_add_symbols}/{@link dxf_add_symbol} call
 *
 * @param[in] subscription A handle of the subscription where source will be changed
 * @param[in] source       Source of order to set, 4 symbols maximum length
 *
 * @return {@link DXF_SUCCESS} on order source setup or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_set_order_source(dxf_subscription_t subscription, const char* source);

/**
 * @ingroup c-api-orders
 *
 * @brief Add a new source to subscription
 *
 * @details
 *
 * @warning You must configure order source before {@link dxf_add_symbols}/{@link dxf_add_symbol} call
 *
 * @param[in] subscription A handle of the subscription where source will be changed
 * @param[in] source       Source of order event to add, 4 symbols maximum length
 *
 * @return {@link DXF_SUCCESS} if order has been successfully added or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_add_order_source(dxf_subscription_t subscription, const char* source);

/**
 * @ingroup c-api-candle-attributes
 *
 * @brief API that allows user to create candle symbol attributes
 *
 * @details
 *
 * @param[in] base_symbol        The symbols to add
 * @param[in] exchange_code      Exchange attribute of this symbol
 * @param[in] period_value       Aggregation period value of this symbol
 * @param[in] period_type        Aggregation period type of this symbol
 * @param[in] price              Price type attribute of this symbol
 * @param[in] session            Session attribute of this symbol
 * @param[in] alignment          Alignment attribute of this symbol
 * @param[out] candle_attributes Pointer to the configured candle attributes struct
 *
 * \return {@link DXF_SUCCESS} if candle attributes have been created successfully or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 *         *candle_attributes* are returned via output parameter
 */
DXFEED_API ERRORCODE dxf_create_candle_symbol_attributes(dxf_const_string_t base_symbol,
                                                         dxf_char_t exchange_code,
                                                         dxf_double_t period_value,
                                                         dxf_candle_type_period_attribute_t period_type,
                                                         dxf_candle_price_attribute_t price,
                                                         dxf_candle_session_attribute_t session,
                                                         dxf_candle_alignment_attribute_t alignment,
                                                         OUT dxf_candle_attributes_t* candle_attributes);

/**
 * @ingroup c-api-candle-attributes
 *
 * @brief Free memory allocated by dxf_initialize_candle_symbol_attributes(...) function
 *
 * @details
 *
 * @param[in] candle_attributes Pointer to the candle attributes struct
 */
DXFEED_API ERRORCODE dxf_delete_candle_symbol_attributes(dxf_candle_attributes_t candle_attributes);

/**
 * @ingroup c-api-snapshots
 *
 * @brief Creates snapshot with the specified parameters.
 *
 * @details
 *          For Order or Candle events (dx_eid_order or dx_eid_candle) please use
 *          short form of this function: dxf_create_order_snapshot or dxf_create_candle_snapshot respectively.
 *
 *          For order events (event_id is 'dx_eid_order')
 *          If source is NULL string subscription on Order event will be performed. You can specify order
 *          source for Order event by passing suffix: "BYX", "BZX", "DEA", "DEX", "ISE", "IST", "NTV".
 *          If source is equal to "COMPOSITE_BID" or "COMPOSITE_ASK" subscription on MarketMaker event will
 *          be performed. For other events source parameter does not matter.
 *
 * @param[in] connection A handle of a previously created connection which the subscription will be using
 * @param[in] event_id   Single event id. Next events is supported: dxf_eid_order, dxf_eid_candle,
 *                       dx_eid_spread_order, dx_eid_time_and_sale, dx_eid_greeks, dx_eid_series.
 * @param[in] symbol     The symbol to add.
 * @param[in] source     Order source for Order, which can be one of following: "BYX", "BZX", "DEA", "DEX",
 *                       "ISE", "IST", "NTV". For MarketMaker subscription use "COMPOSITE_BID" or
 *                       "COMPOSITE_ASK" keyword.
 * @param[in] time       Time in the past (unix time in milliseconds).
 * @param[out] snapshot  A handle of the created snapshot
 *
 * @return {@link DXF_SUCCESS} if snapshot has been successfully created or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 *         newly created snapshot is return via *snapshot* output parameter
 */
DXFEED_API ERRORCODE dxf_create_snapshot(dxf_connection_t connection, dx_event_id_t event_id,
                                         dxf_const_string_t symbol, const char* source, 
                                         dxf_long_t time, OUT dxf_snapshot_t* snapshot);

/**
 * @ingroup c-api-snapshots
 *
 * @brief Creates Order snapshot with the specified parameters.
 *
 * @details If source is NULL string subscription on Order event will be performed. You can specify order
 *          source for Order event by passing suffix: "BYX", "BZX", "DEA", "DEX", "ISE", "IST", "NTV".
 *          If source is equal to "COMPOSITE_BID" or "COMPOSITE_ASK" subscription on MarketMaker event will
 *          be performed.
 *
 * @param[in] connection A handle of a previously created connection which the subscription will be using
 * @param[in] symbol     The symbol to add
 * @param[in] source     Order source for Order, which can be one of following: "BYX", "BZX", "DEA", "DEX",
 *                       "ISE", "IST", "NTV". For MarketMaker subscription use "COMPOSITE_BID" or
 *                       "COMPOSITE_ASK" keyword.
 * @param[in] time       Time in the past (unix time in milliseconds)
 * @param[out] snapshot  A handle of the created snapshot
 *
 * @return {@link DXF_SUCCESS} if order snapshot has been successfully created or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 *         newly created snapshot is return via *snapshot* output parameter
 */
DXFEED_API ERRORCODE dxf_create_order_snapshot(dxf_connection_t connection, 
                                               dxf_const_string_t symbol, const char* source,
                                               dxf_long_t time, OUT dxf_snapshot_t* snapshot);

/**
 * @ingroup c-api-snapshots
 *
 * @brief Creates Candle snapshot with the specified parameters.
 *
 * @details
 *
 * @param[in] connection        A handle of a previously created connection which the subscription will be using
 * @param[in] candle_attributes Object specified symbol attributes of candle
 * @param[in] time              Time in the past (unix time in milliseconds)
 * @param[out] snapshot         A handle of the created snapshot
 *
 * @return {@link DXF_SUCCESS} if candle snapshot has been successfully created or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 *         newly created snapshot is return via *snapshot* output parameter
 */
DXFEED_API ERRORCODE dxf_create_candle_snapshot(dxf_connection_t connection, 
                                                dxf_candle_attributes_t candle_attributes, 
                                                dxf_long_t time, OUT dxf_snapshot_t* snapshot);

/**
 * @ingroup c-api-snapshots
 *
 * @brief Closes a snapshot.
 *
 * @details All the data associated with it will be freed.
 *
 * @param[in] snapshot A handle of the snapshot to close
 *
 * @return {@link DXF_SUCCESS} if snapshot has been closed successfully or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_close_snapshot(dxf_snapshot_t snapshot);

/**
 * @ingroup c-api-snapshots
 *
 * @brief Attaches a listener callback to the snapshot.
 *
 * @details This callback will be invoked when the new snapshot arrives or existing updates.
 *          No error occurs if it's attempted to attach the same listener twice or more.
 *
 * @param[in] snapshot          A handle of the snapshot to which a listener is to be attached
 * @param[in] snapshot_listener A listener callback function pointer
 * @param[in] user_data         Data to be passed to the callback function
 *
 * @return {@link DXF_SUCCESS} if snapshot listener has been successfully attached or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_attach_snapshot_listener(dxf_snapshot_t snapshot, 
                                                  dxf_snapshot_listener_t snapshot_listener,
                                                  void* user_data);

/**
 * @ingroup c-api-snapshots
 *
 * @brief Detaches a listener from the snapshot.
 *
 * @details No error occurs if it's attempted to detach a listener which wasn't previously attached.
 *
 * @param[in] snapshot          A handle of the snapshot to which a listener is to be detached
 * @param[in] snapshot_listener A listener callback function pointer
 *
 * @return {@link DXF_SUCCESS} if snapshot listener has been successfully detached or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_detach_snapshot_listener(dxf_snapshot_t snapshot, 
                                                  dxf_snapshot_listener_t snapshot_listener);

/**
 * @ingroup c-api-snapshots
 *
 * @brief Attaches an incremental listener callback to the snapshot.
 *
 * @details This callback will be invoked when the new snapshot arrives or existing updates.
 *          No error occurs if it's attempted to attach the same listener twice or more.
 *
 * @param[in] snapshot          A handle of the snapshot to which a listener is to be attached
 * @param[in] snapshot_listener A listener callback function pointer
 * @param[in] user_data         Data to be passed to the callback function
 *
 * @return {@link DXF_SUCCESS} if listener has been successfully attached or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_attach_snapshot_inc_listener(dxf_snapshot_t snapshot, 
                                                  dxf_snapshot_inc_listener_t snapshot_listener,
                                                  void* user_data);

/**
 * @ingroup c-api-snapshots
 *
 * @brief Detaches a listener from the snapshot.
 *
 * @details No error occurs if it's attempted to detach a listener which wasn't previously attached.
 *
 * @param[in] snapshot          A handle of the snapshot to which a listener is to be detached
 * @param[in] snapshot_listener A listener callback function pointer
 *
 * @return {@link DXF_SUCCESS} if snapshot listener has been successfully detached or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_detach_snapshot_inc_listener(dxf_snapshot_t snapshot, 
                                                  dxf_snapshot_inc_listener_t snapshot_listener);

/**
 * @ingroup c-api-snapshots
 *
 * @brief Retrieves the symbol currently added to the snapshot subscription.
 *
 * @details The memory for the resulting symbol is allocated internally, so no actions to free it are required.
 *
 * @param[in] snapshot A handle of the snapshot to which a listener is to be detached
 * @param[out] symbol  A pointer to the string object to which the symbol is to be stored
 *
 * @return {@link DXF_SUCCESS} if snapshot symbol has been successfully received or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 *         *symbol* itself is returned via out parameter
*/
DXFEED_API ERRORCODE dxf_get_snapshot_symbol(dxf_snapshot_t snapshot, OUT dxf_string_t* symbol);

/**
 * @ingroup c-api-price-level-book
 *
 * @brief Creates Price Level book with the specified parameters.
 *
 * @details
 *
 * @param[in] connection A handle of a previously created connection which the subscription will be using
 * @param[in] symbol     The symbol to use
 * @param[in] sources    Order sources for Order, NULL-terminated list. Each element can be one of following:
 *                       "BYX", "BZX", "DEA", "DEX", "ISE", "IST", "NTV".
 * @param[out] book      A handle of the created price level book
 *
 * @return {@link DXF_SUCCESS} if price level book has been successfully created or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 *         *price level book* itself is returned via out parameter
 */
DXFEED_API ERRORCODE dxf_create_price_level_book(dxf_connection_t connection, 
                                                 dxf_const_string_t symbol, const char** sources,
                                                 OUT dxf_price_level_book_t* book);

/**
 * @ingroup c-api-price-level-book
 *
 * @brief Closes a price level book.
 *
 * @details All the data associated with it will be freed.
 *
 * @param book A handle of the price level book to close
 *
 * @return {@link DXF_SUCCESS} if price level book has been successfully closed or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_close_price_level_book(dxf_price_level_book_t book);

/**
 * @ingroup c-api-price-level-book
 *
 * @brief Attaches a listener callback to the price level book.
 *
 * @details This callback will be invoked when price levels change.
 *          No error occurs if it's attempted to attach the same listener twice or more.
 *
 * @param[in] book          A handle of the book to which a listener is to be detached
 * @param[in] book_listener A listener callback function pointer
 * @param[in] user_data     Data to be passed to the callback function
 *
 * @return {@link DXF_SUCCESS} if listener has been successfully attached or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_attach_price_level_book_listener(dxf_price_level_book_t book, 
                                                          dxf_price_level_book_listener_t book_listener,
                                                          void* user_data);

/**
 * @ingroup c-api-price-level-book
 *
 * @brief Detaches a listener from the snapshot.
 *
 * @details No error occurs if it's attempted to detach a listener which wasn't previously attached.
 *
 * @param[in] book          A handle of the book to which a listener is to be detached
 * @param[in] book_listener A listener callback function pointer
 *
 * @return {@link DXF_SUCCESS} if listener has been successfully detached or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_detach_price_level_book_listener(dxf_price_level_book_t book, 
                                                          dxf_price_level_book_listener_t book_listener);

/**
 * @ingroup c-api-regional-book
 *
 * @brief Creates Regional book with the specified parameters.
 *
 * @details Regional book is like Price Level Book but uses regional data instead of full depth order book.
 *
 * @param[in] connection A handle of a previously created connection which the subscription will be using
 * @param[in] symbol     The symbol to use
 * @param[out] book      A handle of the created regional book
 *
 * @return {@link DXF_SUCCESS} if regional book has been successfully created or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 *         *regional book* itself is returned via out parameter
 */
DXFEED_API ERRORCODE dxf_create_regional_book(dxf_connection_t connection, 
                                              dxf_const_string_t symbol,
                                              OUT dxf_regional_book_t* book);

/**
 * @ingroup c-api-regional-book
 *
 * @brief Closes a regional book.
 *
 * @details All the data associated with it will be freed.
 *
 * @param[in] book A handle of the price level book to close
 *
 * @return {@link DXF_SUCCESS} if regional book has been successfully closed or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_close_regional_book(dxf_regional_book_t book);

/**
 * @ingroup c-api-regional-book
 *
 * @brief Attaches a listener callback to regional book.
 *
 * @details This callback will be invoked when price levels created from regional data change.
 *
 * @param[in] book          A handle of the book to which a listener is to be detached
 * @param[in] book_listener A listener callback function pointer
 * @param[in] user_data     Data to be passed to the callback function
 *
 * @return {@link DXF_SUCCESS} if listener has been successfully attached or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_attach_regional_book_listener(dxf_regional_book_t book, 
                                                       dxf_price_level_book_listener_t book_listener,
                                                       void* user_data);

/**
 * @ingroup c-api-regional-book
 *
 * @brief Detaches a listener from the regional book.
 *
 * @details No error occurs if it's attempted to detach a listener which wasn't previously attached.
 *
 * @param book          A handle of the book to which a listener is to be detached
 * @param book_listener A listener callback function pointer
 *
 * @return {@link DXF_SUCCESS} if listener has been successfully detached or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_detach_regional_book_listener(dxf_regional_book_t book, 
                                                       dxf_price_level_book_listener_t book_listener);

/**
 * @ingroup c-api-regional-book
 *
 * @brief Attaches a listener callback to regional book.
 *
 * @details This callback will be invoked when new regional quotes are received.
 *
 * @param[in] book      A handle of the book to which a listener is to be detached
 * @param[in] listener  A listener callback function pointer
 * @param[in] user_data Data to be passed to the callback function
 *
 * @return {@link DXF_SUCCESS} if listener has been successfully attached or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_attach_regional_book_listener_v2(dxf_regional_book_t book,
                                                          dxf_regional_quote_listener_t listener,
                                                          void* user_data);

/**
 * @ingroup c-api-regional-book
 *
 * @brief Detaches a listener from the regional book.
 *
 * @details No error occurs if it's attempted to detach a listener which wasn't previously attached.
 *
 * @param[in] book          A handle of the book to which a listener is to be detached
 * @param[in] book_listener A listener callback function pointer
 *
 * @return {@link DXF_SUCCESS} if listener has been successfully detached or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_detach_regional_book_listener_v2(dxf_regional_book_t book,
                                                          dxf_regional_quote_listener_t listener);

/**
 * @ingroup c-api-common
 *
 * @brief Add dumping of incoming traffic into specific file
 *
 * @details
 *
 * @param[in] connection    A handle of a previously created connection which the subscription will be using
 * @param[in] raw_file_name Raw data file name
 *
 */
DXFEED_API ERRORCODE dxf_write_raw_data(dxf_connection_t connection, const char* raw_file_name);

/**
 * @ingroup c-api-connection-functions
 *
 * @brief Retrieves the array of key-value pairs (properties) for specified connection.
 *
 * @details The memory for the resulting array is allocated during execution of the function
 *          and SHOULD be free by caller with dxf_free_connection_properties_snapshot
 *          function. So done because connection properties can be changed during reconnection.
 *          Returned array is a snapshot of properties at the moment of the call.
 *
 * @param[in] connection  A handle of a previously created connection
 * @param[out] properties Address of pointer to store address of key-value pairs array
 * @param[out] count      Address of variable to store length of key-value pairs array
 *
 * @return {@link DXF_SUCCESS} if connection properties have been successfully received or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 *         *properties* are returned via output parameter
 */
DXFEED_API ERRORCODE dxf_get_connection_properties_snapshot(dxf_connection_t connection,
                                                            OUT dxf_property_item_t** properties,
                                                            OUT int* count);

/**
 * @ingroup c-api-connection-functions
 *
 * @brief Frees memory allocated during {@link dxf_get_connection_properties_snapshot} function execution
 *
 * @details
 *
 * @param[in] properties Pointer to the key-value pairs array
 * @param[in] count      Length of key-value pairs array
 *
 * @return {@link DXF_SUCCESS} if memory has been successfully freed or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 */
DXFEED_API ERRORCODE dxf_free_connection_properties_snapshot(dxf_property_item_t* properties, int count);

/**
 * @ingroup c-api-connection-functions
 *
 * @brief Retrieves the null-terminated string with current connected address in format <host>:<port>.
 *
 * @details If (*address) points to NULL then connection is not connected (reconnection, no valid addresses,
 *          closed connection and others). The memory for the resulting string is allocated during execution
 *          of the function and SHOULD be free by caller with call of dxf_free function. So done because inner
 *          string with connected address can be free during reconnection.
 *
 * @param[in] connection A handle of a previously created connection
 * @param[out] address   Address of pointer to store address of the null-terminated string with current connected address
 *
 * @return {@link DXF_SUCCESS} if address has been successfully received or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 *         *address* itself is returned via out parameter
 */
DXFEED_API ERRORCODE dxf_get_current_connected_address(dxf_connection_t connection, OUT char** address);

/**
 * @ingroup c-api-connection-functions
 *
 * @brief Retrieves the current connection status
 *
 * @details
 *
 * @param[in] connection A handle of a previously created connection
 * @param[out] status    Connection status
 *
 * @return {@link DXF_SUCCESS} if connection status has been successfully received or {@link DXF_FAILURE} on error;
 *         {@link dxf_get_last_error} can be used to retrieve the error code and description in case of failure;
 *         *status* itself is returned via out parameter
 */
DXFEED_API ERRORCODE dxf_get_current_connection_status(dxf_connection_t connection, OUT dxf_connection_status_t* status);

/**
 * @ingroup c-api-common
 *
 * @brief Frees memory allocated in API functions from this module
 *
 * @details
 *
 * @param[in] pointer Pointer to memory allocated earlier in some API function from this module
 */
DXFEED_API ERRORCODE dxf_free(void* pointer);

#endif /* DXFEED_API_H_INCLUDED */
