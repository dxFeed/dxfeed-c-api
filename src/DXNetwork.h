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

extern unsigned g_invalid_buffer_length;

typedef bool (*dx_socket_data_receiver_t)(const void* buffer, unsigned buflen);

struct dx_connection_context_t {
    dx_socket_data_receiver_t       receiver; /* a callback to pass the read data to */    
    dx_on_reader_thread_terminate_t terminator; /* a callback to notify client the dx_socket_reader is going to finished*/
};

/* -------------------------------------------------------------------------- */
/*
 *	Advanced network functions
 */
/* -------------------------------------------------------------------------- */
/*
 *  Creates a connection to the specified host with the given context.

    Input:
        host - a string containing the host address; should conform to the
               following pattern: <host>[:<port>], where <host> may either
               be a computer name or a domain name or an IP address string,
               and <port> is a text representation of a port number.
        cc - a pointer to the connection context structure, which must have
             a 'receiver' field assigned a non-NULL value.
             
    Return value:
        false - some error occurred, use 'dx_get_last_error' for details.
        true - a connection has been established.
 */

bool dx_create_connection (const char* host, const struct dx_connection_context_t* cc);
                           
/* -------------------------------------------------------------------------- */
/*
 *  Sends a portion data via a previously created connection.

    Input:        
        buffer - a pointer to a buffer containing data to send; must not be NULL.
        buflen - a size of data (in bytes) to send.
        
    Return value:
        true - OK.
        false - some error occurred, use 'dx_get_last_error' for details.
 */

bool dx_send_data (const void* buffer, unsigned buflen);

/* -------------------------------------------------------------------------- */
/*
 *  Closes a previously opened connection.

    Input:
        none.
           
    Return value:
        true - OK.
        false - some error occurred, use 'dx_get_last_error' for details.
 */

bool dx_close_connection ();

#endif /* DX_NETWORK_H_INCLUDED */