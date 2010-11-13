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

/*
 *	Implementation of the network functions
 */

#include "DXNetwork.h"
#include "DXSockets.h"
#include "DXErrorHandling.h"
#include "DXThreads.h"
#include "DXMemory.h"
#include "DXErrorCodes.h"
#include "DXAlgorithms.h"
#include "ConnectionContextData.h"
#include "EventSubscription.h"
#include "ClientMessageProcessor.h"
#include "ServerMessageProcessor.h"

/* -------------------------------------------------------------------------- */
/*
 *	Internal structures and types
 */
/* -------------------------------------------------------------------------- */

typedef struct addrinfo* dx_addrinfo_ptr;

typedef struct {
    dx_addrinfo_ptr* elements;
    int size;
    int capacity;
    
    dx_addrinfo_ptr* elements_to_free;
    int elements_to_free_count;
    
    int last_resolution_timestamp;
    int cur_addr_index;
} dx_address_resolution_context_t;

typedef struct {
    const char* host;
    const char* port;
} dx_address_t;

typedef struct {
    dx_address_t* elements;
    int size;
    int capacity;
} dx_address_array_t;

/* -------------------------------------------------------------------------- */
/*
 *	Network connection context
 */
/* -------------------------------------------------------------------------- */

typedef struct {
    dxf_connection_t connection;
    
    dx_connection_context_data_t context_data;
    dx_address_resolution_context_t addr_context;
    
    const char* address;
    dx_socket_t s;
    pthread_t reader_thread;
    pthread_t queue_thread;
    pthread_mutex_t socket_guard;
    
    bool reader_thread_termination_trigger;
    bool queue_thread_termination_trigger;
    bool reader_thread_state;
    bool queue_thread_state;
    dx_error_code_t queue_thread_error;
    
    dx_task_queue_t tq;
    
    int set_fields_flags;
} dx_network_connection_context_t;

#define SOCKET_FIELD_FLAG           (1 << 0)
#define READER_THREAD_FIELD_FLAG    (1 << 1)
#define MUTEX_FIELD_FLAG            (1 << 2)
#define TASK_QUEUE_FIELD_FLAG       (1 << 3)
#define QUEUE_THREAD_FIELD_FLAG     (1 << 4)

/* -------------------------------------------------------------------------- */

