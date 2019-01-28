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

#include "DXErrorCodes.h"

/* -------------------------------------------------------------------------- */
/*
 *	Message description functions implementation
 */
/* -------------------------------------------------------------------------- */

dxf_const_string_t dx_get_error_description (dx_error_code_t code) {
	static dxf_const_string_t s_internal_error_descr = L"Internal software error";

	switch (code) {
	/* common error codes */

	case dx_ec_success: return L"Success";

	case dx_ec_error_subsystem_failure: return s_internal_error_descr;

	case dx_ec_invalid_func_param: return L"Invalid function parameter";
	case dx_ec_invalid_func_param_internal: return s_internal_error_descr;

	case dx_ec_internal_assert_violation: return s_internal_error_descr;

	/* memory error codes */

	case dx_mec_insufficient_memory: return L"Not enough memory to complete operation";

	/* socket error codes */

	case dx_sec_socket_subsystem_init_failed: return L"Socket subsystem initialization failed"; /* Win32-specific */
	case dx_sec_socket_subsystem_init_required: return s_internal_error_descr; /* Win32-specific */
	case dx_sec_socket_subsystem_incompatible_version: return L"Incompatible version of socket subsystem"; /* Win32-specific */
	case dx_sec_connection_gracefully_closed: return L"Connection gracefully closed by peer";
	case dx_sec_network_is_down: return L"Network is down";
	case dx_sec_blocking_call_in_progress: return s_internal_error_descr;
	case dx_sec_addr_family_not_supported: return s_internal_error_descr;
	case dx_sec_no_sockets_available: return L"No sockets available";
	case dx_sec_no_buffer_space_available: return L"Not enough space in socket buffers";
	case dx_sec_proto_not_supported: return s_internal_error_descr;
	case dx_sec_socket_type_proto_incompat: return s_internal_error_descr;
	case dx_sec_socket_type_addrfam_incompat: return s_internal_error_descr;
	case dx_sec_addr_already_in_use: return s_internal_error_descr;
	case dx_sec_blocking_call_interrupted: return s_internal_error_descr;
	case dx_sec_nonblocking_oper_pending: return s_internal_error_descr;
	case dx_sec_addr_not_valid: return s_internal_error_descr;
	case dx_sec_connection_refused: return L"Connection refused";
	case dx_sec_invalid_ptr_arg: return s_internal_error_descr;
	case dx_sec_invalid_arg: return s_internal_error_descr;
	case dx_sec_sock_already_connected: return s_internal_error_descr;
	case dx_sec_network_is_unreachable: return L"Network is unreachable";
	case dx_sec_sock_oper_on_nonsocket: return s_internal_error_descr;
	case dx_sec_connection_timed_out: return L"Connection timed out";
	case dx_sec_res_temporarily_unavail: return s_internal_error_descr;
	case dx_sec_permission_denied: return L"Permission to socket operation denied";
	case dx_sec_network_dropped_connection: return L"Network dropped connection on reset";
	case dx_sec_socket_not_connected: return L"Connection to the specified address failed";
	case dx_sec_operation_not_supported: return s_internal_error_descr;
	case dx_sec_socket_shutdown: return s_internal_error_descr;
	case dx_sec_message_too_long: return s_internal_error_descr;
	case dx_sec_no_route_to_host: return L"No route to host";
	case dx_sec_connection_aborted: return L"Connection aborted";
	case dx_sec_connection_reset: return L"Connection reset";
	case dx_sec_persistent_temp_error: return L"Temporary failure in name resolution persists for too long";
	case dx_sec_unrecoverable_error: return s_internal_error_descr;
	case dx_sec_not_enough_memory: return L"Not enough memory to complete a socket operation";
	case dx_sec_no_data_on_host: return s_internal_error_descr;
	case dx_sec_host_not_found: return L"Host not found";

	case dx_sec_generic_error: return L"Unrecognized socket error";

	/* thread error codes */

	case dx_tec_not_enough_sys_resources: return L"Not enough system resources to perform thread operation";
	case dx_tec_permission_denied: return L"Permission to thread operation denied";
	case dx_tec_invalid_res_operation: return s_internal_error_descr;
	case dx_tec_invalid_resource_id: return s_internal_error_descr;
	case dx_tec_deadlock_detected: return s_internal_error_descr;
	case dx_tec_not_enough_memory: return L"Not enough memory to perform thread operation";
	case dx_tec_resource_busy: return s_internal_error_descr;

	case dx_tec_generic_error: return L"Unrecognized thread error";

	/* network error codes */

	case dx_nec_invalid_port_value: return L"Server address has invalid port value";
	case dx_nec_invalid_function_arg: return s_internal_error_descr;
	case dx_nec_connection_closed: return L"Attempt to use an already closed connection or an internal connection error";
	case dx_nec_open_connection_error: return L"Failed to establish connection with address";
	case dx_nec_unknown_codec: return L"One of codecs in your address string is unknown or not supported";

	/* buffered I/O error codes */

	case dx_bioec_buffer_overflow: return s_internal_error_descr;
	case dx_bioec_buffer_not_initialized: return s_internal_error_descr;
	case dx_bioec_index_out_of_bounds: return s_internal_error_descr;
	case dx_bioec_buffer_underflow: return s_internal_error_descr;

	/* UTF error codes */

	case dx_utfec_bad_utf_data_format: return s_internal_error_descr;
	case dx_utfec_bad_utf_data_format_server: return L"Server data contains bad formatted UTF string";

	/* penta codec error codes */

	case dx_pcec_reserved_bit_sequence: return L"Server data symbol format error";
	case dx_pcec_invalid_symbol_length: return L"Server data symbol format error";
	case dx_pcec_invalid_event_flag: return L"Duplicated event flags prefix";

	/* event subscription error codes */

	case dx_esec_invalid_event_type: return L"Invalid event type";
	case dx_esec_invalid_subscr_id: return L"Invalid subscription descriptor";
	case dx_esec_invalid_symbol_name: return L"Invalid symbol name";
	case dx_esec_invalid_listener: return L"Invalid listener";

	/* logger error codes */

	case dx_lec_failed_to_open_file: return L"Failed to open the log file";

	/* protocol messages error codes */

	case dx_pmec_invalid_message_type: return s_internal_error_descr;

	/* protocol error codes */

	case dx_pec_unexpected_message_type: return L"Unexpected message type";
	case dx_pec_unexpected_message_type_internal: return s_internal_error_descr;
	case dx_pec_descr_record_field_info_corrupted: return L"Corrupted field information in server data description";
	case dx_pec_message_incomplete: return L"Server message is incomplete";
	case dx_pec_invalid_message_length: return L"Invalid server message length";
	case dx_pec_server_message_not_supported: return L"Server message is not supported";
	case dx_pec_invalid_symbol: return L"Server data message contains invalid symbol";
	case dx_pec_record_description_not_received: return L"Data received for record with no description from server";
	case dx_pec_record_field_type_not_supported: return L"Server record field type not supported";
	case dx_pec_record_info_corrupted: return L"Server record information corrupted";
	case dx_pec_unknown_record_name: return L"Unknown server record name";
	case dx_pec_record_not_supported: return L"Server record not supported";
	case dx_pec_describe_protocol_message_corrupted: return L"Server describe protocol message corrupted";
	case dx_pec_unexpected_message_sequence_internal: return s_internal_error_descr;
	case dx_pec_local_message_not_supported_by_server: return L"Local message is not supported by server";
	case dx_pec_inconsistent_message_support: return L"Inconsistent message support by server";
	case dx_pec_authentication_error: return L"Authentication failed";
	case dx_pec_credentials_required: return L"Server required credentials data";

	/* connection error codes */

	case dx_cec_invalid_connection_handle: return L"Invalid connection handle";
	case dx_cec_invalid_connection_handle_internal: return s_internal_error_descr;
	case dx_cec_connection_context_not_initialized: return s_internal_error_descr;
	case dx_cec_invalid_connection_context_subsystem_id: return s_internal_error_descr;

	/* candle event error codes*/

	case dx_ceec_invalid_candle_period_value: return L"Invalid candle event period value";


	/* snapshot error codes */

	case dx_ssec_invalid_snapshot_id: return L"Invalid snapshot descriptor";
	case dx_ssec_invalid_event_id: return L"Invalid event id";
	case dx_ssec_invalid_symbol: return L"Invalid or empty symbol string";
	case dx_ssec_snapshot_exist: return L"Snapshot with such event id and symbol already exists";
	case dx_ssec_invalid_listener: return L"Invalid snapshot listener";
	case dx_ssec_unknown_state: return L"Unknown state of snapshot flags";
	case dx_ssec_duplicate_record: return L"Inserted record is already exist";

	/* configuration record serialization deserialization error codes */

	case dx_csdec_protocol_error: return L"Unexpected token is reached or data is damaged";
	case dx_csdec_unsupported_version: return L"Current stream version of protocol is not supported";

	/* miscellaneous error codes */

	default: return L"Invalid error code";
	}
}