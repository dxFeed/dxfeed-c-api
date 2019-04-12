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
#include <process.h>
#endif

#include <errno.h>

#include "DXThreads.h"
#include "DXErrorHandling.h"
#include "DXErrorCodes.h"
#include "DXMemory.h"
#include "Logger.h"

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary functions implementation
 */
/* -------------------------------------------------------------------------- */

#ifdef _WIN32

#include <windows.h>

void dx_sleep (int milliseconds) {
	Sleep((DWORD)milliseconds);
}

#else

#include <time.h>

void dx_sleep (int milliseconds) {
	struct timespec ts;
	ts.tv_sec  = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&ts, NULL);
}

#endif /* _WIN32 */


/* -------------------------------------------------------------------------- */

static dx_thread_t g_master_thread_id;

void dx_mark_thread_master (void) {
	g_master_thread_id = dx_get_thread_id();
}

/* -------------------------------------------------------------------------- */

bool dx_is_thread_master (void) {
	return dx_compare_threads(dx_get_thread_id(), g_master_thread_id);
}

/* -------------------------------------------------------------------------- */
/*
 *	Wrapper implementation
 */
/* -------------------------------------------------------------------------- */

#ifdef USE_PTHREADS
bool dx_thread_create (dx_thread_t* thread_id, const pthread_attr_t* attr,
					dx_start_routine_t start_routine, void *arg) {
	int res = pthread_create(thread_id, attr, start_routine, arg);

	switch (res) {
	case EAGAIN:
		return dx_set_error_code(dx_tec_not_enough_sys_resources);
	case EINVAL:
		return dx_set_error_code(dx_tec_invalid_resource_id);
	case EPERM:
		return dx_set_error_code(dx_tec_permission_denied);
	default:
		return dx_set_error_code(dx_tec_generic_error);
	case 0:
		return true;
	}
}

/* -------------------------------------------------------------------------- */

bool dx_wait_for_thread (dx_thread_t thread_id, void **value_ptr) {
	int res = pthread_join(thread_id, value_ptr);

	switch (res) {
	case EINVAL:
		return dx_set_error_code(dx_tec_invalid_res_operation);
	case ESRCH:
		return dx_set_error_code(dx_tec_invalid_resource_id);
	case EDEADLK:
		return dx_set_error_code(dx_tec_deadlock_detected);
	default:
		return dx_set_error_code(dx_tec_generic_error);
	case 0:
		return true;
	}
}

/* -------------------------------------------------------------------------- */

bool dx_close_thread_handle (dx_thread_t thread_id) {
	int res = pthread_detach(thread_id);

	switch (res) {
	case EINVAL:
		return dx_set_error_code(dx_tec_invalid_res_operation);
	default:
		return dx_set_error_code(dx_tec_generic_error);
	case ESRCH:
		/* for some reason this return value is given for a valid thread handle */

		/* return dx_set_error_code(dx_tec_invalid_resource_id); */
	case 0:
		return true;
	}
}

/* -------------------------------------------------------------------------- */

bool dx_thread_data_key_create (dx_key_t* key, void (*destructor)(void*)) {
	int res = pthread_key_create(key, destructor);

	switch (res) {
	case EAGAIN:
		return dx_set_error_code(dx_tec_not_enough_sys_resources);
	case ENOMEM:
		return dx_set_error_code(dx_tec_not_enough_memory);
	default:
		return dx_set_error_code(dx_tec_generic_error);
	case 0:
		return true;
	}
}

/* -------------------------------------------------------------------------- */

bool dx_thread_data_key_destroy (dx_key_t key) {
	int res = pthread_key_delete(key);

	switch (res) {
	case EINVAL:
		return dx_set_error_code(dx_tec_invalid_resource_id);
	default:
		return dx_set_error_code(dx_tec_generic_error);
	case 0:
		return true;
	}
}

/* -------------------------------------------------------------------------- */

