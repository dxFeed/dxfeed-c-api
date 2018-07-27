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
 *	Error subsystem function return value roster
 */
/* -------------------------------------------------------------------------- */

typedef enum {
	dx_efr_success,
	dx_efr_invalid_error_code,
	dx_efr_error_subsys_init_failure
} dx_error_function_result_t;

/* -------------------------------------------------------------------------- */
/*
 *	The error manipulation functions
 */
/* -------------------------------------------------------------------------- */
/*
 *  Reports an error.

	Input:
		error_code - an error code.

	Return value:
		dx_efr_success - an error is valid and has been reported.
		dx_efr_invalid_error_code - invalid error code.
		dx_efr_error_subsys_init_failure - the error subsystem wasn't
										successfully initialized.
 */

dx_error_function_result_t dx_set_last_error(dx_error_code_t error_code);

dx_error_function_result_t dx_set_last_error_impl(dx_error_code_t error_code, bool with_logging);

/* -------------------------------------------------------------------------- */
/*
 *  Retrieves an information about a previously reported error.

	Input:
		error_code - a pointer to a variable for storing the error code;
					MUST NOT be NULL.

	Return value:
		dx_efr_success - an error information was retrieved.
		dx_efr_invalid_error_code - error_code is NULL.
		dx_efr_error_subsys_init_failure - the error subsystem wasn't
										successfully initialized.
 */

dx_error_function_result_t dx_get_last_error (int* error_code);

/* -------------------------------------------------------------------------- */
/*
 *  Erases the information about the last reported error, thus making the
	subsequent successful 'dx_get_last_error' calls return 'dx_ec_success'
	error code.

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

bool dx_init_error_subsystem (void);

/* -------------------------------------------------------------------------- */
/*
 *	Convenient helper functions
 */
/* -------------------------------------------------------------------------- */

/*
 *	Returns the currently stored error code.
 *  If the error subsystem had failed to initialize, returns 'dx_ec_internal_assert_violation'.
 */
dx_error_code_t dx_get_error_code (void);

/*
 *	Sets the given error code as the last error and returns 'false'.
 *  Logs the cases when 'dx_set_last_error' didn't return success.
 */

bool dx_set_error_code (dx_error_code_t code);

bool dx_set_error_code_impl(dx_error_code_t code, bool with_logging);

#endif /* DX_ERROR_HANDLING_H_INCLUDED */