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

#ifndef SERVER_MESSAGE_PROCESSOR_H_INCLUDED
#define SERVER_MESSAGE_PROCESSOR_H_INCLUDED

#include "PrimitiveTypes.h"
#include "DXTypes.h"
#include "BufferedIOCommon.h"
#include "DXPMessageData.h"

/* -------------------------------------------------------------------------- */
/*
 *	Server synchronization functions
 */
/* -------------------------------------------------------------------------- */

bool dx_clear_server_info (dxf_connection_t connection);

/* -------------------------------------------------------------------------- */
/*
 *	Describe protocol and message support functions
 */
/* -------------------------------------------------------------------------- */

typedef enum {
    dx_mss_supported,
    dx_mss_not_supported,
    dx_mss_pending
} dx_message_support_status_t;

bool dx_is_message_supported_by_server (dxf_connection_t connection, dx_message_type_t msg,
                                        OUT dx_message_support_status_t* status);
bool dx_describe_protocol_sent (dxf_connection_t connection);

/* -------------------------------------------------------------------------- */
/*
 *	Low level network data receiver
 */
/* -------------------------------------------------------------------------- */

bool dx_socket_data_receiver (dxf_connection_t connection, const void* buffer, int buffer_size);

#endif /* SERVER_MESSAGE_PROCESSOR_H_INCLUDED */