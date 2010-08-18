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
 *	Implementation of the memory functions
 */

#include "DXMemory.h"
#include "DXErrorHandling.h"
#include "DXErrorCodes.h"

#include <malloc.h>

/* -------------------------------------------------------------------------- */
/*
 *	Error related stuff
 */
/* -------------------------------------------------------------------------- */

static struct dx_error_code_descr_t g_memory_errors[] = {
    { dx_mec_insufficient_memory, "Insufficient memory available" },
    
    { ERROR_CODE_FOOTER, ERROR_DESCR_FOOTER }
};

const struct dx_error_code_descr_t* memory_error_roster = g_memory_errors;

/* -------------------------------------------------------------------------- */

void* dx_error_processor (void* src) {
    if (src == NULL) {
        dx_set_last_error(dx_sc_memory, dx_mec_insufficient_memory);
    }

    return src;
}

/* -------------------------------------------------------------------------- */
/*
 *	Memory function wrappers implementation
 */
/* -------------------------------------------------------------------------- */

void* dx_malloc (size_t size) {
    return dx_error_processor(malloc(size));
}

/* -------------------------------------------------------------------------- */

void* dx_calloc (size_t num, size_t size) {
    return dx_error_processor(calloc(num, size));
}

/* -------------------------------------------------------------------------- */

void dx_free (void* buf) {
    free(buf);
}

/* -------------------------------------------------------------------------- */
/*
 *	Implementation of wrappers with no error handling mechanism enabled
 */
/* -------------------------------------------------------------------------- */

void* dx_calloc_no_ehm (size_t num, size_t size) {
    return calloc(num, size);
}
