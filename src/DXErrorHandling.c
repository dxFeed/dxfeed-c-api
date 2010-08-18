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

/* -------------------------------------------------------------------------- */
/*
 *	Subsystem error code aggregation and stuff
 */
/* -------------------------------------------------------------------------- */

extern const struct dx_error_code_descr_t* memory_error_roster;
extern const struct dx_error_code_descr_t* socket_error_roster;
extern const struct dx_error_code_descr_t* thread_error_roster;
extern const struct dx_error_code_descr_t* network_error_roster;
extern const struct dx_error_code_descr_t* parser_error_roster;

struct dx_subsystem_descr_t {
    int subsystem_id;
    const struct dx_error_code_descr_t* error_roster;
};

const struct dx_subsystem_descr_t* dx_subsystems () {
    static bool subsystems_initialized = false;
    static struct dx_subsystem_descr_t s_subsystems[sc_subsystem_count + 1];
    
    if (!subsystems_initialized) {
        s_subsystems[0].subsystem_id = sc_memory;
        s_subsystems[0].error_roster = memory_error_roster;
        
        s_subsystems[1].subsystem_id = sc_sockets;
        s_subsystems[1].error_roster = socket_error_roster;
        
        s_subsystems[2].subsystem_id = sc_threads;
        s_subsystems[2].error_roster = thread_error_roster;
        
        s_subsystems[3].subsystem_id = sc_network;
        s_subsystems[3].error_roster = network_error_roster;
        
        s_subsystems[4].subsystem_id = sc_parser;
        s_subsystems[4].error_roster = parser_error_roster;

        s_subsystems[5].subsystem_id = sc_invalid_subsystem;
        s_subsystems[5].error_roster = NULL;
        
        subsystems_initialized = true;
    }
    
    return s_subsystems;
}

/* -------------------------------------------------------------------------- */

const char* dx_get_error_descr (int subsystem_id, int error_code, enum dx_error_function_result_t* res) {
    const struct dx_subsystem_descr_t* cur_subsys = dx_subsystems();

    for (; cur_subsys->subsystem_id != sc_invalid_subsystem && cur_subsys->error_roster != NULL; ++cur_subsys) {
        const struct dx_error_code_descr_t* cur_error_descr;
        
        if (cur_subsys->subsystem_id != subsystem_id) {
            continue;
        }

        for (cur_error_descr = cur_subsys->error_roster;
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

    *res = efr_invalid_subsystem_id;
    
    return NULL;
}

/* -------------------------------------------------------------------------- */

enum dx_error_function_result_t dx_check_error_data (int subsystem_id, int error_code) {
    enum dx_error_function_result_t res;
    
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

struct dx_last_error_data_t {
    int subsystem_id;
    int error_code;
};

static struct dx_last_error_data_t g_master_thread_last_error_data = { sc_invalid_subsystem, ERROR_CODE_FOOTER };

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

enum dx_error_function_result_t dx_set_last_error (int subsystem_id, int error_code) {
    struct dx_last_error_data_t* error_data = NULL;
    enum dx_error_function_result_t res;
    
    if (subsystem_id == sc_invalid_subsystem && error_code == ERROR_CODE_FOOTER) {
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
    
    return efr_success;
}

/* -------------------------------------------------------------------------- */

enum dx_error_function_result_t dx_get_last_error (int* subsystem_id, int* error_code, const char** error_descr) {
    struct dx_last_error_data_t* error_data = NULL;
    enum dx_error_function_result_t dummy;
    
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

    if (error_data->subsystem_id == sc_invalid_subsystem) {
        return efr_no_error_stored;
    }

    if (subsystem_id != NULL) {
        *subsystem_id = error_data->subsystem_id;
    }
    
    if (error_code != NULL) {
        *error_code = error_data->error_code;
    }
    
    if (error_descr != NULL) {
        *error_descr = dx_get_error_descr(*subsystem_id, *error_code, &dummy);
    }

    return efr_success;
}

/* -------------------------------------------------------------------------- */

bool dx_pop_last_error () {
    return (dx_set_last_error(sc_invalid_subsystem, ERROR_CODE_FOOTER) == efr_success);
}

/* -------------------------------------------------------------------------- */

bool dx_init_error_subsystem () {
    struct dx_last_error_data_t* error_data = NULL;
    
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
    
    error_data = dx_calloc_no_ehm(1, sizeof(struct dx_last_error_data_t));
    
    if (error_data == NULL) {
        return false;
    }
    
    error_data->subsystem_id = sc_invalid_subsystem;
    error_data->error_code = ERROR_CODE_FOOTER;
    
    if (!dx_set_thread_data_no_ehm(g_last_error_data_key, (const void*)error_data)) {
        dx_free(error_data);
        
        return false;
    }
    
    return true;
}