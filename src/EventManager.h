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

#ifndef EVENT_MANAGER_H_INCLUDED
#define EVENT_MANAGER_H_INCLUDED

#include "DXTypes.h"
#include "EventData.h"
#include "PrimitiveTypes.h"
#include "ObjectArray.h"

/* -------------------------------------------------------------------------- */
/*
 *	Event objects management functions
 */
/* -------------------------------------------------------------------------- */

typedef dxf_bool_t(*dx_event_copy_function_t) (const dxf_event_data_t source,
	OUT dx_string_array_ptr_t* string_buffer,
	OUT dxf_event_data_t* new_obj);
typedef void(*dx_event_free_function_t) (dxf_event_data_t obj);

dx_event_copy_function_t dx_get_event_copy_function(dx_event_id_t event_id);
dx_event_free_function_t dx_get_event_free_function(dx_event_id_t event_id);

#endif /* EVENT_MANAGER_H_INCLUDED */