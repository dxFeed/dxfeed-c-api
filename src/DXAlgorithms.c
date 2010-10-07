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
#include "DXMemory.h"

#include <Windows.h>

/* -------------------------------------------------------------------------- */
/*
 *	Array functions implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_capacity_manager_halfer (int new_size, int* capacity) {
    if (new_size > *capacity) {
        *capacity = (int)((double)*capacity * 1.5) + 1;

        return true;
    }

    if (new_size < *capacity && ((double)new_size / *capacity) < 1.5) {
        *capacity = new_size;

        return true;
    }

    return false;
}

/* -------------------------------------------------------------------------- */
/*
 *	String functions implementation
 */
/* -------------------------------------------------------------------------- */

dx_string_t dx_create_string (int size) {
    return (dx_string_t)dx_calloc(size + 1, sizeof(dx_char_t));
}

/* -------------------------------------------------------------------------- */

dx_string_t dx_create_string_src (dx_const_string_t src) {
    return dx_create_string_src_len(src, dx_string_length(src));
}

/* -------------------------------------------------------------------------- */

char* dx_ansi_create_string_src (const char* src) {
    char* res = (char*)dx_calloc((int)strlen(src) + 1, sizeof(char));
    
    if (res == NULL) {
        return NULL;
    }
    
    return strcpy(res, src);
}

/* -------------------------------------------------------------------------- */

dx_string_t dx_create_string_src_len (dx_const_string_t src, int len) {
    dx_string_t res = NULL;

    if (len == 0) {
        return res;
    }

    res = dx_create_string(len);

    if (res == NULL) {
        return res;
    }

    return dx_copy_string_len(res, src, len);
}

/* -------------------------------------------------------------------------- */

dx_string_t dx_copy_string (dx_string_t dest, dx_const_string_t src) {
    return wcscpy(dest, src);
}

/* -------------------------------------------------------------------------- */

dx_string_t dx_copy_string_len (dx_string_t dest, dx_const_string_t src, int len) {
    return wcsncpy(dest, src, len);
}

/* -------------------------------------------------------------------------- */

int dx_string_length (dx_const_string_t str) {
    return (int)wcslen(str);
}

/* -------------------------------------------------------------------------- */

int dx_compare_strings (dx_const_string_t s1, dx_const_string_t s2) {
    return wcscmp(s1, s2);
}

/* -------------------------------------------------------------------------- */

dx_char_t dx_toupper (dx_char_t c) {
    return towupper(c);
}

/* -------------------------------------------------------------------------- */

dx_string_t dx_ansi_to_unicode (const char* ansi_str) {
#ifdef _WIN32
    size_t len = strlen(ansi_str);
    dx_string_t wide_str = NULL;

    // get required size
    int wide_size = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, ansi_str, (int)len, wide_str, 0);
    
    if (wide_size > 0) {
        wide_str = dx_create_string(wide_size);
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, ansi_str, (int)len, wide_str, wide_size);
    }

    return wide_str;
#else /* _WIN32 */
    return NULL; /* todo */
#endif /* _WIN32 */
}

/* -------------------------------------------------------------------------- */

dx_string_t dx_decode_from_integer (dx_long_t code) {
    dx_char_t decoded[8] = { 0 };
    int offset = 0;
    
    while (code != 0) {
        dx_char_t c = (dx_char_t)(code >> 56);
        
        if (c != 0) {
            decoded[offset++] = c;            
        }
        
        code <<= 8;
    }
    
    return dx_create_string_src_len(decoded, offset);
}

/* -------------------------------------------------------------------------- */
/*
 *	Bit operations implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_is_only_single_bit_set (int value) {
    return ((value & (value - 1)) == 0);
}