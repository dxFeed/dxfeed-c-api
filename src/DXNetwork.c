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

#include <stdio.h>
#include <time.h>
#include <string.h>

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
#include "Logger.h"

/* -------------------------------------------------------------------------- */
/*
 *	Internal structures and types
 */
/* -------------------------------------------------------------------------- */

typedef struct addrinfo* dx_addrinfo_ptr;

typedef struct {
    dx_addrinfo_ptr* elements;
    size_t size;
    size_t capacity;
    
    dx_addrinfo_ptr* elements_to_free;
    int elements_to_free_count;
    
    int last_resolution_timestamp;
    size_t cur_addr_index;
} dx_address_resolution_context_t;

typedef struct {
    bool enabled;
    char* key_store;
    char* key_store_password;
    char* trust_store;
    char* trust_store_password;
} dx_codec_tls_t;

typedef struct {
    bool enabled;
} dx_codec_gzip_t;

typedef struct {
    const char* host;
    const char* port;
    const char* username;
    const char* password;
    dx_codec_tls_t tls;
    dx_codec_gzip_t gzip;
} dx_address_t;

typedef struct {
    dx_address_t* elements;
    size_t size;
    size_t capacity;
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
	time_t next_heartbeat;
    dx_thread_t reader_thread;
    dx_thread_t queue_thread;
    dx_mutex_t socket_guard;
    
    bool reader_thread_termination_trigger;
    bool queue_thread_termination_trigger;
    bool reader_thread_state;
    bool queue_thread_state;
    dx_error_code_t queue_thread_error;
    
    dx_task_queue_t tq;

    /* protocol description properties */
    dx_property_map_t properties;
    
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
static void dx_protocol_property_clear(dx_network_connection_context_t* context);

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_network) {
    bool res = true;
    dx_network_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_network, &res);

    if (context == NULL) {
        return dx_on_connection_destroyed() && res;
    }

    if (IS_FLAG_SET(context->set_fields_flags, QUEUE_THREAD_FIELD_FLAG)) {
        context->queue_thread_termination_trigger = true;
        
        res = dx_wait_for_thread(context->queue_thread, NULL) && res;
		dx_log_debug_message(L"Queue thread exited");
    }
    
    if (IS_FLAG_SET(context->set_fields_flags, READER_THREAD_FIELD_FLAG)) {
        context->reader_thread_termination_trigger = true;
        
        res = dx_close_socket(context) && res;
        res = dx_wait_for_thread(context->reader_thread, NULL) && res;
		dx_log_debug_message(L"Reader thread exited");
    }

    res = dx_clear_connection_data(context) && res;
    res = dx_on_connection_destroyed() && res;

    return res;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_network) {
    bool res = true;
    dx_network_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_network, &res);
	dx_thread_t cur_thread = dx_get_thread_id();

    if (context == NULL) {
        return true;
    }

	return !dx_compare_threads(cur_thread, context->queue_thread) && !dx_compare_threads(cur_thread, context->reader_thread);
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

    dx_protocol_property_clear(context);
    
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
        context->context_data.notifier((void*)context->address, context->context_data.notifier_user_data);
    }
    
    if (idle_thread_flag != NULL) {
        *idle_thread_flag = true;
    }

    dx_close_socket(context);
}

/* -------------------------------------------------------------------------- */

