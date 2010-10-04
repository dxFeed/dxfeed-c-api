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
 *	Implementation of the error functions.
 */

#include "DXErrorHandling.h"
#include "DXErrorCodes.h"
#include "DXThreads.h"
#include "DXMemory.h"
#include "Logger.h"

/* -------------------------------------------------------------------------- */
/*
 *	Subsystem error code aggregation and stuff
 */
/* -------------------------------------------------------------------------- */

extern const dx_error_code_descr_t* memory_error_roster;
extern const dx_error_code_descr_t* socket_error_roster;
extern const dx_error_code_descr_t* thread_error_roster;
extern const dx_error_code_descr_t* network_error_roster;
extern const dx_error_code_descr_t* parser_error_roster;
extern const dx_error_code_descr_t* event_subscription_error_roster;
extern const dx_error_code_descr_t* logger_error_roster;

const dx_error_code_descr_t* dx_subsystem_errors (dx_subsystem_code_t subsystem_code) {
    static bool subsystems_initialized = false;
    static const dx_error_code_descr_t* s_subsystem_errors[dx_sc_subsystem_count];
    
    if (!subsystems_initialized) {
        s_subsystem_errors[dx_sc_memory] = memory_error_roster;
        s_subsystem_errors[dx_sc_sockets] = socket_error_roster;
        s_subsystem_errors[dx_sc_threads] = thread_error_roster;
        s_subsystem_errors[dx_sc_network] = network_error_roster;
        s_subsystem_errors[dx_sc_parser] = parser_error_roster;
        s_subsystem_errors[dx_sc_event_subscription] = event_subscription_error_roster;
        s_subsystem_errors[dx_sc_logger] = logger_error_roster;
        
        subsystems_initialized = true;
    }
    
    if (subsystem_code < dx_sc_begin || subsystem_code >= dx_sc_subsystem_count) {
        return NULL;
    }
    
    return s_subsystem_errors[subsystem_code];
}

/* -------------------------------------------------------------------------- */

dx_const_string_t dx_get_error_descr (dx_subsystem_code_t subsystem_id, int error_code, dx_error_function_result_t* res) {
    const dx_error_code_descr_t* subsystem_errors = dx_subsystem_errors(subsystem_id);
    const dx_error_code_descr_t* cur_error_descr = subsystem_errors;
    
    if (subsystem_errors == NULL) {
        *res = efr_invalid_subsystem_id;
        
        return NULL;
    }

    for (;
         cur_error_descr->error_code != ERROR_CODE_FOOTER && cur_error_descr->error_descr != ERROR_DESCR_FOOTER;
         ++cur_error_descr) {
        
        if (cur_error_descr->error_code != error_code) {
            continue;
        }

        *res = efr_success;
        
        return cur_error_descr->error_descr;
    }

    *res = efr_invalid_error_code;
    
    return NULL;
}

/* -------------------------------------------------------------------------- */