bool dx_set_thread_data (dx_key_t key, const void* data) {
	int res = pthread_setspecific(key, data);

	switch (res) {
	case ENOMEM:
		return dx_set_error_code(dx_tec_not_enough_memory);
	case EINVAL:
		return dx_set_error_code(dx_tec_invalid_resource_id);
	default:
		return dx_set_error_code(dx_tec_generic_error);
	case 0:
		return true;
	}
}

/* -------------------------------------------------------------------------- */

void* dx_get_thread_data (dx_key_t key) {
	return pthread_getspecific(key);
}

/* -------------------------------------------------------------------------- */

dx_thread_t dx_get_thread_id () {
	return pthread_self();
}

/* -------------------------------------------------------------------------- */

bool dx_compare_threads (dx_thread_t t1, dx_thread_t t2) {
	return (pthread_equal(t1, t2) != 0);
}

/* -------------------------------------------------------------------------- */

bool dx_mutex_create (dx_mutex_t* mutex) {
	int res = pthread_mutexattr_init(&mutex->attr);

	switch (res) {
	case ENOMEM:
		return dx_set_error_code(dx_tec_not_enough_memory);
	case EINVAL:
		return dx_set_error_code(dx_tec_invalid_resource_id);
	default: break;
	}

	res = pthread_mutexattr_settype(&mutex->attr, PTHREAD_MUTEX_RECURSIVE);

	if (res == EINVAL) {
		return dx_set_error_code(dx_tec_invalid_resource_id);
	}

	res = pthread_mutex_init(&mutex->mutex, &mutex->attr);

	switch (res) {
	case EAGAIN:
		return dx_set_error_code(dx_tec_not_enough_sys_resources);
	case ENOMEM:
		return dx_set_error_code(dx_tec_not_enough_memory);
	case EPERM:
		return dx_set_error_code(dx_tec_permission_denied);
	case EBUSY:
		return dx_set_error_code(dx_tec_resource_busy);
	case EINVAL:
		return dx_set_error_code(dx_tec_invalid_resource_id);
	default:
		return dx_set_error_code(dx_tec_generic_error);
	case 0:
		return true;
	}
}

/* -------------------------------------------------------------------------- */

bool dx_mutex_destroy (dx_mutex_t* mutex) {
	int res = pthread_mutex_destroy(&mutex->mutex);

	switch (res) {
	case EBUSY:
		return dx_set_error_code(dx_tec_resource_busy);
	case EINVAL:
		return dx_set_error_code(dx_tec_invalid_resource_id);
	case 0:
		break;
	default:
		return dx_set_error_code(dx_tec_generic_error);
	}

	res = pthread_mutexattr_destroy(&mutex->attr);

	switch (res) {
	case ENOMEM:
		return dx_set_error_code(dx_tec_not_enough_memory);
	case EINVAL:
		return dx_set_error_code(dx_tec_invalid_resource_id);
	default:
		return true;
	}
}

/* -------------------------------------------------------------------------- */

bool dx_mutex_lock (const dx_mutex_t* mutex) {
	int res = pthread_mutex_lock(&mutex->mutex);
	switch (res) {
	case EINVAL:
		return dx_set_error_code(dx_tec_invalid_resource_id);
	case EAGAIN:
		return dx_set_error_code(dx_tec_invalid_res_operation);
	case EDEADLK:
		return dx_set_error_code(dx_tec_deadlock_detected);
	default:
		return dx_set_error_code(dx_tec_generic_error);
	case 0:
		return true;
	}
}

/* -------------------------------------------------------------------------- */

bool dx_mutex_unlock (const dx_mutex_t* mutex) {
	int res = pthread_mutex_unlock(&mutex->mutex);
	switch (res) {
	case EINVAL:
		return dx_set_error_code(dx_tec_invalid_resource_id);
	case EAGAIN:
	case EPERM:
		return dx_set_error_code(dx_tec_invalid_res_operation);
	default:
		return dx_set_error_code(dx_tec_generic_error);
	case 0:
		return true;
	}
}

