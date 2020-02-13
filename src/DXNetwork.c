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

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdint.h>

#ifdef DXFEED_CODEC_TLS_ENABLED
#include <tls.h>
#endif // DXFEED_CODEC_TLS_ENABLED


#include "DXAddressParser.h"
#include "DXNetwork.h"
#include "DXSockets.h"
#include "DXErrorHandling.h"
#include "DXThreads.h"
#include "DXMemory.h"
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
	char* host;
	char* port;
	char* username;
	char* password;
	dx_codec_tls_t tls;
	dx_codec_gzip_t gzip;

	dx_addrinfo_ptr* elements;
	size_t size;
	size_t capacity;
	size_t cur_info_index;

#ifdef DXFEED_CODEC_TLS_ENABLED
	uint8_t* key_store_mem;
	size_t key_store_len;
	uint8_t* trust_store_mem;
	size_t trust_store_len;
#endif // DXFEED_CODEC_TLS_ENABLED

	bool is_connection_failed;
} dx_ext_address_t;

typedef struct {
	dx_ext_address_t* elements;
	size_t size;
	size_t capacity;

	dx_addrinfo_ptr* elements_to_free;
	int elements_to_free_count;

	int last_resolution_timestamp;
	size_t cur_addr_index;
} dx_address_resolution_context_t;

#ifdef DXFEED_CODEC_TLS_ENABLED
typedef struct tls* dx_tls;
typedef struct tls_config* dx_tls_config;
#endif // DXFEED_CODEC_TLS_ENABLED

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
	time_t last_server_heartbeat;
	dx_thread_t reader_thread;
	dx_thread_t queue_thread;
	dx_mutex_t socket_guard;

#ifdef DXFEED_CODEC_TLS_ENABLED
	dx_tls tls_context;
	dx_tls_config tls_config;
#endif // DXFEED_CODEC_TLS_ENABLED

	bool reader_thread_termination_trigger;
	bool queue_thread_termination_trigger;
	bool reader_thread_state;
	bool queue_thread_state;
	dx_error_code_t queue_thread_error;

	dx_task_queue_t tq;

	FILE* raw_dump_file;

	/* protocol description properties */
	dx_property_map_t properties;
	dx_property_map_t prop_backup;

	dxf_connection_status_t status;
	dx_mutex_t status_guard;

	int set_fields_flags;
} dx_network_connection_context_t;

#define SOCKET_FIELD_FLAG           (1 << 0)
#define READER_THREAD_FIELD_FLAG    (1 << 1)
#define MUTEX_FIELD_FLAG            (1 << 2)
#define TASK_QUEUE_FIELD_FLAG       (1 << 3)
#define QUEUE_THREAD_FIELD_FLAG     (1 << 4)
#define DUMPING_RAW_DATA_FIELD_FLAG (1 << 5)
#define PROPERTIES_BACKUP_FLAG      (1 << 8)
#define STATUS_GUARD_FLAG           (1 << 16)

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
		!(dx_mutex_create(&context->status_guard) && (context->set_fields_flags |= STATUS_GUARD_FLAG)) ||
		!dx_set_subsystem_data(connection, dx_ccs_network, context)) {
		dx_clear_connection_data(context);

		return false;
	}

#ifdef DXFEED_CODEC_TLS_ENABLED
	if (tls_init() < 0) {
		dx_clear_connection_data(context);
		return false;
	}
#endif // DXFEED_CODEC_TLS_ENABLED

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
#define TRUST_STORE_DEFAULT_NAME "ca_cert.pem"
#define DX_PROPERTY_AUTH L"authorization"

/* -------------------------------------------------------------------------- */
/*
 *	Helper functions
 */
/* -------------------------------------------------------------------------- */

static void dx_protocol_property_clear(dx_network_connection_context_t* context);
static bool dx_protocol_property_make_backup(dx_network_connection_context_t* context);
static bool dx_protocol_property_restore_backup(dx_network_connection_context_t* context);

/* -------------------------------------------------------------------------- */

#ifdef DXFEED_CODEC_TLS_ENABLED
#ifndef MACOSX
void tls_unload_file(uint8_t* buf, size_t len) {
	//Note: it is simple stub since current TLS API do not support unloading files
}
#endif
#endif

/* -------------------------------------------------------------------------- */

static void dx_logging_ansi_error(const char* ansi_error) {
	if (ansi_error == NULL)
		return;
	dxf_string_t w_error = dx_ansi_to_unicode(ansi_error);
	dx_logging_error(w_error);
	dx_free(w_error);
}

