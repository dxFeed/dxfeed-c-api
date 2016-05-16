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

#ifndef SNAPSHOT_H_INCLUDED
#define SNAPSHOT_H_INCLUDED

#include "PrimitiveTypes.h"
#include "EventData.h"
#include "DXTypes.h"

extern const dxf_snapshot_t dx_invalid_snapshot;

/* -------------------------------------------------------------------------- */
/*
*	Subscription types
*/
/* -------------------------------------------------------------------------- */

//typedef bool(*dx_subscription_processor_t) (dxf_connection_t connection, dx_order_source_array_ptr_t order_source,
//    dxf_const_string_t* symbols, int symbol_count, int event_types);

/* -------------------------------------------------------------------------- */
/*
*	Subscription functions
*/
/* -------------------------------------------------------------------------- */

/* returns dx_invalid_snapshot on error */
dxf_snapshot_t dx_create_snapshot(dxf_connection_t connection, dx_event_id_t event_id, dxf_const_string_t symbol);
bool dx_close_snapshot(dxf_snapshot_t snapshot);
bool dx_add_snapshot_listener(dxf_snapshot_t snapshot, dxf_snapshot_listener_t listener, void* user_data);
bool dx_remove_snapshot_listener(dxf_snapshot_t snapshot, dxf_snapshot_listener_t listener);
bool dx_get_snapshot_subscription(dxf_snapshot_t snapshot, OUT dxf_subscription_t *subscription);

#endif /* SNAPSHOT_H_INCLUDED */