/* -------------------------------------------------------------------------- */
/*
 *	Implementation of wrappers without error handling mechanism
 */
/* -------------------------------------------------------------------------- */

bool dx_set_thread_data_no_ehm (dx_key_t key, const void* data) {
	return (pthread_setspecific(key, data) == 0);
}

#elif defined (USE_WIN32_THREADS)

/* Wrapper for thread start function */
typedef struct dx_thread_wrapper_args_tag {
	dx_start_routine_t start_routine;
	void *args;
} dx_thread_wrapper_args_t;

static unsigned __stdcall dx_thread_start_routine(void *a) {
	dx_thread_wrapper_args_t *args = (dx_thread_wrapper_args_t*)a;
	unsigned rv;
	rv = args->start_routine(args->args);
	dx_free(args);
	return rv;
}

/* Way to process destructors for TLS */
typedef struct dx_data_key_destructor_tag {
	dx_key_t key;
	void (*destructor)(void*);
} dx_data_key_destructor_t;

typedef struct dx_destructors_queue_tag {
	dx_mutex_t mutex;
	int size;
	int count;
	dx_data_key_destructor_t *destructors;
} dx_destructors_queue_t;

dx_destructors_queue_t g_key_destructors;

static void dx_call_keys_destructor(void *arg) {
	int i;
	if (g_key_destructors.count == 0)
		return;
	if (!dx_mutex_lock(&g_key_destructors.mutex))
		return;

	for (i = 0; i < g_key_destructors.count; i++) {
		void *data = dx_get_thread_data(g_key_destructors.destructors[i].key);
		g_key_destructors.destructors[i].destructor(data);
	}

	dx_mutex_unlock(&g_key_destructors.mutex);
}

static void dx_add_key_destructor(dx_key_t key, void (*destructor)(void*)) {
	if (g_key_destructors.size == 0)
		return;
	if (!dx_mutex_lock(&g_key_destructors.mutex))
		return;
	if (g_key_destructors.size == g_key_destructors.count) {
		dx_data_key_destructor_t *ds;
		g_key_destructors.size *= 2;
		ds = dx_calloc(g_key_destructors.size, sizeof(*g_key_destructors.destructors));
		dx_memcpy(ds, g_key_destructors.destructors, g_key_destructors.count * sizeof(*g_key_destructors.destructors));
		dx_free(g_key_destructors.destructors);
		g_key_destructors.destructors = ds;
	}
	g_key_destructors.destructors[g_key_destructors.count].key = key;
	g_key_destructors.destructors[g_key_destructors.count].destructor = destructor;
	g_key_destructors.count++;
	dx_mutex_unlock(&g_key_destructors.mutex);
}

static void dx_remove_key_destructor(dx_key_t key) {
	int i;
	if (g_key_destructors.count == 0)
		return;
	if (!dx_mutex_lock(&g_key_destructors.mutex))
		return;
	for (i = 0; i < g_key_destructors.count; i++) {
		if (g_key_destructors.destructors[i].key == key) {
			dx_memmove(&g_key_destructors.destructors[i], &g_key_destructors.destructors[i + 1], sizeof(*g_key_destructors.destructors) * (g_key_destructors.count - i - 1));
			g_key_destructors.count--;
			break;
		}
	}
	dx_mutex_unlock(&g_key_destructors.mutex);
}

static void dx_deinit_threads(void *arg) {
	dx_free(g_key_destructors.destructors);
	dx_mutex_destroy(&g_key_destructors.mutex);
	dx_memset(&g_key_destructors, 0, sizeof(g_key_destructors));
}

void dx_init_threads() {
	dx_mutex_create(&g_key_destructors.mutex);
	g_key_destructors.size = 16;
	g_key_destructors.count = 0;
	g_key_destructors.destructors = dx_calloc(g_key_destructors.size, sizeof(*g_key_destructors.destructors));

	dx_register_process_destructor(&dx_deinit_threads, NULL);
	dx_register_thread_destructor(&dx_call_keys_destructor, NULL);
}

