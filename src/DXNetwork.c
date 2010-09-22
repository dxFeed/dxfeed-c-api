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

unsigned g_invalid_buffer_length = (unsigned)-1;

/* -------------------------------------------------------------------------- */
/*
 *	Network error codes
 */
/* -------------------------------------------------------------------------- */

static struct dx_error_code_descr_t g_network_errors[] = {
    { dx_nec_invalid_port_value, "Server address has invalid port value" },
    { dx_nec_invalid_function_arg, "Internal software error" },
    { dx_nec_conn_not_established, "Internal software error" },
        
    { ERROR_CODE_FOOTER, ERROR_DESCR_FOOTER }
};

const struct dx_error_code_descr_t* network_error_roster = g_network_errors;

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary stuff in detail
 */
/* -------------------------------------------------------------------------- */

struct dx_connection_data_t {
    struct dx_connection_context_t context;
    dx_socket_t s;
    pthread_t reader_thread;
    bool termination_trigger;
};

static struct dx_connection_data_t* g_conn = NULL;
static const size_t g_sock_operation_timeout = 100;

/* -------------------------------------------------------------------------- */

#define READ_CHUNK_SIZE 1024

void* dx_socket_reader (void* arg) {
    struct dx_connection_data_t* conn_data = NULL;
    struct dx_connection_context_t* ctx = NULL;
    bool receiver_result = true;

    char read_buf[READ_CHUNK_SIZE];
    int count_of_bytes_read = 0;
    
    if (arg == NULL) {
        /* invalid input parameter
           but we cannot report it anywhere so simply exit */
           
        return NULL;
    }
    
    conn_data = (struct dx_connection_data_t*)arg;
    ctx = &(conn_data->context);
    
    if (!dx_init_error_subsystem()) {
        ctx->receiver(NULL, g_invalid_buffer_length);

        if (ctx->terminator) {
            ctx->terminator();
        }

        return NULL;
    }
    
    for (;;) {
        if (conn_data->termination_trigger || !receiver_result) {
            /* the thread is told to terminate */
            
            if (ctx->terminator) {
                ctx->terminator();
            }
            return NULL;
        }
        
        count_of_bytes_read = dx_recv(conn_data->s, (void*)read_buf, READ_CHUNK_SIZE);
        
        switch (count_of_bytes_read) {
        case INVALID_DATA_SIZE:
            /* the socket error
               calling the data receiver to inform them something's going wrong */

            ctx->receiver(NULL, g_invalid_buffer_length);

            if (ctx->terminator) {
                ctx->terminator();
            }
            return NULL;
        case 0:
            /* no data to read */
            dx_sleep(g_sock_operation_timeout);
            
            continue;            
        }
        
        /* reporting the read data */        
        receiver_result = ctx->receiver((const void*)read_buf, count_of_bytes_read);
    }

    if (ctx->terminator) {
        ctx->terminator();
    }
    return NULL;
}

/* -------------------------------------------------------------------------- */

static const int port_min = 0;
static const int port_max = 65535;

bool dx_resolve_host (const char* host, struct addrinfo** addrs) {
    char* hostbuf = NULL;
    char* portbuf = NULL;
    char* port_start = strrchr(host, (int)':');
    struct addrinfo hints;
    bool res = true;
    
    *addrs = NULL;
    
    if (port_start != NULL) {
        int port;
        
        if (sscanf(port_start + 1, "%d", &port) == 1) {
            size_t hostlen = 0;
            
            if (port < port_min || port > port_max) {
                dx_set_last_error(dx_sc_network, dx_nec_invalid_port_value);
                
                return false;
            }
            
            hostlen = strlen(host);
            hostbuf = (char*)dx_malloc(hostlen + 1);
            
            if (hostbuf == NULL) {
                return false;
            }
            
            strcpy(hostbuf, host);
            
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

bool dx_create_connection (const char* host, const struct dx_connection_context_t* cc) {
    struct addrinfo* addrs = NULL;
    dx_socket_t s = INVALID_SOCKET;
    struct addrinfo* cur_addr = NULL;
    struct dx_connection_data_t* conn_data = NULL;
    
    if (host == NULL || cc == NULL || cc->receiver == NULL) {
        dx_set_last_error(dx_sc_network, dx_nec_invalid_function_arg);
        
        return false;
    }
    
    if (!dx_resolve_host(host, &addrs)) {
        return false;
    }
    
    cur_addr = addrs;
    
    for (; cur_addr != NULL; cur_addr = cur_addr->ai_next) {
        s = dx_socket(cur_addr->ai_family, cur_addr->ai_socktype, cur_addr->ai_protocol);

        if (s == INVALID_SOCKET) {
            /* failed to create a new socket */

            continue;
        }

        if (!dx_connect(s, cur_addr->ai_addr, (socklen_t)cur_addr->ai_addrlen)) {
            /* failed to connect */

            dx_close(s);

            continue;
        }
        
        break;
    }
    
    dx_freeaddrinfo(addrs);
    
    if (cur_addr == NULL) {
        /* we failed to establish a socket connection */
        
        return false;
    }
    
    conn_data = (struct dx_connection_data_t*)dx_calloc(1, sizeof(struct dx_connection_data_t));
    
    if (conn_data == NULL) {
        dx_close(s);

        return false;
    }
    
    conn_data->context = *cc;
    conn_data->s = s;
    
    /* the tricky place here.
       the error subsystem internally uses the thread-specific storage to ensure the error 
       operation functions are thread-safe and each thread supports its own last error.
       to do that, the key to that thread-specific data (which itself is the same for
       all threads) must be initialized.
       it happens implicitly on the first call to any error reporting function, but to
       ensure there'll be no potential thread race to create it, it's made sure such 
       key is created before any secondary thread is created. */
    dx_init_error_subsystem();
    
    if (!dx_thread_create(&(conn_data->reader_thread), NULL, dx_socket_reader, (void*)conn_data)) {
        /* failed to create a thread */
        
        dx_free((void*)conn_data);
        dx_close(s);
        
        return false;
    }
    
    g_conn = conn_data;    
    
    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_send_data (const void* buffer, unsigned buflen) {
    const char* char_buf = (const char*)buffer;
    
    if (buffer == NULL || buflen == 0) {
        dx_set_last_error(dx_sc_network, dx_nec_invalid_function_arg);
        
        return false;
    }
    
    if (g_conn == NULL) {
        dx_set_last_error(dx_sc_network, dx_nec_conn_not_established);
        
        return false;
    }
    
    do {
        int sent_count = dx_send(g_conn->s, (const void*)char_buf, buflen);
        
        switch (sent_count) {
        case INVALID_DATA_SIZE:
            /* that means an error */
            return false;
        case 0:
            /* try again */
            dx_sleep(g_sock_operation_timeout);
            
            continue;
        }
        
        char_buf += sent_count;
        buflen -= sent_count;
    } while (buflen > 0);
    
    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_close_connection () {
    bool res = true;
    
    if (g_conn == NULL) {
        dx_set_last_error(dx_sc_network, dx_nec_conn_not_established);

        return false;
    }
    
    g_conn->termination_trigger = true;
    
    res = dx_wait_for_thread(g_conn->reader_thread, NULL) && res;
    res = dx_close(g_conn->s) && res;
    
    dx_free((void*)g_conn);
    
    g_conn = NULL;    
    
    return res;
}