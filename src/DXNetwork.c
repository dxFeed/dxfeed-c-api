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
 *	Network connection context
 */
/* -------------------------------------------------------------------------- */

typedef struct {
    dx_connection_context_t context;
    const char* host;
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
static const int g_idle_thread_timeout = 500;

#define READ_CHUNK_SIZE 1024

/* -------------------------------------------------------------------------- */
/*
 *	Helper functions
 */
/* -------------------------------------------------------------------------- */

bool dx_clear_connection_data (dx_network_connection_context_t* conn_data) {
    int set_fields_flags = 0;
    bool success = true;
    
    if (conn_data == NULL) {
        return true;
    }
    
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
    
    CHECKED_FREE(conn_data->host);
    
    dx_free((void*)conn_data);
    
    return success;
}

/* -------------------------------------------------------------------------- */

void dx_notify_conn_termination (dx_network_connection_context_t* conn_data, OUT bool* idle_thread_flag) {
    if (conn_data->context.notifier != NULL) {
        conn_data->context.notifier(conn_data->host);
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
        dx_notify_conn_termination(conn_data, NULL);
        
        return NULL;
    }
    
    for (;;) {
        if (conn_data->termination_trigger) {
            /* the thread is told to terminate */
            
            break;
        }
        
        if (!receiver_result && !is_thread_idle) {
            dx_notify_conn_termination(conn_data, &is_thread_idle);
        }

        if (is_thread_idle) {
            dx_sleep(g_idle_thread_timeout);
            
            continue;
        }

        number_of_bytes_read = dx_recv(conn_data->s, (void*)read_buf, READ_CHUNK_SIZE);
        
        switch (number_of_bytes_read) {
        case INVALID_DATA_SIZE:
            /* the socket error */

            dx_notify_conn_termination(conn_data, &is_thread_idle);
            
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

bool dx_resolve_host (const char* host, struct addrinfo** addrs) {
    char* hostbuf = NULL;
    char* portbuf = NULL;
    char* port_start = NULL;
    struct addrinfo hints;
    bool res = true;
    
    *addrs = NULL;
    
    port_start = strrchr(host, (int)host_port_splitter);
    
    if (port_start != NULL) {
        int port;
        
        if (sscanf(port_start + 1, "%d", &port) == 1) {
            if (port < port_min || port > port_max) {
                dx_set_last_error(dx_sc_network, dx_nec_invalid_port_value);
                
                return false;
            }
            
            hostbuf = dx_ansi_create_string_src(host);
            
            if (hostbuf == NULL) {
                return false;
            }
            
            portbuf = hostbuf + (port_start - host);
            
            /* separating host from port with zero symbol and advancing to the first port symbol */
            *(portbuf++) = 0;
        }
    }
    
    dx_memset(&hints, 0, sizeof(hints));
    
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    if (hostbuf == NULL) {
        res = dx_getaddrinfo(host, NULL, &hints, addrs);
    } else {
        res = dx_getaddrinfo(hostbuf, portbuf, &hints, addrs);
    }
    
    if (hostbuf != NULL) {
        dx_free((void*)hostbuf);
    }
    
    return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	Advanced network functions implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_bind_to_host (dxf_connection_t connection, const char* host, const dx_connection_context_t* cc) {
    struct addrinfo* addrs = NULL;
    struct addrinfo* cur_addr = NULL;
    dx_network_connection_context_t* conn_data = (dx_network_connection_context_t*)dx_get_subsystem_data(connection, dx_ccs_network);
    
    if (host == NULL || cc == NULL || cc->receiver == NULL) {
        dx_set_last_error(dx_sc_network, dx_nec_invalid_function_arg);
        
        return false;
    }
    
    if (conn_data == NULL) {
        dx_set_last_error(dx_sc_network, dx_nec_invalid_connection_handle);

        return false;
    }

    conn_data->context = *cc;
    
    if (!dx_resolve_host(host, &addrs)) {
        return false;
    }
    
    cur_addr = addrs;    
    
    for (; cur_addr != NULL; cur_addr = cur_addr->ai_next) {
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
    
    dx_freeaddrinfo(addrs);
    
    if (cur_addr == NULL) {
        /* we failed to establish a socket connection */
        
        return false;
    }
    
    conn_data->set_fields_flags |= SOCKET_FIELD_FLAG;
    
    if ((conn_data->host = dx_ansi_create_string_src(host)) == NULL) {
        return false;
    }

    if (!dx_mutex_create(&(conn_data->socket_guard))) {
        return false;
    }

    conn_data->set_fields_flags |= MUTEX_FIELD_FLAG;
    
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