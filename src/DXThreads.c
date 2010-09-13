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
 *	Thread error codes
 */
/* -------------------------------------------------------------------------- */

static struct dx_error_code_descr_t g_thread_errors[] = {
    { dx_tec_not_enough_sys_resources, "Not enough system resources to perform requested operation" },
    { dx_tec_permission_denied, "Permission denied" },
    { dx_tec_invalid_res_operation, "Internal software error" },
    { dx_tec_invalid_resource_id, "Internal software error" },
    { dx_tec_deadlock_detected, "Internal software error" },
    { dx_tec_not_enough_memory, "Not enough memory to perform requested operation" },
    { dx_tec_resource_busy, "Internal software error" },
    { dx_tec_generic_error, "Unrecognized thread error" },
    
    { ERROR_CODE_FOOTER, ERROR_DESCR_FOOTER }
};

const struct dx_error_code_descr_t* thread_error_roster = g_thread_errors;

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary functions implementation
 */
/* -------------------------------------------------------------------------- */

#ifdef _WIN32

#include <Windows.h>

void dx_sleep (size_t milliseconds) {
    Sleep((DWORD)milliseconds);
}

#endif /* _WIN32 */

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
        dx_set_last_error(dx_sc_threads, dx_tec_not_enough_sys_resources); break;
    case EINVAL:
        dx_set_last_error(dx_sc_threads, dx_tec_invalid_resource_id); break;
    case EPERM:
        dx_set_last_error(dx_sc_threads, dx_tec_permission_denied); break;
    default:
        dx_set_last_error(dx_sc_threads, dx_tec_generic_error); break;
    case 0:
        return true;
    }
    
    return false;
}

/* -------------------------------------------------------------------------- */

bool dx_wait_for_thread (pthread_t thread_id, void **value_ptr) {
    int res = pthread_join(thread_id, value_ptr);
    
    switch (res) {
    case EINVAL:
        dx_set_last_error(dx_sc_threads, dx_tec_invalid_res_operation); break;
    case ESRCH:
        dx_set_last_error(dx_sc_threads, dx_tec_invalid_resource_id); break;
    case EDEADLK:
        dx_set_last_error(dx_sc_threads, dx_tec_deadlock_detected); break;
    default:
        dx_set_last_error(dx_sc_threads, dx_tec_generic_error); break;
    case 0:
        return true;
    }
    
    return false;
}

/* -------------------------------------------------------------------------- */

bool dx_thread_data_key_create (pthread_key_t* key, void (*destructor)(void*)) {
    int res = pthread_key_create(key, destructor);
    
    switch (res) {
    case EAGAIN:
        dx_set_last_error(dx_sc_threads, dx_tec_not_enough_sys_resources); break;
    case ENOMEM:
        dx_set_last_error(dx_sc_threads, dx_tec_not_enough_memory); break;
    default:
        dx_set_last_error(dx_sc_threads, dx_tec_generic_error); break;
    case 0:
        return true;    
    }
    
    return false;
}

/* -------------------------------------------------------------------------- */

bool dx_thread_data_key_destroy (pthread_key_t key) {
    int res = pthread_key_delete(key);

    switch (res) {
    case EINVAL:
        dx_set_last_error(dx_sc_threads, dx_tec_invalid_resource_id); break;
    default:
        dx_set_last_error(dx_sc_threads, dx_tec_generic_error); break;
    case 0:
        return true;
    }

    return false;
}

/* -------------------------------------------------------------------------- */

bool dx_set_thread_data (pthread_key_t key, const void* data) {
    int res = pthread_setspecific(key, data);

    switch (res) {
    case ENOMEM:
        dx_set_last_error(dx_sc_threads, dx_tec_not_enough_memory); break;
    case EINVAL:
        dx_set_last_error(dx_sc_threads, dx_tec_invalid_resource_id); break;
    default:
        dx_set_last_error(dx_sc_threads, dx_tec_generic_error); break;
    case 0:
        return true;
    }

    return false;
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
    
    dx_logging_verbose_info("Create mutex %#010x", mutex);

    switch (res) {
    case EAGAIN:
        dx_set_last_error(dx_sc_threads, dx_tec_not_enough_sys_resources); break;
    case ENOMEM:
        dx_set_last_error(dx_sc_threads, dx_tec_not_enough_memory); break;
    case EPERM:
        dx_set_last_error(dx_sc_threads, dx_tec_permission_denied); break;
    case EBUSY:
        dx_set_last_error(dx_sc_threads, dx_tec_resource_busy); break;
    case EINVAL:
        dx_set_last_error(dx_sc_threads, dx_tec_invalid_resource_id); break;
    default:
        dx_set_last_error(dx_sc_threads, dx_tec_generic_error); break;
    case 0:
        return true;
    }
    
    return false;
}

/* -------------------------------------------------------------------------- */

bool dx_mutex_destroy (pthread_mutex_t* mutex) {
    int res = pthread_mutex_destroy(mutex);
    
    dx_logging_verbose_info("Destroy mutex %#010x", mutex);

    switch (res) {
    case EBUSY:
        dx_set_last_error(dx_sc_threads, dx_tec_resource_busy); break;
    case EINVAL:
        dx_set_last_error(dx_sc_threads, dx_tec_invalid_resource_id); break;
    default:
        dx_set_last_error(dx_sc_threads, dx_tec_generic_error); break;
    case 0:
        return true;
    }
    
    return false;
}

/* -------------------------------------------------------------------------- */

bool dx_mutex_lock (pthread_mutex_t* mutex) {
    int res = pthread_mutex_lock(mutex);

    dx_logging_verbose_info("Lock mutex %#010x", mutex);

    switch (res) {
    case EINVAL:
        dx_set_last_error(dx_sc_threads, dx_tec_invalid_resource_id); break;
    case EAGAIN:
        dx_set_last_error(dx_sc_threads, dx_tec_invalid_res_operation); break;
    case EDEADLK:
        dx_set_last_error(dx_sc_threads, dx_tec_deadlock_detected); break;
    default:
        dx_set_last_error(dx_sc_threads, dx_tec_generic_error); break;
    case 0:
        return true;
    }
    
    return false;
}

/* -------------------------------------------------------------------------- */

bool dx_mutex_unlock (pthread_mutex_t* mutex) {

    int res = pthread_mutex_unlock(mutex);
    
    dx_logging_verbose_info("Unlock mutex %#010x", mutex);

    switch (res) {
    case EINVAL:
        dx_set_last_error(dx_sc_threads, dx_tec_invalid_resource_id); break;
    case EAGAIN:
    case EPERM:
        dx_set_last_error(dx_sc_threads, dx_tec_invalid_res_operation); break;
    default:
        dx_set_last_error(dx_sc_threads, dx_tec_generic_error); break;
    case 0:
        return true;
    }
    
    return false;
}

/* -------------------------------------------------------------------------- */
/*
 *	Implementation of wrappers without error handling mechanism
 */
/* -------------------------------------------------------------------------- */

bool dx_set_thread_data_no_ehm (pthread_key_t key, const void* data) {
    return (pthread_setspecific(key, data) == 0);
}