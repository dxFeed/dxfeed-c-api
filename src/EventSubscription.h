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

#ifndef SUBSCRIPTIONS_H_INCLUDED
#define SUBSCRIPTIONS_H_INCLUDED

#include "PrimitiveTypes.h"
#include "EventData.h"

extern const dxf_subscription_t dx_invalid_subscription;

/* -------------------------------------------------------------------------- */
/*
 *	Subscription functions
 */
/* -------------------------------------------------------------------------- */

// returns dx_invalid_subscription on error
dxf_subscription_t dx_create_event_subscription (int event_types);
bool dx_close_event_subscription (dxf_subscription_t subscr_id);
bool dx_add_symbols (dxf_subscription_t subscr_id, dx_const_string_t* symbols, size_t symbol_count);
bool dx_remove_symbols (dxf_subscription_t subscr_id, dx_const_string_t* symbols, size_t symbol_count);
bool dx_add_listener (dxf_subscription_t subscr_id, dx_event_listener_t listener);
bool dx_remove_listener (dxf_subscription_t subscr_id, dx_event_listener_t listener);
bool dx_get_event_subscription_event_types (dxf_subscription_t subscr_id, OUT int* event_types);
bool dx_process_event_data (int event_type, dx_const_string_t symbol_name, dx_int_t symbol_cipher,
                            const dx_event_data_t* data, int data_count);

#endif /* SUBSCRIPTIONS_H_INCLUDED */