bool dx_clear_connection_data (dx_network_connection_context_t* context);

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_network) {
    dx_network_connection_context_t* context = NULL;
    
    CHECKED_CALL_0(dx_on_connection_created);
    
    context = dx_calloc(1, sizeof(dx_network_connection_context_t));

    if (context == NULL) {
        return false;
    }
    
    context->connection = connection;
    context->queue_thread_state = true;

    if (!(dx_create_task_queue(&(context->tq)) && (context->set_fields_flags |= TASK_QUEUE_FIELD_FLAG)) ||
        !dx_set_subsystem_data(connection, dx_ccs_network, context)) {
        dx_clear_connection_data(context);

        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_close_socket (dx_network_connection_context_t* context);

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_network) {
    bool res = true;
    dx_network_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_network, &res);

    if (context == NULL) {
        return dx_on_connection_destroyed() && res;
    }

    if (IS_FLAG_SET(context->set_fields_flags, QUEUE_THREAD_FIELD_FLAG)) {
        context->queue_thread_termination_trigger = true;
        
        res = dx_wait_for_thread(context->queue_thread, NULL) && res;
    }
    
    if (IS_FLAG_SET(context->set_fields_flags, READER_THREAD_FIELD_FLAG)) {
        context->reader_thread_termination_trigger = true;
        
        res = dx_close_socket(context) && res;
        res = dx_wait_for_thread(context->reader_thread, NULL) && res;
    }

    res = dx_clear_connection_data(context) && res;
    res = dx_on_connection_destroyed() && res;

    return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	Common data
 */
/* -------------------------------------------------------------------------- */

#define READ_CHUNK_SIZE 1024

/* -------------------------------------------------------------------------- */
/*
 *	Helper functions
 */
/* -------------------------------------------------------------------------- */

void dx_clear_addr_context_data (dx_address_resolution_context_t* context_data) {
    if (context_data->elements_to_free != NULL) {
        int i = 0;
        
        for (; i < context_data->elements_to_free_count; ++i) {
            dx_freeaddrinfo(context_data->elements_to_free[i]);
        }
        
        dx_free(context_data->elements);
        dx_free(context_data->elements_to_free);
        
        context_data->elements = NULL;
        context_data->elements_to_free = NULL;
        context_data->size = 0;
        context_data->capacity = 0;
        context_data->elements_to_free_count = 0;
        
        context_data->cur_addr_index = 0;
    }
}

/* -------------------------------------------------------------------------- */

bool dx_clear_connection_data (dx_network_connection_context_t* context) {
    int set_fields_flags = 0;
    bool success = true;
    
    if (context == NULL) {
        return true;
    }
    
    dx_clear_addr_context_data(&(context->addr_context));
    
    set_fields_flags = context->set_fields_flags;

    if (IS_FLAG_SET(set_fields_flags, SOCKET_FIELD_FLAG)) {
        success = dx_close(context->s) && success;
    }
    
    if (IS_FLAG_SET(set_fields_flags, MUTEX_FIELD_FLAG)) {
        success = dx_mutex_destroy(&(context->socket_guard)) && success;
    }
    
    if (IS_FLAG_SET(set_fields_flags, READER_THREAD_FIELD_FLAG)) {
        success = dx_close_thread_handle(context->reader_thread) && success;
    }
    
    if (IS_FLAG_SET(set_fields_flags, TASK_QUEUE_FIELD_FLAG)) {
        success = dx_destroy_task_queue(context->tq) && success;
    }
    
    CHECKED_FREE(context->address);
    
    dx_free(context);
    
    return success;
}

/* -------------------------------------------------------------------------- */

static bool dx_close_socket (dx_network_connection_context_t* context) {
    bool res = true;
    
    if (!IS_FLAG_SET(context->set_fields_flags, SOCKET_FIELD_FLAG)) {
        return res;
    }
        
    CHECKED_CALL(dx_mutex_lock, &(context->socket_guard));
    
    res = dx_close(context->s);
    context->set_fields_flags &= ~SOCKET_FIELD_FLAG;
    
    return dx_mutex_unlock(&(context->socket_guard)) && res;
}

/* -------------------------------------------------------------------------- */

void dx_notify_conn_termination (dx_network_connection_context_t* context, OUT bool* idle_thread_flag) {
    if (context->context_data.notifier != NULL) {
        context->context_data.notifier(context->connection);
    }
    
    if (idle_thread_flag != NULL) {
        *idle_thread_flag = true;
    }

    dx_close_socket(context);
}

/* -------------------------------------------------------------------------- */

void* dx_queue_executor (void* arg) {
    static int s_idle_timeout = 100;
    static int s_small_timeout = 25;
    
    dx_network_connection_context_t* context = NULL;
    
    if (arg == NULL) {
        /* invalid input parameter
           but we cannot report it anywhere so simply exit */
        dx_set_error_code(dx_ec_invalid_func_param_internal);

        return NULL;
    }
    
    context = (dx_network_connection_context_t*)arg;
    
    if (!dx_init_error_subsystem()) {
        context->queue_thread_error = dx_ec_error_subsystem_failure;
        context->queue_thread_state = false;
        
        return NULL;
    }
    
    for (;;) {
        bool queue_empty = true;
        
        if (context->queue_thread_termination_trigger) {
            break;
        }
        
        if (!context->reader_thread_state || !context->queue_thread_state) {
            dx_sleep(s_idle_timeout);
            
            continue;
        }
        
        if (!dx_is_queue_empty(context->tq, &queue_empty)) {
            context->queue_thread_error = dx_get_error_code();
            context->queue_thread_state = false;
            
            continue;
        }
        
        if (queue_empty) {
            dx_sleep(s_idle_timeout);
            
            continue;
        }
        
        if (!dx_execute_task_queue(context->tq)) {
            context->queue_thread_error = dx_get_error_code();
            context->queue_thread_state = false;

            continue;
        }
        
        dx_sleep(s_small_timeout);
    }
    
    return NULL;
}

/* -------------------------------------------------------------------------- */

bool dx_reestablish_connection (dx_network_connection_context_t* context);

void* dx_socket_reader (void* arg) {
    dx_network_connection_context_t* context = NULL;
    dx_connection_context_data_t* context_data = NULL;
    bool is_thread_idle = false;
    
    char read_buf[READ_CHUNK_SIZE];
    int number_of_bytes_read = 0;
    
    if (arg == NULL) {
        /* invalid input parameter
           but we cannot report it anywhere so simply exit */
        dx_set_error_code(dx_ec_invalid_func_param_internal);
           
        return NULL;
    }
    
    context = (dx_network_connection_context_t*)arg;
    context_data = &(context->context_data);
        
    if (!dx_init_error_subsystem()) {
        /* the error subsystem is broken, aborting the thread */
        
        context_data->notifier(context->connection);
        
        return NULL;
    }
    
    /*
     *	That's an important initialization. Initially, this flag is false, to indicate that
     *  the socket reader thread isn't initialized. And only when it's ready to work, this flag
     *  is set to true.
     *  This also prevents the queue thread from executing the task queue before the socket reader is ready.
     */
    context->reader_thread_state = true;
    
    for (;;) {
        if (context->reader_thread_termination_trigger) {
            /* the thread is told to terminate */
            
            break;
        }
        
        if (context->reader_thread_state && !context->queue_thread_state) {
            context->reader_thread_state = false;
            context->queue_thread_state = true;
            
            dx_set_error_code(context->queue_thread_error);
        }
        
        if (!context->reader_thread_state && !is_thread_idle) {
            dx_notify_conn_termination(context, &is_thread_idle);
        }

        if (is_thread_idle) {
            if (dx_reestablish_connection(context)) {
                context->reader_thread_state = true;
                is_thread_idle = false;
            } else {
                /* no waiting is required here, because there is a timeout within 'dx_reestablish_connection' */
                
                continue;
            }
        }

        number_of_bytes_read = dx_recv(context->s, (void*)read_buf, READ_CHUNK_SIZE);
        
        if (number_of_bytes_read == INVALID_DATA_SIZE) {
            context->reader_thread_state = false;
            
            continue;
        }
        
        /* reporting the read data */        
        context->reader_thread_state = context_data->receiver(context->connection, (const void*)read_buf, number_of_bytes_read);
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static const int port_min = 0;
static const int port_max = 65535;
static const char host_port_splitter = ':';
static const char host_splitter = ',';

bool dx_split_address (const char* host, OUT dx_address_t* addr) {
    char* port_start = NULL;
    
    addr->host = dx_ansi_create_string_src(host);
    addr->port = NULL;

    if (addr->host == NULL) {
        return false;
    }
    
    port_start = strrchr(host, (int)host_port_splitter);
    
    if (port_start != NULL) {
        int port;

        if (sscanf(port_start + 1, "%d", &port) == 1) {
            if (port < port_min || port > port_max) {
                dx_free((void*)addr->host);
                
                return dx_set_error_code(dx_nec_invalid_port_value);
            }

            addr->port = addr->host + (port_start - host);

            /* separating host from port with zero symbol and advancing to the first port symbol */
            *(char*)(addr->port++) = 0;
        }
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

void dx_clear_address_array (dx_address_array_t* addresses) {
    int i = 0;
    
    if (addresses->elements == NULL) {
        return;
    }
    
    for (; i < addresses->size; ++i) {
        /* note that port buffer is not freed, because it's in fact just a pointer to a position
           within the host buffer */
        
        dx_free((void*)addresses->elements[i].host);
    }
    
    dx_free(addresses->elements);
}

/* -------------------------------------------------------------------------- */

bool dx_get_addresses_from_collection (const char* collection, OUT dx_address_array_t* addresses) {
    char* collection_copied = NULL;
    char* cur_address = NULL;
    char* splitter_pos = NULL;
    const char* last_port = NULL;
    int i = 0;
    
    if (collection == NULL || addresses == NULL) {
        dx_set_last_error(dx_ec_invalid_func_param_internal);

        return false;
    }
    
    cur_address = collection_copied = dx_ansi_create_string_src(collection);
    
    if (collection_copied == NULL) {
        return false;
    }
    
    do {
        dx_address_t addr;
        bool failed;
        
        splitter_pos = strchr(cur_address, (int)host_splitter);
        
        if (splitter_pos != NULL) {
            *(splitter_pos++) = 0;
        }
        
        if (!dx_split_address(cur_address, &addr)) {
            dx_clear_address_array(addresses);
            dx_free(collection_copied);
            
            return false;
        }
        
        DX_ARRAY_INSERT(*addresses, dx_address_t, addr, addresses->size, dx_capacity_manager_halfer, failed);
        
        if (failed) {
            dx_clear_address_array(addresses);
            dx_free(collection_copied);
            dx_free((void*)addr.host);
            
            return false;
        }
        
        cur_address = splitter_pos;
    } while (splitter_pos != NULL);
    
    dx_free(collection_copied);
    
    if ((last_port = addresses->elements[addresses->size - 1].port) == NULL) {
        dx_clear_address_array(addresses);
        
        return false;
    }
    
    for (; i < addresses->size; ++i) {
        if (addresses->elements[i].port == NULL) {
            addresses->elements[i].port = last_port;
        }
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

void dx_sleep_before_resolve (dx_network_connection_context_t* context) {
    static int RECONNECT_TIMEOUT = 10000;

    int* last_res_timestamp = &(context->addr_context.last_resolution_timestamp);
    int timestamp_diff = dx_millisecond_timestamp_diff(dx_millisecond_timestamp(), *last_res_timestamp);

    if (*last_res_timestamp != 0 && timestamp_diff < RECONNECT_TIMEOUT) {
        dx_sleep((int)((1.0 + dx_random_double(1)) * (RECONNECT_TIMEOUT - timestamp_diff)));
    }
    
    *last_res_timestamp = dx_millisecond_timestamp();
}

/* -------------------------------------------------------------------------- */

void dx_shuffle_addrs (dx_address_resolution_context_t* addrs) {
    DX_ARRAY_SHUFFLE(addrs->elements, dx_addrinfo_ptr, addrs->size);
}

/* -------------------------------------------------------------------------- */

bool dx_resolve_address (dx_network_connection_context_t* context) {
    dx_address_array_t addresses;
    struct addrinfo hints;
    int i = 0;
    
    if (context == NULL) {
        dx_set_last_error(dx_ec_invalid_func_param_internal);

        return false;
    }
    
    dx_memset(&addresses, 0, sizeof(dx_address_array_t));
    
    dx_sleep_before_resolve(context);
    
    CHECKED_CALL_2(dx_get_addresses_from_collection, context->address, &addresses);
    
    dx_memset(&hints, 0, sizeof(hints));
    
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    context->addr_context.elements_to_free = dx_calloc(addresses.size, sizeof(dx_addrinfo_ptr));
    
    if (context->addr_context.elements_to_free == NULL) {
        dx_clear_address_array(&addresses);
        
        return false;
    }
    
    for (; i < addresses.size; ++i) {
        dx_addrinfo_ptr addr = NULL;
        dx_addrinfo_ptr cur_addr = NULL;
        
        if (!dx_getaddrinfo(addresses.elements[i].host, addresses.elements[i].port, &hints, &addr)) {
            continue;
        }
        
        cur_addr = context->addr_context.elements_to_free[context->addr_context.elements_to_free_count++] = addr;
        
        for (; cur_addr != NULL; cur_addr = cur_addr->ai_next) {
            bool failed = false;
            
            DX_ARRAY_INSERT(context->addr_context, dx_addrinfo_ptr, cur_addr, context->addr_context.size, dx_capacity_manager_halfer, failed);

            if (failed) {
                dx_clear_address_array(&addresses);
                dx_clear_addr_context_data(&(context->addr_context));

                return false;
            }    
        }
    }
    
    dx_clear_address_array(&addresses);
    dx_shuffle_addrs(&(context->addr_context));
    
    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_connect_to_resolved_addresses (dx_network_connection_context_t* context) {
    dx_address_resolution_context_t* ctx = &(context->addr_context);
    
    CHECKED_CALL(dx_mutex_lock, &(context->socket_guard));

    for (; ctx->cur_addr_index < ctx->size; ++ctx->cur_addr_index) {
        dx_addrinfo_ptr cur_addr = ctx->elements[ctx->cur_addr_index];
        
        context->s = dx_socket(cur_addr->ai_family, cur_addr->ai_socktype, cur_addr->ai_protocol);

        if (context->s == INVALID_SOCKET) {
            /* failed to create a new socket */

            continue;
        }

        if (!dx_connect(context->s, cur_addr->ai_addr, (socklen_t)cur_addr->ai_addrlen)) {
            /* failed to connect */

            dx_close(context->s);

            continue;
        }

        break;
    }

    if (ctx->cur_addr_index == ctx->size) {
        /* we failed to establish a socket connection */

        dx_mutex_unlock(&(context->socket_guard));
        
        return false;
    }

    context->set_fields_flags |= SOCKET_FIELD_FLAG;
    
    return dx_mutex_unlock(&(context->socket_guard));
}

/* -------------------------------------------------------------------------- */

static bool dx_server_event_subscription_refresher (dxf_connection_t connection,
                                                    dxf_const_string_t* symbols, int symbol_count, int event_types, bool pause_state) {
    if (pause_state) {
        /* paused subscriptions don't need any refreshment */
        
        return true;
    }
    
    return dx_subscribe_symbols_to_events(connection, symbols, symbol_count, event_types, false, false);
}

/* ---------------------------------- */

bool dx_reestablish_connection (dx_network_connection_context_t* context) {
    CHECKED_CALL(dx_clear_server_info, context->connection);
    
    if (!dx_connect_to_resolved_addresses(context)) {
        /*
         *	An important moment here.
         *  Before we resolve address anew, we must attempt to connect to the previously
         *  resolved addresses, because there might still be some valid addresses resolved from
         *  the previous attempt. And only if the new connection attempt fails, which also means
         *  there are no valid addresses left, a new resolution is started, and then we repeat
         *  the connection attempt with the newly resolved addresses.
         */
        
        dx_clear_addr_context_data(&(context->addr_context));
        
        CHECKED_CALL(dx_resolve_address, context);
        CHECKED_CALL(dx_connect_to_resolved_addresses, context);
    }
    
    CHECKED_CALL(dx_send_protocol_description, context->connection);
    CHECKED_CALL_2(dx_send_record_description, context->connection, false);
    CHECKED_CALL_2(dx_process_connection_subscriptions, context->connection, dx_server_event_subscription_refresher);
    
    return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Advanced network functions implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_bind_to_address (dxf_connection_t connection, const char* address, const dx_connection_context_data_t* ccd) {
    dx_network_connection_context_t* context = NULL;
    bool res = true;
    
    if (address == NULL || ccd == NULL || ccd->receiver == NULL) {
        return dx_set_error_code(dx_ec_invalid_func_param_internal);
    }
    
    context = dx_get_subsystem_data(connection, dx_ccs_network, &res);
    
    if (context == NULL) {
        if (res) {
            dx_set_error_code(dx_cec_connection_context_not_initialized);
        }

        return false;
    }

    context->context_data = *ccd;
    
    CHECKED_CALL(dx_mutex_create, &(context->socket_guard));

    context->set_fields_flags |= MUTEX_FIELD_FLAG;
    
    if ((context->address = dx_ansi_create_string_src(address)) == NULL ||
        !dx_resolve_address(context) ||
        !dx_connect_to_resolved_addresses(context)) {
        
        return false;
    }
    
    /* the tricky place here.
       the error subsystem internally uses the thread-specific storage to ensure the error 
       operation functions are thread-safe and each thread supports its own last error.
       to do that, the key to that thread-specific data (which itself is the same for
       all threads) must be initialized.
       it happens implicitly on the first call to any error reporting function, but to
       ensure there'll be no potential thread race to create it, it's made sure such 
       key is created before any secondary thread is created. */
    CHECKED_CALL_0(dx_init_error_subsystem);
    
    if (!dx_thread_create(&(context->queue_thread), NULL, dx_queue_executor, context)) {
        return false;
    }
    
    context->set_fields_flags |= QUEUE_THREAD_FIELD_FLAG;
    
    if (!dx_thread_create(&(context->reader_thread), NULL, dx_socket_reader, context)) {
        /* failed to create a thread */
        
        return false;
    }
    
    context->set_fields_flags |= READER_THREAD_FIELD_FLAG;
    
    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_send_data (dxf_connection_t connection, const void* buffer, int buffer_size) {
    dx_network_connection_context_t* context = NULL;
    const char* char_buf = (const char*)buffer;
    bool res = true;
    
    if (buffer == NULL || buffer_size == 0) {
        return dx_set_error_code(dx_ec_invalid_func_param_internal);
    }
    
    context = dx_get_subsystem_data(connection, dx_ccs_network, &res);
    
    if (context == NULL) {
        if (res) {        
            dx_set_error_code(dx_cec_connection_context_not_initialized);
        }

        return false;
    }
    
    CHECKED_CALL(dx_mutex_lock, &(context->socket_guard));
    
    if (!IS_FLAG_SET(context->set_fields_flags, SOCKET_FIELD_FLAG)) {
        dx_mutex_unlock(&(context->socket_guard));
        
        return dx_set_error_code(dx_nec_connection_closed);
    }

    do {
        int sent_count = dx_send(context->s, (const void*)char_buf, buffer_size);
        
        if (sent_count == INVALID_DATA_SIZE) {
            dx_mutex_unlock(&(context->socket_guard));

            return false;
        }
        
        char_buf += sent_count;
        buffer_size -= sent_count;
    } while (buffer_size > 0);
    
    return dx_mutex_unlock(&(context->socket_guard));
}

/* -------------------------------------------------------------------------- */

bool dx_add_worker_thread_task (dxf_connection_t connection, dx_task_processor_t processor, void* data) {
    dx_network_connection_context_t* context = NULL;
    bool res = true;
    
    if (processor == NULL) {
        return dx_set_error_code(dx_ec_invalid_func_param_internal);
    }
    
    context = dx_get_subsystem_data(connection, dx_ccs_network, &res);

    if (context == NULL) {
        if (res) {        
            dx_set_error_code(dx_cec_connection_context_not_initialized);
        }

        return false;
    }
    
    return dx_add_task_to_queue(context->tq, processor, data);
}