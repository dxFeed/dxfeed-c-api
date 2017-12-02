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
 *	Wrappers for the common C memory functions, to encapsulate the error handling
 */

#ifndef DX_MEMORY_H_INCLUDED
#define DX_MEMORY_H_INCLUDED

#include "DXTypes.h"

/* -------------------------------------------------------------------------- */
/*
 *	Memory function wrappers
 */
/* -------------------------------------------------------------------------- */

void* dx_malloc (size_t size);
void* dx_calloc (size_t num, size_t size);
void  dx_free (void* buf);
void* dx_memcpy (void* destination, const void* source, size_t size);
void* dx_memmove (void* destination, const void* source, size_t size);
void* dx_memset (void* destination, int c, size_t size);

/* -------------------------------------------------------------------------- */
/*
 *	Memory function wrappers without error handling mechanism

 *  May be useful when the internal error handling mechanism cannot be relied
 *  upon, e.g. within its initialization.
 */
/* -------------------------------------------------------------------------- */

void* dx_calloc_no_ehm (size_t num, size_t size);
void dx_free_no_ehm (void* buf);

#endif /* DX_MEMORY_H_INCLUDED */