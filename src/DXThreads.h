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
 *	Aggregates the thread API to provide a platform-independent abstraction.
 *  The underlying thread API is based on POSIX thread API (pthreads).
 */

#ifndef DX_THREADS_H_INCLUDED
#define DX_THREADS_H_INCLUDED

#ifdef _WIN32
    #include "../thirdparty/pthreads/win32/include/pthread.h"
#endif /* _WIN32 */

#include "PrimitiveTypes.h"

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary functions
 */
/* -------------------------------------------------------------------------- */

void dx_sleep (size_t milliseconds);

/* -------------------------------------------------------------------------- */
/*
 *  below are the wrappers for the used POSIX thread functions that incorporate
 *  the error handling.
 *  these wrappers are common for any implementation unless some implementation brings
 *  some special error codes, in that case the wrapper implementation must be
 *  customized as well.
 */
/* -------------------------------------------------------------------------- */

bool dx_thread_create (pthread_t* thread_id, const pthread_attr_t* attr,
                       void* (*start_routine)(void*), void *arg);
bool dx_wait_for_thread (pthread_t thread_id, void **value_ptr);
bool dx_thread_data_key_create (pthread_key_t* key, void (*destructor)(void*));
bool dx_thread_data_key_destroy (pthread_key_t key);
bool dx_set_thread_data (pthread_key_t key, const void* data);
void* dx_get_thread_data (pthread_key_t key);
pthread_t dx_get_thread_id ();
bool dx_compare_threads (pthread_t t1, pthread_t t2);
bool dx_mutex_create (pthread_mutex_t* mutex);
bool dx_mutex_destroy (pthread_mutex_t* mutex);
bool dx_mutex_lock (pthread_mutex_t* mutex);
bool dx_mutex_unlock (pthread_mutex_t* mutex);

/* -------------------------------------------------------------------------- */
/*
 *	The thread function wrappers that do not invoke internal error
 *  handling mechanism.
 *  Such functions are used when the internal error handling mechanism
 *  cannot be trusted, e.g. within the error subsystem initialization routines.
 */
/* -------------------------------------------------------------------------- */

bool dx_set_thread_data_no_ehm (pthread_key_t key, const void* data);

#endif /* THREADS_H_INCLUDED */