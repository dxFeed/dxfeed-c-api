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

#ifdef _WIN32
#include <Windows.h>
#endif  /* _WIN32 */

#include "DXErrorHandling.h"
#include "DXErrorCodes.h"
#include "Logger.h"
#include "DXThreads.h"
#include "DXMemory.h"

/* -------------------------------------------------------------------------- */
/*
 *	Thread-specific stuff
 */
/* -------------------------------------------------------------------------- */

static bool g_initialization_attempted = false;
static dx_key_t g_last_error_data_key;

static dx_error_code_t g_master_thread_last_error_code = dx_ec_success;

/* -------------------------------------------------------------------------- */

static void dx_key_data_destructor (void* data) {
	if (data != &g_master_thread_last_error_code && data != NULL) {
		dx_free(data);
	}
}

static void dx_key_remover(void *data) {
	if (g_initialization_attempted) {
		dx_thread_data_key_destroy(g_last_error_data_key);
		g_initialization_attempted = false;
	}
}

/* -------------------------------------------------------------------------- */
/*
 *	Internal error handling helpers
 */
/* -------------------------------------------------------------------------- */

dx_error_function_result_t dx_check_error_code (dx_error_code_t error_code) {
	if (error_code < 0 || error_code >= dx_ec_count) {
		return dx_efr_invalid_error_code;
	}

	return dx_efr_success;
}

/* -------------------------------------------------------------------------- */
/*
 *	Error manipulation functions implementation
 */
/* -------------------------------------------------------------------------- */

dx_error_function_result_t dx_set_last_error(dx_error_code_t error_code) {
	return dx_set_last_error_impl(error_code, true);
}

dx_error_function_result_t dx_set_last_error_impl(dx_error_code_t error_code, bool with_logging) {
	dx_error_code_t* error_data = NULL;
	dx_error_function_result_t res;

	if (error_code == dx_ec_success) {
		/* that's a special case used to prune the previously stored error */

		res = dx_efr_success;
	} else {
		if (error_code)

		res = dx_check_error_code(error_code);
	}

	if (res != dx_efr_success) {
		return res;
	}

	if (!dx_init_error_subsystem()) {
		return dx_efr_error_subsys_init_failure;
	}

	if (dx_is_thread_master()) {
		error_data = &g_master_thread_last_error_code;
	} else {
		error_data = dx_get_thread_data(g_last_error_data_key);

		if (error_data == NULL) {
			return dx_efr_error_subsys_init_failure;
		}
	}

	*error_data = error_code;

	if (with_logging && error_code != dx_ec_success) {
		dx_logging_error_by_code(error_code);
	}

	return dx_efr_success;
}

/* -------------------------------------------------------------------------- */

dx_error_function_result_t dx_get_last_error (int* error_code) {
	dx_error_code_t* error_data = NULL;

	if (error_code == NULL) {
		return dx_efr_invalid_error_code;
	}

	if (!dx_init_error_subsystem()) {
		return dx_efr_error_subsys_init_failure;
	}

	if (dx_is_thread_master()) {
		error_data = &g_master_thread_last_error_code;
	} else {
		error_data = dx_get_thread_data(g_last_error_data_key);

		if (error_data == NULL) {
			return dx_efr_error_subsys_init_failure;
		}
	}

	if (error_code == NULL) {
		return dx_efr_invalid_error_code;
	}

	*error_code = *error_data;

	return dx_efr_success;
}

/* -------------------------------------------------------------------------- */

bool dx_pop_last_error () {
	return (dx_set_last_error(dx_ec_success) == dx_efr_success);
}

/* -------------------------------------------------------------------------- */

bool dx_init_error_subsystem (void) {
	dx_error_code_t* error_data = NULL;

	if (!g_initialization_attempted) {
		g_initialization_attempted = true;

		dx_mark_thread_master();

		if (!dx_thread_data_key_create(&g_last_error_data_key, dx_key_data_destructor)) {
			return false;
		}

		if (!dx_set_thread_data(g_last_error_data_key, &g_master_thread_last_error_code)) {
			return false;
		}

		dx_register_process_destructor(&dx_key_remover, NULL);

		return true;
	} else if (dx_is_thread_master()) {
		return true;
	}

	/* only the secondary threads reach here */

	if (dx_get_thread_data(g_last_error_data_key) != NULL) {
		return true;
	}

	error_data = (dx_error_code_t*)dx_calloc_no_ehm(1, sizeof(dx_error_code_t));

	if (error_data == NULL) {
		return false;
	}

	*error_data = dx_ec_success;

	if (!dx_set_thread_data_no_ehm(g_last_error_data_key, (const void*)error_data)) {
		dx_free(error_data);

		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Convenient helper functions implementation
 */
/* -------------------------------------------------------------------------- */

dx_error_code_t dx_get_error_code (void) {
	int code;

	if (dx_get_last_error(&code) == dx_efr_error_subsys_init_failure) {
		return dx_ec_internal_assert_violation;
	} else {
		return code;
	}
}

/* -------------------------------------------------------------------------- */

bool dx_set_error_code (dx_error_code_t code) {
	return dx_set_error_code_impl(code, true);
}

bool dx_set_error_code_impl(dx_error_code_t code, bool with_logging) {
	dx_error_function_result_t res = dx_set_last_error_impl(code, with_logging);

	if (res != dx_efr_success) {
		dx_logging_info(L"Setting error code failed: code = %d, return value = %d", code, res);
	}

	return false;
}