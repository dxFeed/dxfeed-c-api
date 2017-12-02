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

#include "DXFeed.h"

#include "ConnectionContextData.h"
#include "DXMemory.h"
#include "DXErrorHandling.h"
#include "DXAlgorithms.h"
#include "Logger.h"

/* -------------------------------------------------------------------------- */
/*
 *	Various data
 */
/* -------------------------------------------------------------------------- */

static const dx_conn_ctx_subsys_manipulator_t g_initializer_queue[dx_ccs_count] = {
	DX_CONNECTION_SUBSYS_INIT_NAME(dx_ccs_network),
	DX_CONNECTION_SUBSYS_INIT_NAME(dx_ccs_data_structures),
	DX_CONNECTION_SUBSYS_INIT_NAME(dx_ccs_buffered_input),
	DX_CONNECTION_SUBSYS_INIT_NAME(dx_ccs_buffered_output),
	DX_CONNECTION_SUBSYS_INIT_NAME(dx_ccs_record_buffers),
	DX_CONNECTION_SUBSYS_INIT_NAME(dx_ccs_server_msg_processor),
	DX_CONNECTION_SUBSYS_INIT_NAME(dx_ccs_event_subscription),
	DX_CONNECTION_SUBSYS_INIT_NAME(dx_ccs_record_transcoder),
	DX_CONNECTION_SUBSYS_INIT_NAME(dx_ccs_snapshot_subscription),
	DX_CONNECTION_SUBSYS_INIT_NAME(dx_ccs_price_level_book),
	DX_CONNECTION_SUBSYS_INIT_NAME(dx_ccs_regional_book)
};

static const dx_conn_ctx_subsys_manipulator_t g_deinitializer_queue[dx_ccs_count] = {
	DX_CONNECTION_SUBSYS_DEINIT_NAME(dx_ccs_network),
	DX_CONNECTION_SUBSYS_DEINIT_NAME(dx_ccs_server_msg_processor),
	DX_CONNECTION_SUBSYS_DEINIT_NAME(dx_ccs_event_subscription),
	DX_CONNECTION_SUBSYS_DEINIT_NAME(dx_ccs_data_structures),
	DX_CONNECTION_SUBSYS_DEINIT_NAME(dx_ccs_buffered_input),
	DX_CONNECTION_SUBSYS_DEINIT_NAME(dx_ccs_buffered_output),
	DX_CONNECTION_SUBSYS_DEINIT_NAME(dx_ccs_record_buffers),
	DX_CONNECTION_SUBSYS_DEINIT_NAME(dx_ccs_record_transcoder),
	DX_CONNECTION_SUBSYS_DEINIT_NAME(dx_ccs_snapshot_subscription),
	DX_CONNECTION_SUBSYS_DEINIT_NAME(dx_ccs_price_level_book),
	DX_CONNECTION_SUBSYS_DEINIT_NAME(dx_ccs_regional_book)
};

static const dx_conn_ctx_subsys_manipulator_t g_check_queue[dx_ccs_count] = {
	DX_CONNECTION_SUBSYS_CHECK_NAME(dx_ccs_network),
	DX_CONNECTION_SUBSYS_CHECK_NAME(dx_ccs_server_msg_processor),
	DX_CONNECTION_SUBSYS_CHECK_NAME(dx_ccs_event_subscription),
	DX_CONNECTION_SUBSYS_CHECK_NAME(dx_ccs_data_structures),
	DX_CONNECTION_SUBSYS_CHECK_NAME(dx_ccs_buffered_input),
	DX_CONNECTION_SUBSYS_CHECK_NAME(dx_ccs_buffered_output),
	DX_CONNECTION_SUBSYS_CHECK_NAME(dx_ccs_record_buffers),
	DX_CONNECTION_SUBSYS_CHECK_NAME(dx_ccs_record_transcoder),
	DX_CONNECTION_SUBSYS_CHECK_NAME(dx_ccs_snapshot_subscription),
	DX_CONNECTION_SUBSYS_CHECK_NAME(dx_ccs_price_level_book),
	DX_CONNECTION_SUBSYS_CHECK_NAME(dx_ccs_regional_book)
};
/* -------------------------------------------------------------------------- */
/*
 *	Various types
 */
/* -------------------------------------------------------------------------- */

typedef struct {
	void* subsystem_data[dx_ccs_count];
} dx_connection_data_collection_t;

/* -------------------------------------------------------------------------- */
/*
 *	Main functions implementation
 */
/* -------------------------------------------------------------------------- */

dxf_connection_t dx_init_connection (void) {
	dx_connection_data_collection_t* res = dx_calloc(1, sizeof(dx_connection_data_collection_t));
	int i = dx_ccs_begin;

	if (res == NULL) {
		return NULL;
	}
	for (; i < dx_ccs_count; ++i) {
		if (!g_initializer_queue[i]((dxf_connection_t)res)) {
			dx_deinit_connection((dxf_connection_t)res);

			return NULL;
		}
	}

	return (dxf_connection_t)res;
}

/* -------------------------------------------------------------------------- */

bool dx_deinit_connection (dxf_connection_t connection) {
	int i = dx_ccs_begin;
	bool res = true;

	for (; i < dx_ccs_count; ++i) {
		res = g_deinitializer_queue[i](connection) && res;
	}

	dx_free(connection);

	return res;
}

/* -------------------------------------------------------------------------- */

bool dx_can_deinit_connection (dxf_connection_t connection) {
	int i = dx_ccs_begin;

	for (; i < dx_ccs_count; ++i) {
		if (!g_check_queue[i](connection)) {
			return false;
		}
	}

	return true;
}

/* -------------------------------------------------------------------------- */

void* dx_get_subsystem_data (dxf_connection_t connection, dx_connection_context_subsystem_t subsystem, OUT bool* res) {
	if (connection == NULL) {
		dx_set_error_code(dx_cec_invalid_connection_handle_internal);
		DX_CHECKED_SET_VAL_TO_PTR(res, false);

		return NULL;
	}

	if (subsystem < dx_ccs_begin || subsystem >= dx_ccs_count) {
		dx_set_last_error(dx_cec_invalid_connection_context_subsystem_id);
		DX_CHECKED_SET_VAL_TO_PTR(res, false);

		return NULL;
	}

	DX_CHECKED_SET_VAL_TO_PTR(res, true);

	return ((dx_connection_data_collection_t*)connection)->subsystem_data[subsystem];
}

/* -------------------------------------------------------------------------- */

bool dx_set_subsystem_data (dxf_connection_t connection, dx_connection_context_subsystem_t subsystem, void* data) {
	if (connection == NULL) {
		dx_set_last_error(dx_cec_invalid_connection_handle_internal);

		return false;
	}

	if (subsystem < dx_ccs_begin || subsystem >= dx_ccs_count) {
		dx_set_last_error(dx_cec_invalid_connection_context_subsystem_id);

		return false;
	}

	((dx_connection_data_collection_t*)connection)->subsystem_data[subsystem] = data;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_validate_connection_handle (dxf_connection_t connection, bool is_internal) {
	if (connection == NULL) {
		return dx_set_error_code(is_internal ? dx_cec_invalid_connection_handle_internal : dx_cec_invalid_connection_handle);
	}

	return true;
}