dx_error_function_result_t dx_check_error_data (dx_subsystem_code_t subsystem_id, int error_code) {
    dx_error_function_result_t res;
    
    dx_get_error_descr(subsystem_id, error_code, &res);
    
    return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	Thread-specific stuff
 */
/* -------------------------------------------------------------------------- */

static bool g_initialization_attempted = false;
static pthread_key_t g_last_error_data_key;
static pthread_t g_master_thread_id;

typedef struct {
    int subsystem_id;
    int error_code;
} dx_last_error_data_t;

static dx_last_error_data_t g_master_thread_last_error_data = { dx_sc_invalid_subsystem, ERROR_CODE_FOOTER };

/* -------------------------------------------------------------------------- */

bool dx_is_master_thread () {
    return dx_compare_threads(dx_get_thread_id(), g_master_thread_id);
}

/* -------------------------------------------------------------------------- */

void dx_key_data_destructor (void* data) {
    if (data != &g_master_thread_last_error_data) {
        dx_free(data);
    }
    
    if (dx_is_master_thread()) {
        dx_thread_data_key_destroy(g_last_error_data_key);

        g_initialization_attempted = false;
    }
}

/* -------------------------------------------------------------------------- */
/*
 *	Error manipulation functions implementation
 */
/* -------------------------------------------------------------------------- */

dx_error_function_result_t dx_set_last_error (int subsystem_id, int error_code) {
    dx_last_error_data_t* error_data = NULL;
    dx_error_function_result_t res;
    
    if (subsystem_id == dx_sc_invalid_subsystem && error_code == DX_INVALID_ERROR_CODE) {
        /* that's a special case used to prune the previously stored error */
        
        res = efr_success;
    } else {
        res = dx_check_error_data(subsystem_id, error_code);
    }
    
    if (res != efr_success) {
        return res;
    }
    
    if (!dx_init_error_subsystem()) {
        return efr_error_subsys_init_failure;
    }
    
    if (dx_is_master_thread()) {
        error_data = &g_master_thread_last_error_data;
    } else {
        error_data = dx_get_thread_data(g_last_error_data_key);

        if (error_data == NULL) {
            return efr_error_subsys_init_failure;
        }    
    }
    
    error_data->subsystem_id = subsystem_id;
    error_data->error_code = error_code;

    {
        dx_error_function_result_t dummy;
        dx_logging_error(dx_get_error_descr(subsystem_id, error_code, &dummy));
    }

    return efr_success;
}

/* -------------------------------------------------------------------------- */

dx_error_function_result_t dx_get_last_error (int* subsystem_id, int* error_code, dx_const_string_t* error_descr) {
    dx_last_error_data_t* error_data = NULL;
    dx_error_function_result_t dummy;
    
    if (!g_initialization_attempted && !dx_init_error_subsystem()) {
        return efr_error_subsys_init_failure;
    }
    
    if (dx_is_master_thread()) {
        error_data = &g_master_thread_last_error_data;
    } else {
        error_data = dx_get_thread_data(g_last_error_data_key);

        if (error_data == NULL) {
            return efr_error_subsys_init_failure;
        }
    }

    if (error_data->subsystem_id == dx_sc_invalid_subsystem &&
        error_data->error_code == DX_INVALID_ERROR_CODE) {
        
        return efr_no_error_stored;
    }

    if (subsystem_id != NULL) {
        *subsystem_id = error_data->subsystem_id;
    }
    
    if (error_code != NULL) {
        *error_code = error_data->error_code;
    }
    
    if (error_descr != NULL) {
        *error_descr = dx_get_error_descr(error_data->subsystem_id, error_data->error_code, &dummy);
    }

    return efr_success;
}

/* -------------------------------------------------------------------------- */

bool dx_pop_last_error () {
    return (dx_set_last_error(dx_sc_invalid_subsystem, DX_INVALID_ERROR_CODE) == efr_success);
}

/* -------------------------------------------------------------------------- */

bool dx_init_error_subsystem () {
    dx_last_error_data_t* error_data = NULL;
    
    if (!g_initialization_attempted) {
        g_initialization_attempted = true;
        g_master_thread_id = dx_get_thread_id();
        
        if (!dx_thread_data_key_create(&g_last_error_data_key, dx_key_data_destructor)) {
            return false;
        }
        
        if (!dx_set_thread_data(g_last_error_data_key, &g_master_thread_last_error_data)) {
            return false;
        }
        
        return true;
    } else if (dx_is_master_thread()) {
        return true;
    }
    
    /* only the secondary threads reach here */
    
    error_data = (dx_last_error_data_t*)dx_calloc_no_ehm(1, sizeof(dx_last_error_data_t));
    
    if (error_data == NULL) {
        return false;
    }
    
    error_data->subsystem_id = dx_sc_invalid_subsystem;
    error_data->error_code = DX_INVALID_ERROR_CODE;
    
    if (!dx_set_thread_data_no_ehm(g_last_error_data_key, (const void*)error_data)) {
        dx_free(error_data);
        
        return false;
    }
    
    return true;
}