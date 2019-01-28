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
 *  the record data
 */

#ifndef RECORD_BUFFERS_H_INCLUDED
#define RECORD_BUFFERS_H_INCLUDED

#include "PrimitiveTypes.h"
#include "EventData.h"

/* -------------------------------------------------------------------------- */
/*
 *	Connection context functions
 */
/* -------------------------------------------------------------------------- */

void* dx_get_record_buffers_connection_context (dxf_connection_t connection);

/* -------------------------------------------------------------------------- */
/*
 *	Buffer manager functions prototypes
 */
/* -------------------------------------------------------------------------- */

typedef void* (*dx_get_record_ptr_t)(void* context, int record_index);
typedef void* (*dx_get_record_buffer_ptr_t)(void* context);

/* -------------------------------------------------------------------------- */
/*
 *	Buffer manager collection
 */
/* -------------------------------------------------------------------------- */

typedef struct {
	dx_get_record_ptr_t record_getter;
	dx_get_record_buffer_ptr_t record_buffer_getter;
} dx_buffer_manager_collection_t;

extern const dx_buffer_manager_collection_t g_buffer_managers[dx_rid_count];

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary memory management functions
 */
/* -------------------------------------------------------------------------- */

bool dx_store_string_buffer (void* context, dxf_const_string_t buf);
bool dx_store_byte_array_buffer(void* context, dxf_byte_array_t buf);
void dx_free_buffers(void* context);

#endif /* RECORD_BUFFERS_H_INCLUDED */