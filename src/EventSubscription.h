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

#pragma once

#include "DXTypes.h"
#include "EventData.h"
#include "PrimitiveTypes.h"

/* -------------------------------------------------------------------------- */
/*
 *	Subscription defines
 */
/* -------------------------------------------------------------------------- */

extern const dxf_subscription_t dx_invalid_subscription;

extern const dxf_const_string_t dx_all_order_sources[];
extern const size_t dx_all_order_sources_count;
extern const dxf_const_string_t dx_all_special_order_sources[];
extern const size_t dx_all_special_order_sources_count;
extern const size_t dx_all_regional_count;

typedef enum dxf_special_order_source_t {
	dxf_sos_DEFAULT = 0,
	dxf_sos_COMPOSITE_BID = 1,
	dxf_sos_COMPOSITE_ASK = 2,
	dxf_sos_REGIONAL_BID = 3,
	dxf_sos_REGIONAL_ASK = 4,
	dxf_sos_AGGREGATE_BID = 5,
	dxf_sos_AGGREGATE_ASK = 6,
	dxf_sos_EMPTY = 7,
	dxf_sos_COMPOSITE = 8,
	dxf_sos_REGIONAL = 9,
	dxf_sos_AGGREGATE = 10,
} dxf_special_order_source_t;

/* -------------------------------------------------------------------------- */
/*
 *	Subscription types
 */
/* -------------------------------------------------------------------------- */

typedef int (*dx_subscription_processor_t)(dxf_connection_t connection, dx_order_source_array_ptr_t order_source,
										   dxf_const_string_t* symbols, size_t symbol_count, unsigned event_types,
										   dxf_uint_t subscr_flags, dxf_long_t time);

/* -------------------------------------------------------------------------- */
/*
 *	Subscription functions
 */
/* -------------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* returns dx_invalid_subscription on error */
dxf_subscription_t dx_create_event_subscription(dxf_connection_t connection, unsigned event_types,
												dx_event_subscr_flag subscr_flags, dxf_long_t time);
int dx_close_event_subscription(dxf_subscription_t subscr_id);
int dx_add_symbols(dxf_subscription_t subscr_id, dxf_const_string_t* symbols, int symbol_count);
// Adds symbols to the subscription and counts the number of unique added symbols that will be sent to the server for
// subscription.
int dx_add_symbols_v2(dxf_subscription_t subscr_id, dxf_const_string_t* symbols, int symbol_count,
					  int* added_symbols_indices, int* added_symbols_count);
int dx_remove_symbols(dxf_subscription_t subscr_id, dxf_const_string_t* symbols, size_t symbol_count);
int dx_add_listener(dxf_subscription_t subscr_id, dxf_event_listener_t listener, void* user_data);
int dx_add_listener_v2(dxf_subscription_t subscr_id, dxf_event_listener_v2_t listener, void* user_data);
int dx_remove_listener(dxf_subscription_t subscr_id, dxf_event_listener_t listener);
int dx_remove_listener_v2(dxf_subscription_t subscr_id, dxf_event_listener_v2_t listener);
int dx_get_subscription_connection(dxf_subscription_t subscr_id, OUT dxf_connection_t* connection);
int dx_get_event_subscription_event_types(dxf_subscription_t subscr_id, OUT unsigned* event_types);
int dx_get_event_subscription_symbols_count(dxf_subscription_t subscr_id, OUT size_t* symbol_count);
int dx_get_event_subscription_symbols(dxf_subscription_t subscr_id, OUT dxf_const_string_t** symbols,
									  OUT size_t* symbol_count);
int dx_get_event_subscription_flags(dxf_subscription_t subscr_id, OUT dx_event_subscr_flag* subscr_flags);
int dx_set_event_subscription_flags(dxf_subscription_t subscr_id, dx_event_subscr_flag subscr_flags);
int dx_get_event_subscription_time(dxf_subscription_t subscr_id, OUT dxf_long_t* time);
int dx_process_event_data(dxf_connection_t connection, dx_event_id_t event_id, dxf_const_string_t symbol_name,
						  dxf_const_event_data_t data, const dxf_event_params_t* event_params);
int dx_get_last_symbol_event(dxf_connection_t connection, dxf_const_string_t symbol_name, int event_type,
							 OUT dxf_event_data_t* event_data);

int dx_process_connection_subscriptions(dxf_connection_t connection, dx_subscription_processor_t processor);

/* Functions for working with order source */
int dx_add_order_source(dxf_subscription_t subscr_id, dxf_const_string_t source);
void dx_clear_order_sources(dxf_subscription_t subscr_id);
// Returns non-special order sources.
dx_order_source_array_ptr_t dx_get_order_sources(dxf_subscription_t subscr_id);

/* Functions for working with symbols*/
dxf_ulong_t dx_symbol_name_hasher(dxf_const_string_t symbol_name);

int dx_has_any_subscribed_symbol(dxf_connection_t connection);

#ifdef __cplusplus
}
#endif