/* -------------------------------------------------------------------------- */

static dx_ext_address_t* dx_get_current_address(dx_network_connection_context_t* context) {
	if (context == NULL || context->addr_context.elements == NULL) {
		return NULL;
	}

	return &(context->addr_context.elements[context->addr_context.cur_addr_index]);
}

/* -------------------------------------------------------------------------- */

void dx_clear_addr_context_data (dx_address_resolution_context_t* context_data) {
	if (context_data->elements_to_free != NULL) {
		int i = 0;

		for (; i < context_data->elements_to_free_count; ++i) {
			dx_freeaddrinfo(context_data->elements_to_free[i]);
		}

		dx_free(context_data->elements_to_free);

		context_data->elements_to_free = NULL;
		context_data->elements_to_free_count = 0;
	}

	if (context_data->elements != NULL) {
		size_t i;
		for (i = 0; i < context_data->size; i++) {
			dx_ext_address_t* addr = &(context_data->elements[i]);
			if (addr->host != NULL) {
				dx_free(addr->host);
				addr->host = NULL;
			}
			if (addr->port != NULL) {
				dx_free(addr->port);
				addr->port = NULL;
			}
			if (addr->username != NULL) {
				dx_free(addr->username);
				addr->username = NULL;
			}
			if (addr->password != NULL) {
				dx_free(addr->password);
				addr->password = NULL;
			}

			dx_codec_tls_free(&addr->tls);

			dx_free(addr->elements);
			addr->elements = NULL;
			addr->size = 0;
			addr->capacity = 0;

#ifdef DXFEED_CODEC_TLS_ENABLED
			if (addr->key_store_mem != NULL) {
				tls_unload_file(addr->key_store_mem, addr->key_store_len);
				addr->key_store_mem = NULL;
				addr->key_store_len = 0;
			}
			if (addr->trust_store_mem != NULL) {
				tls_unload_file(addr->trust_store_mem, addr->trust_store_len);
				addr->trust_store_mem = NULL;
				addr->trust_store_len = 0;
			}
#endif // DXFEED_CODEC_TLS_ENABLED
		}

		dx_free(context_data->elements);
		context_data->elements = NULL;
		context_data->size = 0;
		context_data->capacity = 0;
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

	set_fields_flags = context->set_fields_flags;

	if (IS_FLAG_SET(set_fields_flags, SOCKET_FIELD_FLAG)) {
#ifdef DXFEED_CODEC_TLS_ENABLED
		if (dx_get_current_address(context)->tls.enabled) {
			tls_close(context->tls_context);
			tls_free(context->tls_context);
			tls_config_free(context->tls_config);
		} else {
			success = dx_close(context->s) && success;
		}
#else
		success = dx_close(context->s) && success;
#endif // DXFEED_CODEC_TLS_ENABLED
	}

	dx_clear_addr_context_data(&(context->addr_context));

	if (IS_FLAG_SET(set_fields_flags, MUTEX_FIELD_FLAG)) {
		success = dx_mutex_destroy(&context->socket_guard) && success;
	}

	if (IS_FLAG_SET(set_fields_flags, READER_THREAD_FIELD_FLAG)) {
		success = dx_close_thread_handle(context->reader_thread) && success;
	}

	if (IS_FLAG_SET(set_fields_flags, TASK_QUEUE_FIELD_FLAG)) {
		success = dx_destroy_task_queue(context->tq) && success;
	}

	if (IS_FLAG_SET(set_fields_flags, DUMPING_RAW_DATA_FIELD_FLAG)) {
		if (context->raw_dump_file != NULL)
			success = (fclose(context->raw_dump_file) == 0) && success;
		context->raw_dump_file = NULL;
	}

	if (IS_FLAG_SET(set_fields_flags, STATUS_GUARD_FLAG)) {
		success = dx_mutex_destroy(&context->status_guard) && success;
	}

	CHECKED_FREE(context->address);

	dx_protocol_property_clear(context);

	dx_free(context);

	return success;
}

/* -------------------------------------------------------------------------- */

static bool dx_close_socket (dx_network_connection_context_t* context) {
	bool res = true;

	dx_connection_status_set(context->connection, dxf_cs_not_connected);

	CHECKED_CALL(dx_mutex_lock, &(context->socket_guard));

	if (!IS_FLAG_SET(context->set_fields_flags, SOCKET_FIELD_FLAG)) {
		return dx_mutex_unlock(&(context->socket_guard)) && res;
	}

#ifdef DXFEED_CODEC_TLS_ENABLED
	if (dx_get_current_address(context)->tls.enabled) {
		tls_close(context->tls_context);
		tls_free(context->tls_context);
		tls_config_free(context->tls_config);
	} else {
		res = dx_close(context->s);
	}
#else
	res = dx_close(context->s);
#endif // DXFEED_CODEC_TLS_ENABLED

	context->set_fields_flags &= ~SOCKET_FIELD_FLAG;

	return dx_mutex_unlock(&(context->socket_guard)) && res;
}

/* -------------------------------------------------------------------------- */

static bool dx_have_credentials(dx_network_connection_context_t* context) {
	return dx_property_map_contains(&context->properties, DX_PROPERTY_AUTH);
}

/* -------------------------------------------------------------------------- */

void dx_notify_conn_termination (dx_network_connection_context_t* context, OUT bool* idle_thread_flag) {
	//if connection required login and we have credentials - don't call notifier
	if (dx_connection_status_get(context->connection) == dxf_cs_login_required) {
		if (dx_have_credentials(context))
			return;
		dx_get_current_address(context)->is_connection_failed = true;
		dx_set_error_code(dx_pec_credentials_required);
	}

	if (context->context_data.notifier != NULL) {
		context->context_data.notifier((void*)context->connection, context->context_data.notifier_user_data);
	}

	if (idle_thread_flag != NULL) {
		*idle_thread_flag = true;
	}

	if (dx_get_error_code() == dx_pec_authentication_error) {
		dx_get_current_address(context)->is_connection_failed = true;
	}

	dx_close_socket(context);
}

/* -------------------------------------------------------------------------- */

#define HEARTBEAT_TIMEOUT_IN_SECONDS 60
#define MAX_MISSING_HEARTBEATS 2
#define SERVER_HEARTBEAT_TIMEOUT_IN_SECONDS (MAX_MISSING_HEARTBEATS * HEARTBEAT_TIMEOUT_IN_SECONDS)

#if !defined(_WIN32) || defined(USE_PTHREADS)
void* dx_queue_executor (void* arg) {
#else
unsigned dx_queue_executor(void* arg) {
#endif	
	static const int s_idle_timeout = 100;
	static const int s_small_timeout = 25;

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

	time(&context->next_heartbeat);

	for (;;) {
		bool queue_empty = true;


        const time_t last_server_heartbeat = atomic_read_time(&context->last_server_heartbeat);
		if (last_server_heartbeat != 0 && difftime(time(NULL), last_server_heartbeat) >= SERVER_HEARTBEAT_TIMEOUT_IN_SECONDS) {
			dx_logging_info(L"No messages from server for at least %d seconds (%d missing heartbeats). Disconnecting...",
				SERVER_HEARTBEAT_TIMEOUT_IN_SECONDS, MAX_MISSING_HEARTBEATS);
			dx_close_socket(context);
			dx_sleep(s_idle_timeout);
			continue;
		}
		if (difftime(time(NULL), context->next_heartbeat) >= 0) {
			if (!dx_send_heartbeat(context->connection, true)) {
				dx_sleep(s_idle_timeout);
				continue;
			}
			time(&context->next_heartbeat);
			context->next_heartbeat += HEARTBEAT_TIMEOUT_IN_SECONDS;
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

		if (IS_FLAG_SET(context->set_fields_flags, DUMPING_RAW_DATA_FIELD_FLAG)) {
			number_of_bytes_read = (int)fread((void*)read_buf, sizeof(char), READ_CHUNK_SIZE, context->raw_dump_file);

			if (feof(context->raw_dump_file)) {
				number_of_bytes_read = INVALID_DATA_SIZE;
				context->set_fields_flags |= READER_THREAD_FIELD_FLAG;
				dx_notify_conn_termination(context, &is_thread_idle);
				context->reader_thread_termination_trigger = true;
			}

			if (ferror(context->raw_dump_file)) {
				dx_logging_error(L"Raw data file read error.");
				number_of_bytes_read = INVALID_DATA_SIZE;
			}
		} else {
			atomic_write_time(&context->last_server_heartbeat, time(NULL));
#ifdef DXFEED_CODEC_TLS_ENABLED
			if (dx_get_current_address(context)->tls.enabled) {
				number_of_bytes_read = (int)tls_read(context->tls_context, (void*)read_buf, READ_CHUNK_SIZE);
				if (number_of_bytes_read == -1)
					dx_logging_ansi_error(tls_error(context->tls_context));
			} else {
				number_of_bytes_read = dx_recv(context->s, (void*)read_buf, READ_CHUNK_SIZE);
			}
#else
			number_of_bytes_read = dx_recv(context->s, (void*)read_buf, READ_CHUNK_SIZE);
#endif // DXFEED_CODEC_TLS_ENABLED
            atomic_write_time(&context->last_server_heartbeat, 0);
		}

		if (number_of_bytes_read == INVALID_DATA_SIZE) {
			context->reader_thread_state = false;

			continue;
		}

		/* reporting the read data */
		context->reader_thread_state = context_data->receiver(context->connection, (const void*)read_buf, number_of_bytes_read);
	}
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
	DX_ARRAY_SHUFFLE(addrs->elements, dx_ext_address_t, addrs->size);
}

/* -------------------------------------------------------------------------- */

static bool dx_create_ext_address(const dx_address_t* source, OUT dx_ext_address_t* dest) {
	if (source == NULL || dest == NULL)
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	dx_memset((void*)dest, 0, sizeof(dx_ext_address_t));

	if (source->host != NULL) {
		dest->host = dx_ansi_create_string_src(source->host);
		if (dest->host == NULL)
			return false;
	}

	if (source->port != NULL) {
		dest->port = dx_ansi_create_string_src(source->port);
		if (dest->port == NULL)
			return false;
	}

	if (source->username != NULL) {
		dest->username = dx_ansi_create_string_src(source->username);
		if (dest->username == NULL)
			return false;
	}

	if (source->password != NULL) {
		dest->password = dx_ansi_create_string_src(source->password);
		if (dest->password == NULL)
			return false;
	}

	if (!dx_codec_tls_copy(&(source->tls), OUT &(dest->tls)) ||
		!dx_codec_gzip_copy(&(source->gzip), OUT &(dest->gzip)))
		return false;

#ifdef DXFEED_CODEC_TLS_ENABLED
	if (dest->tls.enabled && dest->tls.trust_store == NULL)
		dest->tls.trust_store = dx_ansi_create_string_src(TRUST_STORE_DEFAULT_NAME);
#endif // !DXFEED_CODEC_TLS_ENABLED

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_resolve_address(dx_network_connection_context_t* context) {
	struct addrinfo hints;
	size_t i = 0;
	FILE* raw_dump_file = NULL;
	dx_address_array_t addresses;
	int resolved_count = 0;

	if (context == NULL) {
		dx_set_last_error(dx_ec_invalid_func_param_internal);

		return false;
	}

	//check raw data file is used
	if (IS_FLAG_SET(context->set_fields_flags, DUMPING_RAW_DATA_FIELD_FLAG))
		return true;
	//check address is local file
	raw_dump_file = fopen(context->address, "rb");
	if (raw_dump_file != NULL) {
		fclose(raw_dump_file);
		context->set_fields_flags |= DUMPING_RAW_DATA_FIELD_FLAG;
		return true;
	}

	dx_sleep_before_resolve(context);

	CHECKED_CALL_2(dx_get_addresses_from_collection, context->address, &addresses);
	if (addresses.size == 0)
		return false;

	dx_memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	context->addr_context.elements_to_free = dx_calloc(addresses.size, sizeof(dx_addrinfo_ptr));

	if (context->addr_context.elements_to_free == NULL) {
		dx_clear_address_array(&addresses);
		return false;
	}

	for (i = 0; i < addresses.size; ++i) {
		dx_addrinfo_ptr addr = NULL;
		dx_addrinfo_ptr cur_addr = NULL;
		dx_ext_address_t address;
		bool failed = false;

		if (!dx_create_ext_address(&(addresses.elements[i]), OUT &address)) {
			dx_clear_addr_context_data(&(context->addr_context));
			dx_clear_address_array(&addresses);
			return false;
		}

		DX_ARRAY_INSERT(context->addr_context, dx_ext_address_t, address, context->addr_context.size, dx_capacity_manager_halfer, failed);
		if (failed) {
			dx_clear_addr_context_data(&(context->addr_context));
			dx_clear_address_array(&addresses);
			return false;
		}

		if (!dx_getaddrinfo(addresses.elements[i].host, addresses.elements[i].port, &hints, &addr)) {
			continue;
		}

		cur_addr = context->addr_context.elements_to_free[context->addr_context.elements_to_free_count++] = addr;

		for (; cur_addr != NULL; cur_addr = cur_addr->ai_next) {
			bool failed = false;
			dx_ext_address_t* last = &(context->addr_context.elements[context->addr_context.size - 1]);

			DX_ARRAY_INSERT(*last, dx_addrinfo_ptr, cur_addr, last->size, dx_capacity_manager_halfer, failed);

			if (failed) {
				dx_clear_addr_context_data(&(context->addr_context));
				dx_clear_address_array(&addresses);
				return false;
			}
			resolved_count++;
		}
	}

	dx_clear_address_array(&addresses);

	//all addresses in collection cannot be resolved
	if (resolved_count == 0) {

		return false;
	}

	dx_shuffle_addrs(&(context->addr_context));

	return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_connect_via_socket(dx_network_connection_context_t* context, dx_ext_address_t* address) {
	for (; address->cur_info_index < address->size; ++address->cur_info_index) {
		dx_addrinfo_ptr cur_addr = address->elements[address->cur_info_index];

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

		dx_connection_status_set(context->connection, dxf_cs_connected);
		return true;
	}
	return false;
}

/* -------------------------------------------------------------------------- */

#ifdef DXFEED_CODEC_TLS_ENABLED
static bool dx_connect_via_tls(dx_network_connection_context_t* context, dx_ext_address_t* address) {
	context->tls_config = tls_config_new();
	if (context->tls_config == NULL) {
		/* failed to create TLS config */

		return false;
	}

	/* Configure protocol and ciphers */
	if (tls_config_set_protocols(context->tls_config, TLS_PROTOCOL_TLSv1_1 | TLS_PROTOCOL_TLSv1_2) < 0) {
		dx_logging_ansi_error(tls_config_error(context->tls_config));
		tls_config_free(context->tls_config);
		return false;
	}

	/* load root CA */
	if (address->tls.trust_store != NULL) {
		if (address->tls.trust_store_password == NULL) {
			if (tls_config_set_ca_file(context->tls_config, address->tls.trust_store) < 0) {
				dx_logging_ansi_error(tls_config_error(context->tls_config));
				tls_config_free(context->tls_config);
				return false;
			}
		} else {
			address->trust_store_mem = tls_load_file(address->tls.trust_store, &(address->trust_store_len), address->tls.trust_store_password);
			if (address->trust_store_mem == NULL ||
				tls_config_set_ca_mem(context->tls_config, address->trust_store_mem, address->trust_store_len) < 0) {
				tls_config_free(context->tls_config);
				return false;
			}
		}
	}
	/* load private certificate */
	if (address->tls.key_store != NULL) {
		if (address->tls.key_store_password == NULL) {
			if (tls_config_set_key_file(context->tls_config, address->tls.key_store) < 0) {
				dx_logging_ansi_error(tls_config_error(context->tls_config));
				tls_config_free(context->tls_config);
				return false;
			}
		} else {
			address->key_store_mem = tls_load_file(address->tls.key_store, &(address->key_store_len), address->tls.key_store_password);
			if (address->key_store_mem == NULL ||
				tls_config_set_key_mem(context->tls_config, address->key_store_mem, address->key_store_len) < 0) {
				tls_config_free(context->tls_config);
				return false;
			}
		}
	}

	context->tls_context = tls_client();
	if (context->tls_context == NULL) {
		/* failed to create TLS context */
		tls_config_free(context->tls_config);

		return false;
	}
	if (tls_configure(context->tls_context, context->tls_config) < 0 ||
		tls_connect(context->tls_context, address->host, address->port) < 0) {

		dx_logging_ansi_error(tls_error(context->tls_context));

		/* failed to configure or connect */
		tls_config_free(context->tls_config);
		tls_free(context->tls_context);

		return false;
	}

	/* try perform handshake */
	if (tls_handshake(context->tls_context) < 0) {
		dx_logging_ansi_error(tls_error(context->tls_context));
		tls_config_free(context->tls_config);
		tls_free(context->tls_context);

		return false;
	}

	dx_connection_status_set(context->connection, dxf_cs_connected);
	return true;
}
#endif // DXFEED_CODEC_TLS_ENABLED

/* -------------------------------------------------------------------------- */

bool dx_connect_to_resolved_addresses(dx_network_connection_context_t* context) {
	dx_address_resolution_context_t* ctx = &(context->addr_context);

	//raw data file
	if (IS_FLAG_SET(context->set_fields_flags, DUMPING_RAW_DATA_FIELD_FLAG)) {
		if (context->raw_dump_file != NULL)
			return true;
		context->raw_dump_file = fopen(context->address, "rb");
		if (context->raw_dump_file == NULL) {
			dx_logging_error(L"Cannot open raw file!");
			return false;
		}
		return true;
	}

	CHECKED_CALL(dx_mutex_lock, &(context->socket_guard));

	for (; ctx->cur_addr_index < ctx->size; ++ctx->cur_addr_index) {
		dx_ext_address_t* cur_addr = &(ctx->elements[ctx->cur_addr_index]);

		if (cur_addr->is_connection_failed)
			continue;

		/* Make backup of properties configured via API on first pass */
		if (!IS_FLAG_SET(context->set_fields_flags, PROPERTIES_BACKUP_FLAG)) {
			if (!dx_protocol_property_make_backup(context)) {
				dx_mutex_unlock(&(context->socket_guard));
				return false;
			}
			context->set_fields_flags |= PROPERTIES_BACKUP_FLAG;
		} else {
			/* Restore properties configured via API before next connection will be open */
			if (!dx_protocol_property_restore_backup(context)) {
				dx_mutex_unlock(&(context->socket_guard));
				return false;
			}
		}

		if (cur_addr->username != NULL &&
			cur_addr->password != NULL &&
			!dx_property_map_contains(&context->properties, DX_PROPERTY_AUTH)) {
			if (!dx_protocol_configure_basic_auth(context->connection, cur_addr->username, cur_addr->password)) {
				dx_mutex_unlock(&(context->socket_guard));
				return false;
			}
		}

#ifdef DXFEED_CODEC_TLS_ENABLED
		if (cur_addr->tls.enabled) {
			if (dx_connect_via_tls(context, cur_addr))
				break;
		} else {
			if (dx_connect_via_socket(context, cur_addr))
				break;
		}
#else
		if (dx_connect_via_socket(context, cur_addr))
			break;
#endif // DXFEED_CODEC_TLS_ENABLED
	}

	if (ctx->cur_addr_index == ctx->size) {
		/* we failed to establish connection */

		dx_mutex_unlock(&(context->socket_guard));

		return dx_set_error_code(dx_nec_open_connection_error);
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
	// Cleanup task queue
	dx_cleanup_task_queue(context->tq);

	// if connection required login send credentials again
	if (dx_connection_status_get(context->connection) == dxf_cs_login_required) {
		CHECKED_CALL_2(dx_send_protocol_description, context->connection, false);
		CHECKED_CALL_2(dx_send_record_description, context->connection, false);
		CHECKED_CALL_2(dx_process_connection_subscriptions, context->connection, dx_server_event_subscription_refresher);
		return true;
	}

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

	CHECKED_CALL(dx_mutex_create, &context->socket_guard);

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

	if (!IS_FLAG_SET(context->set_fields_flags, SOCKET_FIELD_FLAG | DUMPING_RAW_DATA_FIELD_FLAG)) {
		dx_mutex_unlock(&(context->socket_guard));

		return dx_set_error_code(dx_nec_connection_closed);
	}

	do {
		int sent_count = INVALID_DATA_SIZE;
		if (IS_FLAG_SET(context->set_fields_flags, DUMPING_RAW_DATA_FIELD_FLAG)) {
			sent_count = buffer_size;
		} else {
#ifdef DXFEED_CODEC_TLS_ENABLED
			dx_ext_address_t* current_address = dx_get_current_address(context);

			// prevent sending while reconnecting
			if (current_address != NULL) {
				if (current_address->tls.enabled) {
					sent_count = (int)tls_write(context->tls_context, (const void*)char_buf, buffer_size);
					if (sent_count == -1)
						dx_logging_ansi_error(tls_error(context->tls_context));
				} else {
					sent_count = dx_send(context->s, (const void*)char_buf, buffer_size);
				}
			}
#else
			sent_count = dx_send(context->s, (const void*)char_buf, buffer_size);
#endif // DXFEED_CODEC_TLS_ENABLED
		}

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
 *	Connection status functions
 */
/* -------------------------------------------------------------------------- */

const dxf_char_t* dx_get_connection_status_string(dxf_connection_status_t status) {
	static dxf_char_t* strings[(size_t)(dxf_cs_authorized + 1)] =
			{L"Not Connected", L"Connected", L"Login Required", L"Authorized"};
	static dxf_char_t* unknown = L"Unknown";

	if (status < 0 || status > dxf_cs_authorized) {
		return unknown;
	}

	return strings[(size_t)status];
}

void dx_connection_status_set(dxf_connection_t connection, dxf_connection_status_t status) {
	dx_network_connection_context_t* context = NULL;
	bool res = true;

	context = dx_get_subsystem_data(connection, dx_ccs_network, &res);

	if (context == NULL) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return;
	}

	dxf_connection_status_t old_status;

	dx_mutex_lock(&(context->status_guard));
	old_status = context->status;
	dx_logging_verbose_info(L"Connection status changed %d (%ls) -> %d (%ls)",
							old_status, dx_get_connection_status_string(old_status),
							status, dx_get_connection_status_string(status));

	context->status = status;
	dx_mutex_unlock(&(context->status_guard));

	dxf_conn_status_notifier_t conn_status_notifier = context->context_data.conn_status_notifier;

	if (conn_status_notifier == NULL) {
		return;
	}

	conn_status_notifier(connection, old_status, status, context->context_data.notifier_user_data);
}

/* -------------------------------------------------------------------------- */

dxf_connection_status_t dx_connection_status_get(dxf_connection_t connection) {
	dx_network_connection_context_t* context = NULL;
	dxf_connection_status_t status = dxf_cs_not_connected;
	bool res = true;

	context = dx_get_subsystem_data(connection, dx_ccs_network, &res);

	if (context == NULL) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}

		return status;
	}

	CHECKED_CALL(dx_mutex_lock, &(context->status_guard));
	status = context->status;
	dx_mutex_unlock(&(context->status_guard));

	return status;
}

/* -------------------------------------------------------------------------- */
/*
 *	Protocol properties functions
 */
/* -------------------------------------------------------------------------- */

static void dx_protocol_property_clear(dx_network_connection_context_t* context) {
	if (context == NULL)
		return;
	dx_property_map_free_collection(&context->properties);
	dx_property_map_free_collection(&context->prop_backup);
}

/* -------------------------------------------------------------------------- */

static bool dx_protocol_property_make_backup(dx_network_connection_context_t* context) {
	if (context == NULL)
		return dx_set_error_code(dx_ec_invalid_func_param_internal);

	return dx_property_map_clone(&context->properties, &context->prop_backup);
}

/* -------------------------------------------------------------------------- */

static bool dx_protocol_property_restore_backup(dx_network_connection_context_t* context) {
	if (context == NULL)
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	dx_property_map_free_collection(&context->properties);
	return dx_property_map_clone(&context->prop_backup, &context->properties);
}

/* -------------------------------------------------------------------------- */

bool dx_protocol_property_get_snapshot(dxf_connection_t connection,
                 OUT dxf_property_item_t** ppProperties, OUT int* pSize)
{	
	if (ppProperties == NULL || pSize == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param);
	}
	*ppProperties = NULL;
	*pSize = 0;

	bool res = true;
	const dx_network_connection_context_t* const pContext = dx_get_subsystem_data(connection, dx_ccs_network, &res);
	if (pContext == NULL) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}
		return false;
	}
	CHECKED_CALL(dx_mutex_lock, &pContext->socket_guard);
	const size_t size = pContext->properties.size;
	const dx_property_item_t* const pElements = pContext->properties.elements;
	dxf_property_item_t* const pProperties = dx_calloc(size, sizeof(dxf_property_item_t));
	if (pProperties == NULL) {
		dx_mutex_unlock(&pContext->socket_guard);
		return false;
	}
	for (int i = 0; i < size; i++) {
		pProperties[i].key = dx_create_string_src(pElements[i].key);
		pProperties[i].value = dx_create_string_src(pElements[i].value);
	}
	if (!dx_mutex_unlock(&pContext->socket_guard)) {
		dxf_free_connection_properties_snapshot(pProperties, (int)size);
		return false;
	}
	*pSize = (int)size;
	*ppProperties = pProperties;
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

	return dx_property_map_set(&context->properties, key, value);
}

/* -------------------------------------------------------------------------- */

bool dx_protocol_property_set_many(dxf_connection_t connection, const dx_property_map_t* other) {
	dx_network_connection_context_t* context = NULL;
	bool res = true;

	context = dx_get_subsystem_data(connection, dx_ccs_network, &res);

	if (context == NULL) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}
		return false;
	}

	return dx_property_map_set_many(&context->properties, other);
}

