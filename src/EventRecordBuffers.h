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
 *	Contains the functionality for managing the memory required to store
 *  the event record data
 */
 
#ifndef EVENT_RECORD_BUFFERS_H_INCLUDED
#define EVENT_RECORD_BUFFERS_H_INCLUDED

#include "PrimitiveTypes.h"
#include "EventData.h"

/* -------------------------------------------------------------------------- */
/*
 *	Buffer manager functions prototypes
 */
/* -------------------------------------------------------------------------- */

typedef void* (*dx_get_record_ptr_t)(size_t record_index);
typedef void* (*dx_get_record_buffer_ptr_t)(void);

/* -------------------------------------------------------------------------- */
/*
 *	Buffer manager collection
 */
/* -------------------------------------------------------------------------- */

typedef struct {
    dx_get_record_ptr_t record_getter;
    dx_get_record_buffer_ptr_t record_buffer_getter;
} dx_buffer_manager_collection_t;

extern const dx_buffer_manager_collection_t g_buffer_managers[dx_eid_count];

void dx_clear_event_record_buffers (void);

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary memory management functions
 */
/* -------------------------------------------------------------------------- */

bool dx_store_string_buffer (dx_const_string_t buf);
void dx_free_string_buffers (void);

#endif /* EVENT_RECORD_BUFFERS_H_INCLUDED */