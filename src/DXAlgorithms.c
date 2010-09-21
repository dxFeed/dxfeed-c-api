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

#include <Windows.h>

/* -------------------------------------------------------------------------- */
/*
 *	Array functions implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_capacity_manager_halfer (size_t new_size, size_t* capacity) {
    if (new_size > *capacity) {
        *capacity = (size_t)((double)*capacity * 1.5) + 1;

        return true;
    }

    if (new_size < *capacity && ((double)new_size / *capacity) < 1.5) {
        *capacity = new_size;

        return true;
    }

    return false;
}

/* -------------------------------------------------------------------------- */

dx_string_t dx_create_string (size_t size) {
    return (dx_string_t)dx_calloc(size + 1, sizeof(dx_char_t));
}

/* -------------------------------------------------------------------------- */

dx_string_t dx_ansi_to_unicode( const char* ansi_str ) {
#ifdef _WIN32
    size_t len = strlen(ansi_str);
    dx_string_t wide_str = NULL;

    // get required size
    int wide_size = MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, ansi_str, (int)len, wide_str, 0);
    if (wide_size > 0) {
        wide_str = dx_create_string(wide_size);
        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, ansi_str, (int)len, wide_str, wide_size);
    }

    return wide_str;
#else // _WIN32
    return NULL; // todo
#endif // _WIN32
}

/* -------------------------------------------------------------------------- */

bool dx_is_one_bit_sets( dx_int_t val ) {
    return (val & (val - 1)) == 0;
}