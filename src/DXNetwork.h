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
 *	Functions, structures, types etc to operate the network
 */

#ifndef DX_NETWORK_H_INCLUDED
#define DX_NETWORK_H_INCLUDED

#include "PrimitiveTypes.h"
#include "DXFeed.h"
#include "DXProperties.h"
#include "TaskQueue.h"

#define DX_AUTH_KEY "authorization"
#define DX_AUTH_BASIC_KEY "Basic"
#define DX_AUTH_BEARER_KEY "Bearer"

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary structures and types
 */
/* -------------------------------------------------------------------------- */

typedef bool (*dx_socket_data_receiver_t) (dxf_connection_t connection, const void* buffer, int buffer_size);

typedef struct {
	dx_socket_data_receiver_t receiver; /* a callback to pass the read data to */
	dxf_conn_termination_notifier_t notifier; /* a callback to notify client the current connection is to be finished and reestablished */
	dxf_conn_status_notifier_t conn_status_notifier; /* the callback to inform the client side that the connection status has changed */
	dxf_socket_thread_creation_notifier_t stcn; /* a callback that's called on a socket thread creation */
	dxf_socket_thread_destruction_notifier_t stdn; /* a callback that is called on a socket thread destruction */
	void* notifier_user_data; /* the user data passed to the notifier callbacks */
} dx_connection_context_data_t;

/* -------------------------------------------------------------------------- */
/*
 *	Advanced network functions
 */
/* -------------------------------------------------------------------------- */
/*
 *  Binds the connection to the specified address with the given context.

	Input:
		connection - the connection object to bind.
		address - a string containing the host address; should conform to the
			following pattern: <host>[:<port>], where <host> may either
			be a computer name or a domain name or an IP address string,
			and <port> is a text representation of a port number.
		ccd - a pointer to the connection context data structure, which must have
			a 'receiver' field assigned a non-NULL value.

	Return value:
		true - the connection has been successfully bound to the host.
		false - some error occurred, use 'dx_get_last_error' for details.
 */

bool dx_bind_to_address (dxf_connection_t connection, const char* address,
						const dx_connection_context_data_t* ccd);

/* -------------------------------------------------------------------------- */
/*
 *  Sends a portion data via a previously bound connection.

	Input:
		connection - a handle of a previously bound connection.
		buffer - a pointer to a buffer containing data to send; must not be NULL.
		buffer_length - a size of data (in bytes) to send.

	Return value:
		true - OK.
		false - some error occurred, use 'dx_get_last_error' for details.
 */

bool dx_send_data (dxf_connection_t connection, const void* buffer, int buffer_size);

/* -------------------------------------------------------------------------- */
/*
 *	Adds a task for a socket reader thread. The task is put into a special queue.
 *  The task queue is executed on each iteration of the internal infinite loop
 *  right before the socket read operation.
 *  The tasks are responsible for defining a moment when they are no longer needed
 *  by returning a corresponding flag.

	Input:
		connection - a handle of a previously bound connection.
		processor - a task processor function.
		data - the user-defined data to pass to the task processor.

	Return value:
		true - OK.
		false - some error occurred, use 'dx_get_last_error' for details.
 */

bool dx_add_worker_thread_task (dxf_connection_t connection, dx_task_processor_t processor, void* data);

/* -------------------------------------------------------------------------- */
/*
 *	Connection status functions
 */
/* -------------------------------------------------------------------------- */

void dx_connection_status_set(dxf_connection_t connection, dxf_connection_status_t status);
dxf_connection_status_t dx_connection_status_get(dxf_connection_t connection);

/* -------------------------------------------------------------------------- */
/*
 *	Protocol properties functions
 */
/* -------------------------------------------------------------------------- */

bool dx_protocol_property_get_snapshot(dxf_connection_t connection,
							OUT dxf_property_item_t** ppProperties, OUT int* pSize);
bool dx_protocol_property_set(dxf_connection_t connection,
							dxf_const_string_t key, dxf_const_string_t value);
bool dx_protocol_property_set_many(dxf_connection_t connection,
								const dx_property_map_t* other);
const dx_property_map_t* dx_protocol_property_get_all(dxf_connection_t connection);
bool dx_protocol_property_contains(dxf_connection_t connection,
								dxf_const_string_t key);

char* dx_protocol_get_basic_auth_data(const char* user, const char* password);
bool dx_protocol_configure_basic_auth(dxf_connection_t connection,
									const char* user, const char* password);
bool dx_protocol_configure_custom_auth(dxf_connection_t connection,
									const char* authscheme,
									const char* authdata);

bool dx_get_current_connected_address(dxf_connection_t connection, OUT char** ppAddress);

#endif /* DX_NETWORK_H_INCLUDED */