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

#ifdef _WIN32
#include <Windows.h>
#else
#include <time.h>
#include <stdlib.h>
#include <wctype.h>
#endif /* _WIN32 */
 
#include "DXAlgorithms.h"
#include "DXMemory.h"

#include <time.h>
#include <string.h>
#include <limits.h>

/* -------------------------------------------------------------------------- */
/*
 *	Bit operations implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_is_only_single_bit_set (int value) {
    return (value != 0 && (value & (value - 1)) == 0);
}

/* -------------------------------------------------------------------------- */
/*
 *	Random number generation functions implementation
 */
/* -------------------------------------------------------------------------- */

static void dx_init_randomizer (void) {
    static bool is_randomizer_initialized = false;

    if (!is_randomizer_initialized) {
        is_randomizer_initialized = true;

        srand((unsigned int)time(NULL));
    }
}

/* -------------------------------------------------------------------------- */

int dx_random_integer (int max_value) {
    dx_init_randomizer();

    return (int)((long long)max_value * (unsigned int)rand() / RAND_MAX);
}

/* -------------------------------------------------------------------------- */

double dx_random_double (double max_value) {
    dx_init_randomizer();

    return max_value / RAND_MAX * rand();
}

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

    if (new_size < *capacity && ((double)new_size / *capacity) < 0.5) {
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

dxf_string_t dx_create_string (int size) {
    return (dxf_string_t)dx_calloc(size + 1, sizeof(dxf_char_t));
}

/* -------------------------------------------------------------------------- */

dxf_string_t dx_create_string_src (dxf_const_string_t src) {
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

dxf_string_t dx_create_string_src_len (dxf_const_string_t src, int len) {
    dxf_string_t res = NULL;

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

dxf_string_t dx_copy_string (dxf_string_t dest, dxf_const_string_t src) {
    return wcscpy(dest, src);
}

/* -------------------------------------------------------------------------- */

dxf_string_t dx_copy_string_len (dxf_string_t dest, dxf_const_string_t src, int len) {
    return wcsncpy(dest, src, len);
}

/* -------------------------------------------------------------------------- */

int dx_string_length (dxf_const_string_t str) {
    return (int)wcslen(str);
}

/* -------------------------------------------------------------------------- */

int dx_compare_strings (dxf_const_string_t s1, dxf_const_string_t s2) {
    return wcscmp(s1, s2);
}

/* -------------------------------------------------------------------------- */

dxf_char_t dx_toupper (dxf_char_t c) {
    return towupper(c);
}

/* -------------------------------------------------------------------------- */

dxf_string_t dx_ansi_to_unicode (const char* ansi_str) {
#ifdef _WIN32
    size_t len = strlen(ansi_str);
    dxf_string_t wide_str = NULL;

    // get required size
    int wide_size = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, ansi_str, (int)len, wide_str, 0);
    
    if (wide_size > 0) {
        wide_str = dx_create_string(wide_size);
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, ansi_str, (int)len, wide_str, wide_size);
    }

    return wide_str;
#else /* _WIN32 */
    mbstate_t state;
    dxf_string_t wide_str = NULL;
    // We trust it
    size_t len = strlen(ansi_str);

    mbrlen(NULL, 0, &state);
    
    const char *p = ansi_str;
    size_t wide_size = mbsrtowcs(NULL, &p, 0, &state);

    if (wide_size > 0) {
        wide_str = dx_create_string(wide_size);
        p = ansi_str;
        mbrlen(NULL, 0, &state);
        mbsrtowcs(wide_str, &p, wide_size, &state);
    }

    return wide_str;
#endif /* _WIN32 */
}

/* -------------------------------------------------------------------------- */

dxf_string_t dx_decode_from_integer (dxf_long_t code) {
    dxf_char_t decoded[8] = { 0 };
    int offset = 0;
    
    while (code != 0) {
        dxf_char_t c = (dxf_char_t)(code >> 56);
        
        if (c != 0) {
            decoded[offset++] = c;            
        }
        
        code <<= 8;
    }
    
    return dx_create_string_src_len(decoded, offset);
}

/* -------------------------------------------------------------------------- */
/*
 *	Time functions implementation
 */
/* -------------------------------------------------------------------------- */

int dx_millisecond_timestamp (void) {

#ifdef _WIN32
    return (int)GetTickCount();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int)(ts.tv_sec * 1000 + ts.tv_sec / 1000000);
#endif
}

/* -------------------------------------------------------------------------- */

int dx_millisecond_timestamp_diff (int newer, int older) {
    long long res = 0;
    
    if ((unsigned)older > (unsigned)newer) {
        res += UINT_MAX;
    }
    
    return (int)(res + (unsigned)newer - (unsigned)older);
}