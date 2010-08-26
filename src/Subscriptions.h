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

typedef void* dx_subscription_t;

extern const dx_subscription_t dx_invalid_subscription;

enum dx_event_type_t {
    dx_et_quote = 0x1,
    dx_et_trade = 0x2,
    
    /*
     *	add new event type codes above this comment, use the bit-mask values,
     *  modify the first value below to remove the newly used bits from the
     *  exclusion bit-mask.
     */
    
    dx_et_unused = ~0x3,
    dx_et_all = -1
};

struct dx_quote_data_t {
    int dummy;
};

struct dx_trade_data_t {
    int dummy;
};

struct dx_event_data_t {
    int dummy;
};

typedef void (*dx_quote_listener_t)(enum dx_event_type_t event_type, const char* symbol_name, const struct dx_quote_data_t* data);
typedef void (*dx_trade_listener_t)(enum dx_event_type_t event_type, const char* symbol_name, const struct dx_trade_data_t* data);
typedef void (*dx_event_listener_t)(enum dx_event_type_t event_type, const char* symbol_name, const struct dx_event_data_t* data);

/* -------------------------------------------------------------------------- */
/*
 *	Subscription functions
 */
/* -------------------------------------------------------------------------- */

dx_subscription_t dx_create_subscription (int event_types);
bool dx_close_subscription (dx_subscription_t subscr_id);
bool dx_add_symbols (dx_subscription_t subscr_id, const char** symbols, size_t symbol_count);
bool dx_remove_symbols (dx_subscription_t subscr_id, const char** symbols, size_t symbol_count);
bool dx_add_listener (dx_subscription_t subscr_id, dx_event_listener_t listener, enum dx_event_type_t event_type);
bool dx_remove_listener (dx_subscription_t subscr_id, dx_event_listener_t listener);


#endif /* SUBSCRIPTIONS_H_INCLUDED */