/* -------------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------- */

bool dx_protocol_property_contains(dxf_connection_t connection, dxf_const_string_t key) {
	dx_network_connection_context_t* context = NULL;
	bool res = true;

	context = dx_get_subsystem_data(connection, dx_ccs_network, &res);

	if (context == NULL) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}
		return false;
	}

	return dx_property_map_contains(&context->properties, key);
}

/* -------------------------------------------------------------------------- */

char* dx_protocol_get_basic_auth_data(const char* user, const char* password) {
	size_t credentials_size;
	size_t base64_size;
	char* credentials_buf;
	char* base64_buf;

	if (user == NULL || password == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		return NULL;
	}

	/* The buffer stores user and password as <username>:<password>. The length
	of the buffer is sum of user and password length and addtional ':' symbol. */
	credentials_size = strlen(user) + strlen(password) + 1;
	credentials_buf = dx_ansi_create_string(credentials_size);
	if (sprintf(credentials_buf, "%s:%s", user, password) <= 0) {
		dx_free(credentials_buf);
		return NULL;
	}

	base64_size = dx_base64_length(credentials_size);
	base64_buf = dx_ansi_create_string(base64_size);
	if (!dx_base64_encode((const char*)credentials_buf, credentials_size, base64_buf, base64_size)) {
		dx_free(base64_buf);
		dx_free(credentials_buf);
		return NULL;
	}

	dx_free(credentials_buf);

	return base64_buf;
}

