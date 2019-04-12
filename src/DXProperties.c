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
#include "DXErrorCodes.h"
#include "DXErrorHandling.h"
#include "DXProperties.h"

/* -------------------------------------------------------------------------- */
/*
 *	Property map functions implementations
 */
/* -------------------------------------------------------------------------- */

static void dx_property_map_free_item(void* obj) {
	if (obj == NULL)
		return;
	dx_property_item_t* item = (dx_property_item_t*)obj;
	if (item->key != NULL)
		dx_free(item->key);
	if (item->value != NULL)
		dx_free(item->value);
}

/* -------------------------------------------------------------------------- */

void dx_property_map_free_collection(dx_property_map_t* props) {
	dx_property_item_t* item = props->elements;
	size_t i;
	for (i = 0; i < props->size; i++) {
		dx_property_map_free_item((void*)item++);
	}
	dx_free(props->elements);
	props->elements = NULL;
	props->size = 0;
	props->capacity = 0;
}

/* -------------------------------------------------------------------------- */

static inline int dx_property_item_comparator(dx_property_item_t item1, dx_property_item_t item2) {
	return dx_compare_strings((dxf_const_string_t)item1.key, (dxf_const_string_t)item2.key);
}

/* -------------------------------------------------------------------------- */

bool dx_property_map_clone(const dx_property_map_t* src, dx_property_map_t* dest) {
	size_t i;
	bool failed = false;
	if (src == NULL || dest == NULL)
		return dx_set_error_code(dx_ec_invalid_func_param);

	dx_property_map_free_collection(dest);

	DX_ARRAY_RESERVE(*dest, dx_property_item_t, src->size, failed);
	if (failed)
		return false;
	for (i = 0; i < src->size; i++) {
		dx_property_item_t item;
		item.key = dx_create_string_src(src->elements[i].key);
		item.value = dx_create_string_src(src->elements[i].value);
		dest->elements[i] = item;
	}
	return true;
}

/* -------------------------------------------------------------------------- */

dxf_string_t dx_create_empty_string() {
	return (dxf_string_t)dx_calloc(1, sizeof(dxf_char_t));
}

/* -------------------------------------------------------------------------- */

bool dx_property_map_set(dx_property_map_t* props, dxf_const_string_t key, dxf_const_string_t value) {
	dx_property_item_t item = { (dxf_string_t)key, (dxf_string_t)value };
	bool found;
	size_t index;

	if (props == NULL || key == NULL || value == NULL)
		return dx_set_error_code(dx_ec_invalid_func_param);

	DX_ARRAY_BINARY_SEARCH(props->elements, 0, props->size, item, dx_property_item_comparator, found, index);
	if (found) {
		dx_property_item_t* item_ptr = props->elements + index;
		if (dx_compare_strings(item_ptr->value, value) == 0) {
			// map already contains such key-value pair
			return true;
		}
		dxf_string_t value_copy = dx_string_length(value) > 0 ? dx_create_string_src(value) : dx_create_empty_string();
		if (value_copy == NULL)
			return false;
		dx_free(item_ptr->value);
		item_ptr->value = value_copy;
	} else {
		dx_property_item_t new_item = { dx_create_string_src(key), dx_string_length(value) > 0 ? dx_create_string_src(value) : dx_create_empty_string() };
		bool insertion_failed;
		if (new_item.key == NULL || new_item.value == NULL) {
			dx_property_map_free_item((void*)&new_item);
			return false;
		}
		DX_ARRAY_INSERT(*props, dx_property_item_t, new_item, index, dx_capacity_manager_halfer, insertion_failed);
		if (insertion_failed) {
			dx_property_map_free_item((void*)&new_item);
			return false;
		}
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_property_map_set_many(dx_property_map_t* props, const dx_property_map_t* other) {
	size_t i;
	dx_property_map_t temp = { 0 };

	if (props == NULL || other == NULL)
		return dx_set_error_code(dx_ec_invalid_func_param);

	if (!dx_property_map_clone(props, &temp))
		return false;

	for (i = 0; i < other->size; i++) {
		dx_property_item_t* item = &other->elements[i];
		if (!dx_property_map_set(&temp, item->key, item->value)) {
			dx_property_map_free_collection(&temp);
			return false;
		}
	}

	dx_property_map_free_collection(props);
	props->elements = temp.elements;
	props->size = temp.size;
	props->capacity = temp.capacity;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_property_map_contains(const dx_property_map_t* props, dxf_const_string_t key) {
	dx_property_item_t item = { (dxf_string_t)key, NULL };
	bool found;
	size_t index;

	if (props == NULL)
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	if (key == NULL)
		return dx_set_error_code(dx_ec_invalid_func_param);

	DX_ARRAY_BINARY_SEARCH(props->elements, 0, props->size, item, dx_property_item_comparator, found, index);

	(void)index;
	return found;
}

/* -------------------------------------------------------------------------- */

bool dx_property_map_try_get_value(const dx_property_map_t* props,
	dxf_const_string_t key,
	OUT dxf_const_string_t* value) {
	dx_property_item_t item = { (dxf_string_t)key, NULL };
	bool found;
	size_t index;

	if (props == NULL || key == NULL || value == NULL)
		return dx_set_error_code(dx_ec_invalid_func_param);

	DX_ARRAY_BINARY_SEARCH(props->elements, 0, props->size, item, dx_property_item_comparator, found, index);
	if (!found) {
		return false;
	}

	*value = props->elements[index].value;
	return true;
}
