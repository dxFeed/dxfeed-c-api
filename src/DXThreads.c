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

#include "DXThreads.h"
#include "DXErrorHandling.h"
#include "DXErrorCodes.h"
#include "Logger.h"

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary functions implementation
 */
/* -------------------------------------------------------------------------- */

#ifdef _WIN32

#include <Windows.h>

void dx_sleep (int milliseconds) {
    Sleep((DWORD)milliseconds);
}

#endif /* _WIN32 */

/* -------------------------------------------------------------------------- */

static pthread_t g_master_thread_id;

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

bool dx_thread_create (pthread_t* thread_id, const pthread_attr_t* attr,
                       void* (*start_routine)(void*), void *arg) {
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

bool dx_wait_for_thread (pthread_t thread_id, void **value_ptr) {
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

bool dx_close_thread_handle (pthread_t thread_id) {
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

bool dx_thread_data_key_create (pthread_key_t* key, void (*destructor)(void*)) {
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

bool dx_thread_data_key_destroy (pthread_key_t key) {
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

bool dx_set_thread_data (pthread_key_t key, const void* data) {
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

void* dx_get_thread_data (pthread_key_t key) {
    return pthread_getspecific(key);
}

/* -------------------------------------------------------------------------- */

pthread_t dx_get_thread_id () {
    return pthread_self();
}

/* -------------------------------------------------------------------------- */

bool dx_compare_threads (pthread_t t1, pthread_t t2) {
    return (pthread_equal(t1, t2) != 0);
}

/* -------------------------------------------------------------------------- */

bool dx_mutex_create (pthread_mutex_t* mutex) {
    int res = pthread_mutex_init(mutex, NULL);
    
  //  dx_logging_verbose_info(L"Create mutex %#010x", mutex);

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

bool dx_mutex_destroy (pthread_mutex_t* mutex) {
    int res = pthread_mutex_destroy(mutex);
    
   // dx_logging_verbose_info(L"Destroy mutex %#010x", mutex);

    switch (res) {
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

bool dx_mutex_lock (pthread_mutex_t* mutex) {
    int res = pthread_mutex_lock(mutex);

 //   dx_logging_verbose_info(L"Lock mutex %#010x", mutex);

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

bool dx_mutex_unlock (pthread_mutex_t* mutex) {
    int res = pthread_mutex_unlock(mutex);
    
  //  dx_logging_verbose_info(L"Unlock mutex %#010x", mutex);

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

bool dx_set_thread_data_no_ehm (pthread_key_t key, const void* data) {
    return (pthread_setspecific(key, data) == 0);
}