#if !defined(_WIN32) || defined(USE_PTHREADS)
void* dx_queue_executor (void* arg) {
#else
unsigned dx_queue_executor(void* arg) {
#endif
    static int s_idle_timeout = 100;
    static int s_small_timeout = 25;
    static int s_heartbeat_timeout = 60; // in seconds
    
	time_t current_time;

	dx_network_connection_context_t* context = NULL;
    
    if (arg == NULL) {
        /* invalid input parameter
           but we cannot report it anywhere so simply exit */
        dx_set_error_code(dx_ec_invalid_func_param_internal);

        return DX_THREAD_RETVAL_NULL;
    }
    
    context = (dx_network_connection_context_t*)arg;
    
    if (!dx_init_error_subsystem()) {
        context->queue_thread_error = dx_ec_error_subsystem_failure;
        context->queue_thread_state = false;
        
        return DX_THREAD_RETVAL_NULL;
    }

	time (&context->next_heartbeat);

    for (;;) {
	    bool queue_empty = true;
		
		time (&current_time);
		if ( difftime ( current_time , context->next_heartbeat) >= 0 ) {
            if (!dx_send_heartbeat(context->connection, true)) {
                dx_sleep(s_idle_timeout);
                continue;
            }
			time (&context->next_heartbeat);
			context->next_heartbeat += s_heartbeat_timeout;
		}
		

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
    
    return DX_THREAD_RETVAL_NULL;
}

/* -------------------------------------------------------------------------- */

void dx_socket_reader (dx_network_connection_context_t* context);

#if !defined(_WIN32) || defined(USE_PTHREADS)
void* dx_socket_reader_wrapper(void* arg) {
#else
unsigned dx_socket_reader_wrapper(void* arg) {
#endif
    dx_network_connection_context_t* context = NULL;
    dx_connection_context_data_t* context_data = NULL;
        
    if (arg == NULL) {
        /* invalid input parameter
           but we cannot report it anywhere so simply exit */
        dx_set_error_code(dx_ec_invalid_func_param_internal);

        return DX_THREAD_RETVAL_NULL;
    }
    
    context = (dx_network_connection_context_t*)arg;
    context_data = &(context->context_data);
    
    if (context_data->stcn != NULL) {
        if (context_data->stcn(context->connection, context_data->notifier_user_data) == 0) {
            /* zero return value means fatal client side error */
            
            return DX_THREAD_RETVAL_NULL;
        }
    }
    
    dx_socket_reader(context);
    
    if (context_data->stdn != NULL) {
        context_data->stdn(context->connection, context_data->notifier_user_data);
    }
    
    return DX_THREAD_RETVAL_NULL;
}

/* -------------------------------------------------------------------------- */

bool dx_reestablish_connection (dx_network_connection_context_t* context);

void dx_socket_reader (dx_network_connection_context_t* context) {
    dx_connection_context_data_t* context_data = NULL;
    bool is_thread_idle = false;
    
    char read_buf[READ_CHUNK_SIZE];
    int number_of_bytes_read = 0;
    
    context_data = &(context->context_data);
        
    if (!dx_init_error_subsystem()) {
        /* the error subsystem is broken, aborting the thread */
        
        context_data->notifier(context->connection, context_data->notifier_user_data);
        
        return;
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
    size_t i = 0;
    
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
    size_t i = 0;
    
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

#ifdef ADDRESS_CODEC_TLS_ENABLED
#define DX_CODEC_TLS_STATUS true
#else
#define DX_CODEC_TLS_STATUS false
#endif

#ifdef ADDRESS_CODEC_GZIP_ENABLED
#define DX_CODEC_GZIP_STATUS true
#else
#define DX_CODEC_GZIP_STATUS false
#endif

typedef bool(*dx_codec_parser_t)(const char* codec, size_t size, dx_address_t* addr);

typedef struct {
    const char* name;
    bool supported;
    dx_codec_parser_t parser;
} dx_codec_info_t;

typedef struct {
    char* key;
    char* value;
} dx_address_property_t;

//function forward declaration
bool dx_codec_tls_parser(const char* codec, size_t size, OUT dx_address_t* addr);
//function forward declaration
bool dx_codec_gzip_parser(const char* codec, size_t size, OUT dx_address_t* addr);

static const dx_codec_info_t codecs[] = {
    { "tls", DX_CODEC_TLS_STATUS, dx_codec_tls_parser },
    { "gzip", DX_CODEC_GZIP_STATUS, dx_codec_gzip_parser }
};
static const char null_symbol = 0;
static const char entry_begin_symbol = '(';
static const char entry_end_symbol = ')';
static const char whitespace = ' ';
static const char codec_splitter = '+';
static const char properties_begin_symbol = '[';
static const char properties_end_symbol = ']';
static const char properties_splitter = ',';
static const char property_value_splitter = '=';

static bool dx_is_empty_entry(const char* entry_begin, const char* entry_end) {
    if (entry_begin > entry_end)
        return true;
    while (*entry_begin == whitespace) {
        if (entry_begin == entry_end)
            return true;
        entry_begin++;
    }
    return false;
}

static char* dx_find_first(char* from, char* end, int ch) {
    char* pos = strchr(from, ch);
    if (pos > end)
        return NULL;
    return pos;
}

static bool dx_has_next(char* next) {
    return next != NULL && *next != null_symbol;
}

static bool dx_address_get_next_entry(OUT char** next, OUT char** entry, OUT size_t* size) {
    char* begin = NULL;
    char* end = NULL;
    char* collection = NULL;
    if (entry == NULL || size == NULL || next == NULL)
        return dx_set_error_code(dx_ec_invalid_func_param_internal);

    *entry = NULL;
    *size = 0;
    collection = *next;
    if (*collection == null_symbol)
        return true;
    while (*entry == NULL && *collection != null_symbol) {
        begin = strchr(collection, entry_begin_symbol);
        end = strchr(collection, entry_end_symbol);

        if (begin == NULL && end == NULL) {
            if (!dx_is_empty_entry(collection, collection + strlen(collection))) {
                *entry = collection;
                *size = strlen(collection);
            }
            *next += *size;
            return true;
        } else if (begin != NULL && end != NULL) {
            collection = begin + 1;
            if (dx_is_empty_entry(collection, end)) {
                collection = end + 1;
                continue;
            }
            *entry = collection;
            *size = (end - collection) / sizeof(char);
            *next = end + 1;
            return true;
        } else {
            return dx_set_error_code(dx_ec_invalid_func_param);
        }
    }
    return true;
}

static bool dx_address_get_next_codec(OUT char** next, OUT char** codec, OUT size_t* size) {
    char* end = NULL;
    char* collection = NULL;
    if (codec == NULL || size == NULL || next == NULL)
        return dx_set_error_code(dx_ec_invalid_func_param_internal);

    *codec = NULL;
    *size = 0;
    collection = *next;
    if (collection == null_symbol)
        return true;
    while (*codec == NULL && *collection != null_symbol) {
        end = strchr(collection, codec_splitter);
        if (end == NULL)
            return true;
        if (dx_is_empty_entry(collection, end)) {
            collection = end + 1;
            continue;
        }
        *codec = collection;
        *size = (end - collection) / sizeof(char);
        *next = end + 1;
        return true;
    }

    return true;
}

static bool dx_address_get_codec_name(const char* codec, size_t codec_size, OUT char** name, OUT size_t* name_size) {
    char* end = NULL;
    if (codec == NULL || name == NULL || name_size == NULL) {
        return dx_set_error_code(dx_ec_invalid_func_param_internal);
    }

    *name = NULL;
    *name_size = 0;
    if (codec_size == 0)
        return true;

    end = strchr(codec, properties_begin_symbol);
    *name = codec;
    if (end == NULL || end >= codec + codec_size) {
        *name_size = codec_size;
    } else {
        *name_size = (end - codec) / sizeof(char);
    }
    return true;
}

static bool dx_address_get_codec_properties(const char* codec, size_t codec_size, OUT char** props, OUT size_t* props_size) {
    char* name;
    size_t name_size;
    char* props_begin;
    char* props_end;
    if (codec == NULL || props == NULL || props_size == NULL) {
        return dx_set_error_code(dx_ec_invalid_func_param_internal);
    }
    *props = NULL;
    *props_size = 0;
    if (!dx_address_get_codec_name(codec, codec_size, &name, &name_size))
        return false;
    if (codec_size == name_size)
        return true;
    //check proprties starts with '[' and finishes with ']' symbol
    props_begin = codec + name_size;
    props_end = codec + codec_size;
    if (*props_begin != properties_begin_symbol || *props_end != properties_end_symbol)
        return dx_set_error_code(dx_ec_invalid_func_param);
    if (!dx_is_empty_entry(props_begin, props_end)) {
        *props = props_begin;
        *props_size = props_end - props_begin;
    }
    return true;
}

static bool dx_address_get_next_property(OUT char** next, OUT size_t* next_size, OUT char** prop, OUT size_t* prop_size) {
    if (next == NULL || next_size == NULL || prop == NULL || prop_size == NULL) {
        return dx_set_error_code(dx_ec_invalid_func_param_internal);
    }
    *prop = NULL;
    *prop_size = 0;
    while (*next != null_symbol && next_size > 0) {
        //points to special symbol ',' or '[' that indicates begining of next property entry
        char* collection_begin = *next;
        //points to special symbol ']' that indicates ending of all properties
        char* collection_end = collection_begin + *next_size;
        //points to useful data of current property
        char* begin = collection_begin;
        //points to to special symbol ']' that indicates ending of current property
        char* end;
        //skip first ',' or '[' symbol
        if (*begin == properties_begin_symbol || *begin == properties_splitter)
            begin++;
        end = MIN(
            dx_find_first(begin, collection_end, properties_end_symbol),
            dx_find_first(begin, collection_end, properties_splitter));
        if (begin == NULL || end == NULL) {
            return dx_set_error_code(dx_ec_invalid_func_param);
        } else {
            *next = (*end == properties_end_symbol) ? end + 1 : end;
            *next_size -= *next - collection_begin;
            if (dx_is_empty_entry((const char*)begin, (const char*)end)) {
                continue;
            }
            *prop = begin;
            *prop_size = end - begin - 1;
        }
    }
    return true;
}

static bool dx_address_get_host_port_pair(const char* str, OUT char** address, OUT size_t* size) {
    char* begin;
    char* end;
    if (str == NULL || address == NULL || size == NULL) {
        return dx_set_error_code(dx_ec_invalid_func_param_internal);
    }
    begin = strchr(str, codec_splitter);
    if (begin == NULL) {
        begin = str;
    } else {
        begin++;
    }
    end = strchr(begin, properties_begin_symbol);
    *size = (end == NULL) ? strlen(begin) : end - begin - 1;
    *address = begin;
    return true;
}

static bool dx_address_get_properties(const char* str, OUT char** props, OUT size_t* props_size) {
    char* begin;
    char* end;
    char* address;
    size_t address_size;
    if (str == NULL || *props == NULL || props_size == NULL) {
        return dx_set_error_code(dx_ec_invalid_func_param_internal);
    }
    if (!dx_address_get_host_port_pair(str, &address, &address_size))
        return false;
    begin = address + address_size;
    end = strlen(str);
    if (*begin != properties_begin_symbol || *end != properties_end_symbol)
        return dx_set_error_code(dx_ec_invalid_func_param);
    if (!dx_is_empty_entry(begin, end)) {
        *props = begin;
        *props_size = end - begin;
    }
    return true;
}

void dx_address_clear_property(OUT dx_address_property_t* prop) {
    if (prop == NULL)
        return;
    if (prop->key != NULL)
        dx_free(prop->key);
    if (prop->value != NULL)
        dx_free(prop->value);
    prop->key = NULL;
    prop->value = NULL;
}

//Note: free allocated memory for prop!
bool dx_address_parse_property(const char* str, size_t size, OUT dx_address_property_t* prop) {
    char* splitter = dx_find_first(str, str + size, property_value_splitter);
    size_t key_size;
    size_t value_size;
    if (splitter == NULL || dx_is_empty_entry(str, splitter - 1) || dx_is_empty_entry(splitter + 1, str + size)) {
        dx_logging_info(L"Invalid property from: ...%s", str);
        return dx_set_error_code(dx_nec_invalid_function_arg);
    }
    key_size = splitter - str;
    value_size = size - key_size - 1;
    dx_address_clear_property(prop);
    prop->key = dx_ansi_create_string_src_len(str, key_size);
    prop->value = dx_ansi_create_string_src_len(splitter + 1, value_size);
    if (prop->key == NULL || prop->value == NULL) {
        dx_address_clear_property(prop);
        return false;
    }
    return true;
}

void dx_codec_tls_free(OUT dx_codec_tls_t* tls) {
    if (tls->key_store != NULL)
        dx_free(tls->key_store);
    if (tls->key_store_password != NULL)
        dx_free(tls->key_store_password);
    if (tls->trust_store != NULL)
        dx_free(tls->trust_store);
    if (tls->trust_store_password != NULL)
        dx_free(tls->trust_store_password);
    tls->key_store = NULL;
    tls->key_store_password = NULL;
    tls->trust_store = NULL;
    tls->trust_store_password = NULL;
}

bool dx_codec_tls_parser(const char* codec, size_t size, OUT dx_address_t* addr) {
    char* next;
    size_t next_size;
    dx_codec_tls_t tls = { true, NULL, NULL, NULL, NULL };
    if (!dx_address_get_codec_properties(codec, size, &next, &next_size))
        return false;
    do {
        char* str;
        size_t str_size;
        dx_address_property_t prop;
        if (!dx_address_get_next_property(&next, &next_size, &str, &str_size)) {
            dx_codec_tls_free(&tls);
            return false;
        }
        if (str == NULL)
            continue;
        if (!dx_address_parse_property((const char*)str, str_size, &prop)) {
            dx_codec_tls_free(&tls);
            return false;
        }

        if (strcmp(prop.key, "keyStore") == 0) {
            DX_SWAP(char*, tls.key_store, prop.value);
        } else if (strcmp(prop.key, "keyStorePassword") == 0) {
            DX_SWAP(char*, tls.key_store_password, prop.value);
        } else if (strcmp(prop.key, "trustStore") == 0) {
            DX_SWAP(char*, tls.trust_store, prop.value);
        } else if (strcmp(prop.key, "trustStorePassword") == 0) {
            DX_SWAP(char*, tls.trust_store_password, prop.value);
        } else {
            dx_logging_info(L"Unknown property for TLS: %s=%s", prop.key, prop.value);
            dx_address_clear_property(&prop);
            dx_codec_tls_free(&tls);
            return dx_set_error_code(dx_nec_invalid_function_arg);
        }
        dx_address_clear_property(&prop);

    } while (dx_has_next(next));

    addr->tls = tls;
    return true;
}

bool dx_codec_gzip_parser(const char* codec, size_t size, OUT dx_address_t* addr) {
    addr->gzip.enabled = true;
    return true;
}

static bool dx_get_codec(const char* codec, size_t codec_size, OUT dx_address_t* addr) {
    char* codec_name;
    size_t codec_name_size;
    size_t count = sizeof(codecs) / sizeof(codecs[0]);
    size_t i;
    if (!dx_address_get_codec_name(codec, codec_size, OUT &codec_name, OUT &codec_name_size))
        return false;
    for (i = 0; i < count; ++i) {
        dx_codec_info_t info = codecs[i];
        if (strlen(info.name) != codec_name_size || stricmp(info.name, codec_name) != 0)
            continue;
        if (!info.supported)
            return dx_set_error_code(dx_nec_unknown_codec);
        if (!info.parser(codec, codec_size, addr))
            return false;
        break;
    }
    return true;
}

//TODO: free address everywhere...
static bool dx_address_parse(char* entry, size_t size, OUT dx_address_t* addr) {
    char* next = entry;
    char* codec;
    size_t codec_size;
    char* address;
    size_t address_size;
    //TODO: remove with function
    memset((void*)addr, 0, sizeof(dx_address_t));
    //get codecs
    do {
        if (!dx_address_get_next_codec(OUT &next, OUT &codec, OUT &codec_size))
            return false;
        if (codec == NULL)
            continue;
        if (!dx_get_codec(codec, codec_size, addr))
            return false;
    } while (dx_has_next(next));
    
TODO:continue here
    //get host and port
    if (!dx_address_get_host_port_pair(entry, &address, &address_size) ||
        !dx_split_address(, addr)) {
        return false;
    }
    
}

bool dx_get_addresses_from_collection2(const char* collection, OUT dx_address_array_t* addresses) {
    char* collection_copied = NULL;
    char* cur_address = NULL;
    char* splitter_pos = NULL;
    const char* last_port = NULL;
    size_t i = 0;

    char* next = (char*)collection;

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

        char* entry;
        size_t entry_size;

        if (!dx_address_get_next_entry(&next, &entry, &entry_size)) {
            dx_clear_address_array(addresses);
            dx_free(collection_copied);
            return false;
        }

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
    } while (next != null_symbol);

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
    size_t i = 0;
    
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

    //all addresses in collection cannot be resolved
    if (context->addr_context.size == 0)
        return false;
    
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
                                                    dx_order_source_array_ptr_t order_source,
                                                    dxf_const_string_t* symbols, size_t symbol_count,
                                                    int event_types, dxf_uint_t subscr_flags, 
                                                    dxf_long_t time) {
    return dx_subscribe_symbols_to_events(connection, order_source, symbols, symbol_count, 
        event_types, false, false, subscr_flags, time);
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
    
    CHECKED_CALL_2(dx_send_protocol_description, context->connection, false);
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
    
    if (!dx_thread_create(&(context->reader_thread), NULL, dx_socket_reader_wrapper, context)) {
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

/* -------------------------------------------------------------------------- */
/*
 *	Protocol properties functions
 */
/* -------------------------------------------------------------------------- */

static void dx_protocol_property_free_item(void* obj) {
    if (obj == NULL)
        return;
    dx_property_item_t* item = (dx_property_item_t*) obj;
    if (item->key != NULL)
        dx_free(item->key);
    if (item->value != NULL)
        dx_free(item->value);
}

static void dx_protocol_property_clear(dx_network_connection_context_t* context) {
    if (context == NULL)
        return;
    dx_property_map_t* props = &context->properties;
    dx_property_item_t* item = props->elements;
    size_t i;
    for (i = 0; i < props->size; i++) {
        dx_protocol_property_free_item((void*)item++);
    }
    dx_free(props->elements);
    props->elements = NULL;
    props->size = 0;
    props->capacity = 0;
}

static inline int dx_property_item_comparator(dx_property_item_t item1, dx_property_item_t item2) {
    return dx_compare_strings((dxf_const_string_t)item1.key, (dxf_const_string_t)item2.key);
}

//Note: not fast map-dictionary realization, but enough for properties
static bool dx_protocol_property_set_impl(dx_network_connection_context_t* context, dxf_const_string_t key, dxf_const_string_t value) {
    dx_property_map_t* props = &context->properties;
    dx_property_item_t item = { (dxf_string_t)key, (dxf_string_t)value };
    bool found;
    size_t index;

    if (context == NULL)
        return dx_set_error_code(dx_ec_invalid_func_param_internal);
    if (key == NULL || value == NULL)
        return dx_set_error_code(dx_ec_invalid_func_param);

    DX_ARRAY_BINARY_SEARCH(props->elements, 0, props->size, item, dx_property_item_comparator, found, index);
    if (found) {
        dx_property_item_t* item_ptr = props->elements + index;
        if (dx_compare_strings(item_ptr->value, value) == 0) {
            // map already contains such key-value pair
            return true;
        }
        dxf_string_t value_copy = dx_create_string_src(value);
        if (value_copy == NULL)
            return false;
        dx_free(item_ptr->value);
        item_ptr->value = value_copy;
    } else {
        dx_property_item_t new_item = { dx_create_string_src(key), dx_create_string_src(value) };
        bool insertion_failed;
        if (new_item.key == NULL || new_item.value == NULL) {
            dx_protocol_property_free_item((void*)&new_item);
            return false;
        }
        DX_ARRAY_INSERT(*props, dx_property_item_t, new_item, index, dx_capacity_manager_halfer, insertion_failed);
        if (insertion_failed) {
            dx_protocol_property_free_item((void*)&new_item);
            return false;
        }
    }

    return true;
}

bool dx_protocol_property_set(dxf_connection_t connection, dxf_const_string_t key, dxf_const_string_t value) {
    dx_network_connection_context_t* context = NULL;
    bool res = true;

    context = dx_get_subsystem_data(connection, dx_ccs_network, &res);

    if (context == NULL) {
        if (res) {
            dx_set_error_code(dx_cec_connection_context_not_initialized);
        }
        return false;
    }

    return dx_protocol_property_set_impl(context, key, value);
}

const dx_property_map_t* dx_protocol_property_get_all(dxf_connection_t connection) {
    dx_network_connection_context_t* context = NULL;
    bool res = true;

    context = dx_get_subsystem_data(connection, dx_ccs_network, &res);

    if (context == NULL) {
        if (res) {
            dx_set_error_code(dx_cec_connection_context_not_initialized);
        }
        return NULL;
    }

    return (const dx_property_map_t*)&context->properties;
}
