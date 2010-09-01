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
 *  Here we have the data structures passed along with symbol events
 */
 
 #ifndef EVENT_DATA_H_INCLUDED
 #define EVENT_DATA_H_INCLUDED
 
/* -------------------------------------------------------------------------- */
/*
 *	Event type constants
 
 *  add new values here for new event types, use the next power-of-two
 *  value available and modify the exclusion mask
 */
/* -------------------------------------------------------------------------- */

#define DX_ET_QUOTE    (0x1)
#define DX_ET_TRADE    (0x2)
#define DX_ET_UNUSED   (~0x3)

/* -------------------------------------------------------------------------- */
/*
 *	Event data structures
 */
/* -------------------------------------------------------------------------- */

typedef void* dx_event_data_t;

struct dx_quote_data_t {
    int dummy;
    /* quote specific fields */
};

struct dx_trade_data_t {
    int dummy;
    /* trade specific fields */
};

/* -------------------------------------------------------------------------- */
/*
 *	Event listener prototype
 */
/* -------------------------------------------------------------------------- */

typedef void (*dx_event_listener_t)(int event_type, dx_const_string_t symbol_name,
                                    const dx_event_data_t* data, int data_count);

 
#endif /* EVENT_DATA_H_INCLUDED */