/* -------------------------------------------------------------------------- */

bool dx_protocol_configure_basic_auth(dxf_connection_t connection,
									const char* user, const char* password) {
	bool res = true;
	char* base64_buf = dx_protocol_get_basic_auth_data(user, password);
	if (base64_buf == NULL)
		return false;

	res = dx_protocol_configure_custom_auth(connection, DX_AUTH_BASIC_KEY, base64_buf);

	dx_free(base64_buf);
	return res;
}

/* -------------------------------------------------------------------------- */

bool dx_protocol_configure_custom_auth(dxf_connection_t connection,
									const char* authscheme,
									const char* authdata) {
	size_t buf_size;
	dxf_string_t buf;
	dxf_string_t w_authscheme;
	dxf_string_t w_authdata;
	bool res = true;

	if (connection == NULL || authscheme == NULL || authdata == NULL)
		return dx_set_error_code(dx_ec_invalid_func_param);

	/* This buffer stores 'authorization' value which is [<SchemeName> <SchemeData>].
	Total size of this buffer is sum of 'authscheme' and 'authdata' lengths, space
	between this values and also trailing zero.
	*/
	buf_size = strlen(authscheme) + strlen(authdata) + 2;
	buf = dx_create_string(buf_size);
	w_authscheme = dx_ansi_to_unicode(authscheme);
	w_authdata = dx_ansi_to_unicode(authdata);
	swprintf(buf, buf_size, L"%ls %ls", w_authscheme, w_authdata);

	res = dx_protocol_property_set(connection, DX_PROPERTY_AUTH, buf);

	dx_free(w_authdata);
	dx_free(w_authscheme);
	dx_free(buf);
	return res;
}

