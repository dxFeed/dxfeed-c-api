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

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary structures and types
 */
/* -------------------------------------------------------------------------- */

typedef bool (*dx_socket_data_receiver_t) (dxf_connection_t connection, const void* buffer, int buffer_size);

typedef struct {
    dx_socket_data_receiver_t receiver; /* a callback to pass the read data to */
    dxf_conn_termination_notifier_t notifier; /* a callback to notify client the dx_socket_reader is going to finish */
} dx_connection_context_t;

/* -------------------------------------------------------------------------- */
/*
 *	Advanced network functions
 */
/* -------------------------------------------------------------------------- */
/*
 *  Binds the connection to the specified connector with the given context.

    Input:
        connection - the connection object to bind.
        connector - a string containing the host address; should conform to the
               following pattern: <host>[:<port>], where <host> may either
               be a computer name or a domain name or an IP address string,
               and <port> is a text representation of a port number.
        cc - a pointer to the connection context structure, which must have
             a 'receiver' field assigned a non-NULL value.
             
    Return value:
        true - the connection has been successfully bound to the host.
        false - some error occurred, use 'dx_get_last_error' for details.        
 */

bool dx_bind_to_connector (dxf_connection_t connection, const char* connector,
                           const dx_connection_context_t* cc);
                           
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

#endif /* DX_NETWORK_H_INCLUDED */