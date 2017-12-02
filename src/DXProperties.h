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
 *	Contains various algorithms which may be used across the project
 */

#ifndef DX_PROPERTIES_H_INCLUDED
#define DX_PROPERTIES_H_INCLUDED

#include "PrimitiveTypes.h"
#include "DXTypes.h"

/* -------------------------------------------------------------------------- */
/*
 *	Property map functions implementations
 * Note: not fast map-dictionary realization, but enough for properties
 */
/* -------------------------------------------------------------------------- */

typedef struct {
	dxf_string_t key;
	dxf_string_t value;
} dx_property_item_t;

//Note: not fast map-dictionary realization, but enough for properties
typedef struct {
	dx_property_item_t* elements;
	size_t size;
	size_t capacity;
} dx_property_map_t;

void dx_property_map_free_collection(dx_property_map_t* props);
bool dx_property_map_clone(const dx_property_map_t* src, dx_property_map_t* dest);
bool dx_property_map_set(dx_property_map_t* props, dxf_const_string_t key, dxf_const_string_t value);
bool dx_property_map_set_many(dx_property_map_t* props, const dx_property_map_t* other);
bool dx_property_map_contains(const dx_property_map_t* props, dxf_const_string_t key);
bool dx_property_map_try_get_value(const dx_property_map_t* props, dxf_const_string_t key, OUT dxf_const_string_t* value);

#endif //DX_PROPERTIES_H_INCLUDED
