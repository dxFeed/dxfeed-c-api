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
 *	Functions, structures, types etc to operate the error information.
 */

#ifndef DX_ERROR_HANDLING_H_INCLUDED
#define DX_ERROR_HANDLING_H_INCLUDED

#include "PrimitiveTypes.h"
#include "DXErrorCodes.h"
#include "DXTypes.h"

/* -------------------------------------------------------------------------- */
/*
 *	Error code structures and values
 */
/* -------------------------------------------------------------------------- */

typedef struct {
    int error_code; /* a numeric error code; must be uniqie across a subsystem */
    dx_const_string_t error_descr; /* a text representation of an error;
                                      is optional, may be null */
} dx_error_code_descr_t;

#define ERROR_CODE_FOOTER (-1)
#define ERROR_DESCR_FOOTER ((dx_const_string_t)-1)

/* -------------------------------------------------------------------------- */
/*
 *	Error subsystem function return value roster
 */
/* -------------------------------------------------------------------------- */

typedef enum {
    efr_success,
    efr_invalid_subsystem_id,
    efr_invalid_error_code,
    efr_no_error_stored,
    efr_error_subsys_init_failure    
} dx_error_function_result_t;

/* -------------------------------------------------------------------------- */
/*
 *	The error manipulation functions
 */
/* -------------------------------------------------------------------------- */
/*
 *  Reports an error.

    Input:
        subsystem_id - an ID of the subsystem where the error occurred.
        error_code - an error code; must belong to the subsystems error roster.
        
    Return value:
        efr_success - an error is valid and has been reported.
        efr_invalid_subsystem_id - invalid subsystem ID.
        efr_invalid_error_code - invalid error code.
        efr_error_subsys_init_failure - the error subsystem wasn't
                                        successfully initialized.
 */

dx_error_function_result_t dx_set_last_error (dx_subsystem_code_t subsystem_id, int error_code);

/* -------------------------------------------------------------------------- */
/*
 *  Retrieves an information about a previously reported error.

    Input:
        subsystem_id - a pointer to a variable for storing the subsystem ID
                       of an error; may be NULL if the ID is not required.
        error_code - a pointer to a variable for storing the error code;
                     may be NULL if the code is not required.
        error_descr - a pointer to a variable for storing the error
                      text description; optional, may be NULL if the
                      description is not required.
                      
    Return value:
        efr_success - an error information was retrieved.
        efr_no_error_stored - no error was previously reported, nothing to retrieve.
        efr_error_subsys_init_failure - the error subsystem wasn't
                                        successfully initialized.
 */

dx_error_function_result_t dx_get_last_error (int* subsystem_id, int* error_code, dx_const_string_t* error_descr);

/* -------------------------------------------------------------------------- */
/*
 *  Erases the information about the last reported error, thus making the
    subsequent successful 'dx_get_last_error' calls return 'efr_no_error_stored'.

    Input:
        none.
        
    Return value:
        true - the last error info is successfully erased.
        false - function call failed due to the error subsystem
                initialization failure.
 */

bool dx_pop_last_error ();

/* -------------------------------------------------------------------------- */
/*
 *	Explicitly initializes the error subsystem.
    Should not be called unless you know what you're doing.
    
    Input:
        none.
        
    Return value:
        true - the error subsystem successfully initialized
        false - the error subsystem initialization failed.
 */
 
bool dx_init_error_subsystem ();

#endif /* DX_ERROR_HANDLING_H_INCLUDED */