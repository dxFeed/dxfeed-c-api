/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 2.0 (the "License"); you may not use this file except in compliance with
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

#ifndef CLIENT_MESSAGE_SENDER_H_INCLUDED
#define CLIENT_MESSAGE_SENDER_H_INCLUDED

#include "DXPMessageData.h"
#include "EventData.h"
#include "PrimitiveTypes.h"

int dx_load_events_for_subscription(dxf_connection_t connection, dx_order_source_array_ptr_t order_sources,
									int event_types, dxf_uint_t subscr_flags);
int dx_subscribe_symbols_to_events(dxf_connection_t connection, dx_order_source_array_ptr_t order_sources,
								   dxf_const_string_t* symbols, size_t symbol_count, int* symbols_indices_to_subscribe,
								   int symbols_indices_count, int event_types, int unsubscribe, int task_mode,
								   dxf_uint_t subscr_flags, dxf_long_t time);

int dx_send_record_description(dxf_connection_t connection, int task_mode);
int dx_send_protocol_description(dxf_connection_t connection, int task_mode);
int dx_send_heartbeat(dxf_connection_t connection, int task_mode);

#endif /* CLIENT_MESSAGE_SENDER_H_INCLUDED */
