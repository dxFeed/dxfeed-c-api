// AdvMemory.h
// Contains wrappers for the common C memory functions, to encapsulate
// the error handling

#ifndef ADV_MEMORY_H_INCLUDED
#define ADV_MEMORY_H_INCLUDED

#include <malloc.h>
#include "ErrorReport.h"
#include "ParserCommon.h"

/* -------------------------------------------------------------------------- */
/*
 *	Memory function wrappers
 */
/* -------------------------------------------------------------------------- */

void* dx_error_processor (void* src) {
    if (src == NULL) {
        dx_set_last_error(SS_DataSerializer, DS_OutOfMemory);
    }
    
    return src;
}

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

#endif // ADV_MEMORY_H_INCLUDED