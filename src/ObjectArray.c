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

#include "DXAlgorithms.h"
#include "DXErrorHandling.h"
#include "ObjectArray.h"

#define DX_OBJECT_ARRAY_ADD_BODY(type, alias) \
DX_OBJECT_ARRAY_ADD_PROTOTYPE(type, alias) {\
	bool failed = false; \
	\
	if (object_array == NULL) { \
		return dx_set_error_code(dx_ec_invalid_func_param_internal); \
	} \
	\
	DX_ARRAY_INSERT(*object_array, type, obj, object_array->size, dx_capacity_manager_halfer, failed); \
	\
	return !failed; \
}

#define DX_OBJECT_ARRAY_FREE_BODY(alias, free_function) \
DX_OBJECT_ARRAY_FREE_PROTOTYPE(alias) { \
	size_t i = 0; \
	\
	if (object_array == NULL) { \
		return;\
	}\
	\
	for (; i < object_array->size; ++i) { \
		free_function(object_array->elements[i]); \
	} \
	\
	if (object_array->elements != NULL) { \
		dx_free((void*)object_array->elements); \
	} \
	\
	object_array->elements = NULL; \
	object_array->size = 0; \
	object_array->capacity = 0; \
}

void dx_free_string(dxf_const_string_t str) {
	dx_free((void*)str);
}

DX_OBJECT_ARRAY_ADD_BODY(dxf_const_string_t, string)
DX_OBJECT_ARRAY_FREE_BODY(string, dx_free_string)

void dx_free_byte_array(dxf_byte_array_t byte_array) {
	dx_free(byte_array.elements);
	byte_array.elements = NULL;
	byte_array.size = 0;
	byte_array.capacity = 0;
}

DX_OBJECT_ARRAY_ADD_BODY(dxf_byte_array_t, byte_buffer)
DX_OBJECT_ARRAY_FREE_BODY(byte_buffer, dx_free_byte_array)