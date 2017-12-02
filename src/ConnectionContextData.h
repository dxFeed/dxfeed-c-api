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
 *	Provides functionality to manage the connection-specific data.
 */

#ifndef CONNECTION_CONTEXT_DATA_H_INCLUDED
#define CONNECTION_CONTEXT_DATA_H_INCLUDED

#include "PrimitiveTypes.h"
#include "DXTypes.h"

/* -------------------------------------------------------------------------- */
/*
 *	Various types
 */
/* -------------------------------------------------------------------------- */

typedef enum {
	dx_ccs_begin,
	dx_ccs_network = dx_ccs_begin,
	dx_ccs_event_subscription,
	dx_ccs_record_transcoder,
	dx_ccs_data_structures,
	dx_ccs_record_buffers,
	dx_ccs_server_msg_processor,
	dx_ccs_buffered_input,
	dx_ccs_buffered_output,
	dx_ccs_snapshot_subscription,
	dx_ccs_price_level_book,
	dx_ccs_regional_book,

	dx_ccs_count
} dx_connection_context_subsystem_t;

typedef bool (*dx_conn_ctx_subsys_manipulator_t) (dxf_connection_t connection);

/* -------------------------------------------------------------------------- */
/*
 *	Subsystem initializer/deinitializer macros
 */
/* -------------------------------------------------------------------------- */

#define DX_CONNECTION_SUBSYS_INIT_NAME(subsys_id) \
	subsys_id##_init

#define DX_CONNECTION_SUBSYS_INIT_PROTO(subsys_id) \
	bool DX_CONNECTION_SUBSYS_INIT_NAME(subsys_id) (dxf_connection_t connection)

#define DX_CONNECTION_SUBSYS_DEINIT_NAME(subsys_id) \
	subsys_id##_deinit

#define DX_CONNECTION_SUBSYS_DEINIT_PROTO(subsys_id) \
	bool DX_CONNECTION_SUBSYS_DEINIT_NAME(subsys_id) (dxf_connection_t connection)

#define DX_CONNECTION_SUBSYS_CHECK_NAME(subsys_id) \
	subsys_id##_check

#define DX_CONNECTION_SUBSYS_CHECK_PROTO(subsys_id) \
	bool DX_CONNECTION_SUBSYS_CHECK_NAME(subsys_id) (dxf_connection_t connection)

/* -------------------------------------------------------------------------- */
/*
 *	Subsystem initializer/deinitializer prototypes
 */
/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_network);
DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_event_subscription);
DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_record_transcoder);
DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_data_structures);
DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_record_buffers);
DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_server_msg_processor);
DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_buffered_input);
DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_buffered_output);
DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_snapshot_subscription);
DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_price_level_book);
DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_regional_book);
DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_network);
DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_event_subscription);
DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_record_transcoder);
DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_data_structures);
DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_record_buffers);
DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_server_msg_processor);
DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_buffered_input);
DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_buffered_output);
DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_snapshot_subscription);
DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_price_level_book);
DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_regional_book);
DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_network);
DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_event_subscription);
DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_record_transcoder);
DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_data_structures);
DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_record_buffers);
DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_server_msg_processor);
DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_buffered_input);
DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_buffered_output);
DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_snapshot_subscription);
DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_price_level_book);
DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_regional_book);

/* -------------------------------------------------------------------------- */
/*
 *	Main functions
 */
/* -------------------------------------------------------------------------- */

dxf_connection_t dx_init_connection (void);
bool dx_deinit_connection (dxf_connection_t connection);
bool dx_can_deinit_connection (dxf_connection_t connection);
void* dx_get_subsystem_data (dxf_connection_t connection, dx_connection_context_subsystem_t subsystem, OUT bool* res);
bool dx_set_subsystem_data (dxf_connection_t connection, dx_connection_context_subsystem_t subsystem, void* data);
bool dx_validate_connection_handle (dxf_connection_t connection, bool is_internal);

#endif /* CONNECTION_CONTEXT_DATA_H_INCLUDED */