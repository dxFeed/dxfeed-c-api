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
#include "StringArray.h"

bool dx_string_array_add(dx_string_array_t* string_array, dxf_const_string_t str) {
    bool failed = false;

    if (string_array == NULL) {
        return dx_set_error_code(dx_ec_invalid_func_param_internal);
    }

    DX_ARRAY_INSERT(*string_array, dxf_const_string_t, str, string_array->size, dx_capacity_manager_halfer, failed);

    return !failed;
}

void dx_string_array_free(dx_string_array_t* string_array) {
    int i = 0;

    if (string_array == NULL) {
        return;
    }

    for (; i < string_array->size; ++i) {
        dx_free((void*)string_array->elements[i]);
    }

    if (string_array->elements != NULL) {
        dx_free((void*)string_array->elements);
    }

    string_array->elements = NULL;
    string_array->size = 0;
    string_array->capacity = 0;
}