/* Public API */

bool dx_thread_create (dx_thread_t* thread_id, const pthread_attr_t* attr,
					dx_start_routine_t start_routine, void *arg) {
	dx_thread_wrapper_args_t *wargs = dx_calloc(1, sizeof(*wargs));

	wargs->start_routine = start_routine;
	wargs->args = arg;
	*thread_id = (dx_thread_t)_beginthreadex(NULL, 0, &dx_thread_start_routine, wargs, 0, NULL);

	if (*thread_id != INVALID_HANDLE_VALUE) {
		return true;
	}
	switch (errno) {
	case EAGAIN:
		return dx_set_error_code(dx_tec_not_enough_sys_resources);
	case EINVAL:
		return dx_set_error_code(dx_tec_invalid_resource_id);
	case EPERM:
		return dx_set_error_code(dx_tec_permission_denied);
	default:
		return dx_set_error_code(dx_tec_generic_error);
	}
}

/* -------------------------------------------------------------------------- */

bool dx_wait_for_thread (dx_thread_t thread_id, void **value_ptr) {
	int res;
	res = WaitForSingleObject(thread_id, INFINITE);

	switch (res) {
	case WAIT_FAILED:
		return dx_set_error_code(dx_tec_invalid_res_operation);
	case WAIT_ABANDONED:
		return dx_set_error_code(dx_tec_invalid_resource_id);
	case WAIT_TIMEOUT:
		return dx_set_error_code(dx_tec_deadlock_detected);
	default:
		return dx_set_error_code(dx_tec_generic_error);
	case WAIT_OBJECT_0:
		if (value_ptr == NULL)
			return true;
		return GetExitCodeThread(thread_id, (LPDWORD)value_ptr);
	}
}

/* -------------------------------------------------------------------------- */

bool dx_close_thread_handle (dx_thread_t thread_id) {
	if (CloseHandle(thread_id)) {
		return true;
	}
	return dx_set_error_code(dx_tec_generic_error);
}

/* -------------------------------------------------------------------------- */

bool dx_thread_data_key_create (dx_key_t* key, void (*destructor)(void*)) {
	*key = TlsAlloc();
	if (*key !=  TLS_OUT_OF_INDEXES) {
		if (destructor != NULL)
			dx_add_key_destructor(*key, destructor);
		return true;
	}
	return dx_set_error_code(dx_tec_not_enough_memory);
}

/* -------------------------------------------------------------------------- */

bool dx_thread_data_key_destroy (dx_key_t key) {
	dx_remove_key_destructor(key);
	TlsFree(key);
	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_set_thread_data (dx_key_t key, const void* data) {
	TlsSetValue(key, (void*)data);
	return true;
}

/* -------------------------------------------------------------------------- */

void* dx_get_thread_data (dx_key_t key) {
	return TlsGetValue(key);
}

/* -------------------------------------------------------------------------- */

dx_thread_t dx_get_thread_id () {
	return GetCurrentThread();
}

/* -------------------------------------------------------------------------- */

bool dx_compare_threads (dx_thread_t t1, dx_thread_t t2) {
	return t1 == t2;
}

/* -------------------------------------------------------------------------- */

bool dx_mutex_create (dx_mutex_t* mutex) {
	*mutex = dx_calloc(1, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(*mutex);
	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_mutex_destroy (dx_mutex_t* mutex) {
	DeleteCriticalSection(*mutex);
	free(*mutex);
	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_mutex_lock (const dx_mutex_t* mutex) {
	EnterCriticalSection(*mutex);
	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_mutex_unlock (const dx_mutex_t* mutex) {
	LeaveCriticalSection(*mutex);
	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Implementation of wrappers without error handling mechanism
 */
/* -------------------------------------------------------------------------- */

bool dx_set_thread_data_no_ehm (dx_key_t key, const void* data) {
	return dx_set_thread_data(key, data);
}

#else
#	error "Please, select threads implementation"
#endif
