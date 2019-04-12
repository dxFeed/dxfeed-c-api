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

#if !defined(_WIN32) || defined(USE_PTHREADS)
#	ifdef _WIN32
#		include "pthreads/pthread.h"
#	else
#		include "pthread.h"
#	endif
#	ifndef USE_PTHREADS
#		define USE_PTHREADS
#	endif
typedef pthread_t dx_thread_t;
typedef pthread_key_t dx_key_t;
typedef struct {
	pthread_mutex_t mutex;
	pthread_mutexattr_t attr;
} dx_mutex_t;
typedef void* (*dx_start_routine_t)(void*);
#define DX_THREAD_RETVAL_NULL NULL
#else /* !defined(_WIN32) || defined(USE_PTHREADS) */
#	include <windows.h>
#	define USE_WIN32_THREADS
typedef HANDLE dx_thread_t;
typedef DWORD dx_key_t;
typedef LPCRITICAL_SECTION dx_mutex_t;
typedef void pthread_attr_t;
typedef unsigned (*dx_start_routine_t)(void*);
#define DX_THREAD_RETVAL_NULL 0
#endif /* !defined(_WIN32) || defined(USE_PTHREADS) */

#include "PrimitiveTypes.h"

/* -------------------------------------------------------------------------- */
/*
 *	Callbacks management
 */
/* -------------------------------------------------------------------------- */
void dx_register_thread_constructor(void (*constructor)(void*), void *arg);
void dx_register_thread_destructor(void (*destructor)(void*), void *arg);
void dx_register_process_destructor(void (*destructor)(void*), void *arg);

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary functions
 */
/* -------------------------------------------------------------------------- */

void dx_sleep (int milliseconds);
void dx_mark_thread_master (void);
bool dx_is_thread_master (void);

/* -------------------------------------------------------------------------- */
/*
 *  below are the wrappers for the used POSIX thread functions that incorporate
 *  the error handling.
 *  these wrappers are common for any implementation unless some implementation brings
 *  some special error codes, in that case the wrapper implementation must be
 *  customized as well.
 */
/* -------------------------------------------------------------------------- */

bool dx_thread_create (dx_thread_t* thread_id, const pthread_attr_t* attr,
					dx_start_routine_t start_routine, void *arg);
bool dx_wait_for_thread (dx_thread_t thread_id, void **value_ptr);
bool dx_close_thread_handle (dx_thread_t thread_id);
bool dx_thread_data_key_create (dx_key_t* key, void (*destructor)(void*));
bool dx_thread_data_key_destroy (dx_key_t key);
bool dx_set_thread_data (dx_key_t key, const void* data);
void* dx_get_thread_data (dx_key_t key);
dx_thread_t dx_get_thread_id ();
bool dx_compare_threads (dx_thread_t t1, dx_thread_t t2);
bool dx_mutex_create (dx_mutex_t* mutex);
bool dx_mutex_destroy (dx_mutex_t* mutex);
bool dx_mutex_lock (const dx_mutex_t* mutex);
bool dx_mutex_unlock (const dx_mutex_t* mutex);

/* -------------------------------------------------------------------------- */
/*
 *	The thread function wrappers that do not invoke internal error
 *  handling mechanism.
 *  Such functions are used when the internal error handling mechanism
 *  cannot be trusted, e.g. within the error subsystem initialization routines.
 */
/* -------------------------------------------------------------------------- */

bool dx_set_thread_data_no_ehm (dx_key_t key, const void* data);

#endif /* THREADS_H_INCLUDED */