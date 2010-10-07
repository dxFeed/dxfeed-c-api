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

/* -------------------------------------------------------------------------- */
/*
 *	Network error codes
 */
/* -------------------------------------------------------------------------- */

static const dx_error_code_descr_t g_network_errors[] = {
    { dx_nec_invalid_port_value, L"Server address has invalid port value" },
    { dx_nec_invalid_function_arg, L"Internal software error" },
    { dx_nec_invalid_connection_handle, L"Internal software error" },
    { dx_nec_connection_closed, L"Attempt to use an already closed connection" },
        
    { ERROR_CODE_FOOTER, ERROR_DESCR_FOOTER }
};

const dx_error_code_descr_t* network_error_roster = g_network_errors;

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
    
    time_t last_resolution_time;
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
    dx_connection_context_t context;
    dx_address_resolution_context_t addr_context;
    
    const char* connector;
    dx_socket_t s;
    pthread_t reader_thread;
    pthread_mutex_t socket_guard;
    bool termination_trigger;
    
    int set_fields_flags;
} dx_network_connection_context_t;

#define SOCKET_FIELD_FLAG   (0x1)
#define THREAD_FIELD_FLAG   (0x2)
#define MUTEX_FIELD_FLAG    (0x4)

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_network) {
    dx_network_connection_context_t* conn_data = dx_calloc(1, sizeof(dx_network_connection_context_t));

    if (conn_data == NULL) {
        return false;
    }

    if (!dx_set_subsystem_data(connection, dx_ccs_network, conn_data)) {
        dx_free(conn_data);

        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_clear_connection_data (dx_network_connection_context_t* conn_data);

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_network) {
    bool res = true;
    dx_network_connection_context_t* conn_data = NULL;

    if (connection == NULL) {
        dx_set_last_error(dx_sc_network, dx_nec_invalid_connection_handle);

        return false;
    }

    conn_data = (dx_network_connection_context_t*)dx_get_subsystem_data(connection, dx_ccs_network);

    if (conn_data == NULL) {
        return true;
    }

    if (conn_data->set_fields_flags & THREAD_FIELD_FLAG) {
        conn_data->termination_trigger = true;

        res = dx_wait_for_thread(conn_data->reader_thread, NULL) && res;
    }

    res = dx_clear_connection_data(conn_data) && res;

    return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	Common data
 */
/* -------------------------------------------------------------------------- */

static const int g_sock_operation_timeout = 100;

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

bool dx_clear_connection_data (dx_network_connection_context_t* conn_data) {
    int set_fields_flags = 0;
    bool success = true;
    
    if (conn_data == NULL) {
        return true;
    }
    
    dx_clear_addr_context_data(&(conn_data->addr_context));
    
    set_fields_flags = conn_data->set_fields_flags;

    if (IS_FLAG_SET(set_fields_flags, SOCKET_FIELD_FLAG)) {
        success = dx_close(conn_data->s) && success;
    }
    
    if (IS_FLAG_SET(set_fields_flags, MUTEX_FIELD_FLAG)) {
        success = dx_mutex_destroy(&(conn_data->socket_guard)) && success;
    }
    
    if (IS_FLAG_SET(set_fields_flags, THREAD_FIELD_FLAG)) {
        success = dx_close_thread_handle(conn_data->reader_thread) && success;
    }
    
    CHECKED_FREE(conn_data->connector);
    
    dx_free((void*)conn_data);
    
    return success;
}

/* -------------------------------------------------------------------------- */

void dx_notify_conn_termination (dxf_connection_t connection,
                                 dx_network_connection_context_t* conn_data, OUT bool* idle_thread_flag) {
    if (conn_data->context.notifier != NULL) {
        conn_data->context.notifier(connection);
    }
    
    if (idle_thread_flag != NULL) {
        *idle_thread_flag = true;
    }

    if (!dx_mutex_lock(&(conn_data->socket_guard))) {
        return;
    }

    dx_close(conn_data->s);
    
    conn_data->set_fields_flags &= ~SOCKET_FIELD_FLAG;    
    
    dx_mutex_unlock(&(conn_data->socket_guard));
}

/* -------------------------------------------------------------------------- */

bool dx_reevaluate_connector (dx_network_connection_context_t* conn_data);

void* dx_socket_reader (void* arg) {
    dxf_connection_t connection = NULL;
    dx_network_connection_context_t* conn_data = NULL;
    dx_connection_context_t* ctx = NULL;
    bool receiver_result = true;
    bool is_thread_idle = false;
    
    char read_buf[READ_CHUNK_SIZE];
    int number_of_bytes_read = 0;
    
    if (arg == NULL) {
        /* invalid input parameter
           but we cannot report it anywhere so simply exit */
           
        return NULL;
    }
    
    connection = (dxf_connection_t)arg;
    conn_data = dx_get_subsystem_data(connection, dx_ccs_network);
    
    if (conn_data == NULL) {
        /* same story, nowhere to report */
        
        return NULL;
    }
    
    ctx = &(conn_data->context);
    
    if (!dx_init_error_subsystem()) {
        dx_notify_conn_termination(connection, conn_data, NULL);
        
        return NULL;
    }
    
    for (;;) {
        if (conn_data->termination_trigger) {
            /* the thread is told to terminate */
            
            break;
        }
        
        if (!receiver_result && !is_thread_idle) {
            dx_notify_conn_termination(connection, conn_data, &is_thread_idle);
        }

        if (is_thread_idle) {
            if (dx_reevaluate_connector(conn_data)) {
                receiver_result = true;
                is_thread_idle = false;
            } else {
                continue;
            }
        }

        number_of_bytes_read = dx_recv(conn_data->s, (void*)read_buf, READ_CHUNK_SIZE);
        
        switch (number_of_bytes_read) {
        case INVALID_DATA_SIZE:
            /* the socket error */

            dx_notify_conn_termination(connection, conn_data, &is_thread_idle);
            
            continue;
        case 0:
            /* no data to read */
            dx_sleep(g_sock_operation_timeout);
            
            continue;            
        }
        
        /* reporting the read data */        
        receiver_result = ctx->receiver(connection, (const void*)read_buf, number_of_bytes_read);
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static const int port_min = 0;
static const int port_max = 65535;
static const char host_port_splitter = ':';
static const int invalid_host_count = -1;
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
                dx_set_last_error(dx_sc_network, dx_nec_invalid_port_value);
                
                dx_free((void*)addr->host);

                return false;
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

bool dx_get_addresses_from_connector (const char* connector, OUT dx_address_array_t* addresses) {
    char* connector_copied = NULL;
    char* cur_address = NULL;
    char* splitter_pos = NULL;
    const char* last_port = NULL;
    int i = 0;
    
    if (connector == NULL || addresses == NULL) {
        dx_set_last_error(dx_sc_network, dx_nec_invalid_function_arg);

        return false;
    }
    
    cur_address = connector_copied = dx_ansi_create_string_src(connector);
    
    if (connector_copied == NULL) {
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
            dx_free(connector_copied);
            
            return false;
        }
        
        DX_ARRAY_INSERT(*addresses, dx_address_t, addr, addresses->size, dx_capacity_manager_halfer, failed);
        
        if (failed) {
            dx_clear_address_array(addresses);
            dx_free(connector_copied);
            dx_free((void*)addr.host);
            
            return false;
        }
        
        cur_address = splitter_pos;
    } while (splitter_pos != NULL);
    
    dx_free(connector_copied);
    
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

void dx_sleep_before_resolve (dx_network_connection_context_t* conn_data) {
    static time_t RECONNECT_TIMEOUT = 10;

    time_t* last_res_time = &(conn_data->addr_context.last_resolution_time);
    time_t cur_time = time(NULL);

    if (*last_res_time != 0 && cur_time - *last_res_time < RECONNECT_TIMEOUT) {
        // TODO: add timeout randomization

        dx_sleep((int)((RECONNECT_TIMEOUT - (cur_time - *last_res_time)) * 1000));
    }
    
    time(last_res_time);
}

/* -------------------------------------------------------------------------- */

void dx_shuffle_addrs (dx_address_resolution_context_t* addrs) {
    // TODO: add addrs shuffling
}

/* -------------------------------------------------------------------------- */

bool dx_resolve_connector (dx_network_connection_context_t* conn_data) {
    dx_address_array_t addresses;
    struct addrinfo hints;
    int i = 0;
    
    if (conn_data == NULL) {
        dx_set_last_error(dx_sc_network, dx_nec_invalid_function_arg);

        return false;
    }
    
    dx_memset(&addresses, 0, sizeof(dx_address_array_t));
    
    dx_sleep_before_resolve(conn_data);
    
    if (!dx_get_addresses_from_connector(conn_data->connector, &addresses)) {
        return false;
    }
    
    dx_memset(&hints, 0, sizeof(hints));
    
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    conn_data->addr_context.elements_to_free = dx_calloc(addresses.size, sizeof(dx_addrinfo_ptr));
    
    if (conn_data->addr_context.elements_to_free == NULL) {
        dx_clear_address_array(&addresses);
        
        return false;
    }
    
    for (; i < addresses.size; ++i) {
        dx_addrinfo_ptr addr = NULL;
        dx_addrinfo_ptr cur_addr = NULL;
        
        if (!dx_getaddrinfo(addresses.elements[i].host, addresses.elements[i].port, &hints, &addr)) {
            continue;
        }
        
        cur_addr = conn_data->addr_context.elements_to_free[conn_data->addr_context.elements_to_free_count++] = addr;
        
        for (; cur_addr != NULL; cur_addr = cur_addr->ai_next) {
            bool failed = false;
            
            DX_ARRAY_INSERT(conn_data->addr_context, dx_addrinfo_ptr, cur_addr, conn_data->addr_context.size, dx_capacity_manager_halfer, failed);

            if (failed) {
                dx_clear_address_array(&addresses);
                dx_clear_addr_context_data(&(conn_data->addr_context));

                return false;
            }    
        }
    }
    
    dx_clear_address_array(&addresses);
    dx_shuffle_addrs(&(conn_data->addr_context));
    
    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_connect_to_resolved_addresses (dx_network_connection_context_t* conn_data) {
    dx_address_resolution_context_t* ctx = &(conn_data->addr_context);
    
    if (!dx_mutex_lock(&(conn_data->socket_guard))) {
        return false;
    }

    for (; ctx->cur_addr_index < ctx->size; ++ctx->cur_addr_index) {
        dx_addrinfo_ptr cur_addr = ctx->elements[ctx->cur_addr_index];
        
        conn_data->s = dx_socket(cur_addr->ai_family, cur_addr->ai_socktype, cur_addr->ai_protocol);

        if (conn_data->s == INVALID_SOCKET) {
            /* failed to create a new socket */

            continue;
        }

        if (!dx_connect(conn_data->s, cur_addr->ai_addr, (socklen_t)cur_addr->ai_addrlen)) {
            /* failed to connect */

            dx_close(conn_data->s);

            continue;
        }

        break;
    }

    if (ctx->cur_addr_index == ctx->size) {
        /* we failed to establish a socket connection */

        dx_mutex_unlock(&(conn_data->socket_guard));
        
        return false;
    }

    conn_data->set_fields_flags |= SOCKET_FIELD_FLAG;
    
    return dx_mutex_unlock(&(conn_data->socket_guard));
}

/* -------------------------------------------------------------------------- */

bool dx_reevaluate_connector (dx_network_connection_context_t* conn_data) {
    dx_clear_addr_context_data(&(conn_data->addr_context));
    
    if (!dx_resolve_connector(conn_data) ||
        !dx_connect_to_resolved_addresses(conn_data)) {
    
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Advanced network functions implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_bind_to_connector (dxf_connection_t connection, const char* connector, const dx_connection_context_t* cc) {
    dx_network_connection_context_t* conn_data = (dx_network_connection_context_t*)dx_get_subsystem_data(connection, dx_ccs_network);
    
    if (connector == NULL || cc == NULL || cc->receiver == NULL) {
        dx_set_last_error(dx_sc_network, dx_nec_invalid_function_arg);
        
        return false;
    }
    
    if (conn_data == NULL) {
        dx_set_last_error(dx_sc_network, dx_nec_invalid_connection_handle);

        return false;
    }

    conn_data->context = *cc;
    
    if (!dx_mutex_create(&(conn_data->socket_guard))) {
        return false;
    }

    conn_data->set_fields_flags |= MUTEX_FIELD_FLAG;
    
    if ((conn_data->connector = dx_ansi_create_string_src(connector)) == NULL ||
        !dx_resolve_connector(conn_data) ||
        !dx_connect_to_resolved_addresses(conn_data)) {
        
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
    dx_init_error_subsystem();
    
    if (!dx_thread_create(&(conn_data->reader_thread), NULL, dx_socket_reader, connection)) {
        /* failed to create a thread */
        
        return false;
    }
    
    conn_data->set_fields_flags |= THREAD_FIELD_FLAG;
    
    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_send_data (dxf_connection_t connection, const void* buffer, int buffer_size) {
    dx_network_connection_context_t* conn_data = NULL;
    const char* char_buf = (const char*)buffer;
    
    if (connection == NULL ||
        (conn_data = (dx_network_connection_context_t*)dx_get_subsystem_data(connection, dx_ccs_network)) == NULL) {
        dx_set_last_error(dx_sc_network, dx_nec_invalid_connection_handle);

        return false;
    }
    
    if (buffer == NULL || buffer_size == 0) {
        dx_set_last_error(dx_sc_network, dx_nec_invalid_function_arg);
        
        return false;
    }
    
    if (!dx_mutex_lock(&(conn_data->socket_guard))) {
        return false;
    }
    
    if ((conn_data->set_fields_flags & SOCKET_FIELD_FLAG) == 0) {
        dx_set_last_error(dx_sc_network, dx_nec_connection_closed);
        dx_mutex_unlock(&(conn_data->socket_guard));

        return false;
    }

    do {
        int sent_count = dx_send(conn_data->s, (const void*)char_buf, buffer_size);
        
        switch (sent_count) {
        case INVALID_DATA_SIZE:
            /* that means an error */
            
            dx_mutex_unlock(&(conn_data->socket_guard));
            
            return false;
        case 0:
            /* try again */
            dx_sleep(g_sock_operation_timeout);
            
            continue;
        }
        
        char_buf += sent_count;
        buffer_size -= sent_count;
    } while (buffer_size > 0);
    
    return dx_mutex_unlock(&(conn_data->socket_guard));
}