bool dx_get_current_connected_address(dxf_connection_t connection, OUT char** ppAddress) {
	if (ppAddress == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param);
	}
	*ppAddress = NULL;
	bool res = true;
	dx_network_connection_context_t* const pContext = dx_get_subsystem_data(connection, dx_ccs_network, &res);
	if (pContext == NULL) {
		if (res) {
			dx_set_error_code(dx_cec_connection_context_not_initialized);
		}
		return false;
	}
	CHECKED_CALL(dx_mutex_lock, &pContext->socket_guard);
	if (pContext->addr_context.cur_addr_index < 0 || pContext->addr_context.cur_addr_index >= pContext->addr_context.size) {
		dx_mutex_unlock(&pContext->socket_guard);
		return true;
	}
	const dx_ext_address_t* const pExtAddress = &pContext->addr_context.elements[pContext->addr_context.cur_addr_index];
	if (pExtAddress == NULL) {
		dx_mutex_unlock(&pContext->socket_guard);
		return true;
	}
	const char* const host = pExtAddress->host;
	const char* const port = pExtAddress->port;
	const size_t host_len = dx_ansi_string_length(host);
	const size_t port_len = dx_ansi_string_length(port);
	char * const pAddress = dx_calloc(1, host_len + port_len + 2);
	if (pAddress == NULL) {
		dx_mutex_unlock(&pContext->socket_guard);
		return false;
	}
	dx_ansi_copy_string_len(pAddress, host, host_len);
	pAddress[host_len] = ':';
	dx_ansi_copy_string_len(pAddress + host_len + 1, port, port_len);
	pAddress[host_len + port_len + 1] = '\0';
	if (!dx_mutex_unlock(&pContext->socket_guard)) {
		free(pAddress);
		return false;
	}
	*ppAddress = pAddress;
	return true;
}
