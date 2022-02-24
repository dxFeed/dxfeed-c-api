/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 2.0 (the "License"); you may not use this file except in compliance with
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
	switch (code) {
	/* common error codes */

	case dx_ec_success: return L"Success";

	case dx_ec_error_subsystem_failure: return L"The initialization of the error handling subsystem was failed";
	case dx_ec_invalid_func_param: return L"Invalid function parameter";
	case dx_ec_invalid_func_param_internal: return L"Invalid function parameter(internal)";
	case dx_ec_internal_assert_violation: return L"Internal assert violation";

	/* memory error codes */

	case dx_mec_insufficient_memory: return L"Not enough memory to complete operation";

	/* socket error codes */

	case dx_sec_socket_subsystem_init_failed: return L"Socket subsystem initialization failed"; /* Win32-specific */
	case dx_sec_socket_subsystem_init_required: return L"Socket subsystem is not initialized"; /* Win32-specific */
	case dx_sec_socket_subsystem_incompatible_version: return L"Incompatible version of socket subsystem"; /* Win32-specific */
	case dx_sec_connection_gracefully_closed: return L"Connection gracefully closed by peer";
	case dx_sec_network_is_down: return L"Network is down (WSAENETDOWN/ENETDOWN)";
	case dx_sec_blocking_call_in_progress: return L"Socket operation now in progress (EINPROGRESS/WSAEINPROGRESS)";
	case dx_sec_addr_family_not_supported: return L"Address family not supported by protocol family (WSAEAFNOSUPPORT/EAFNOSUPPORT)";
	case dx_sec_no_sockets_available: return L"No sockets available (WSAEMFILE/EMFILE)";
	case dx_sec_no_buffer_space_available: return L"Not enough space in socket buffers (WSAENOBUFS/ENOBUFS)";
	case dx_sec_proto_not_supported: return L"Protocol not supported (WSAEPROTONOSUPPORT/EPROTONOSUPPORT)";
	case dx_sec_socket_type_proto_incompat: return L"Protocol wrong type for socket (WSAEPROTOTYPE/EPROTOTYPE)";
	case dx_sec_socket_type_addrfam_incompat: return L"Socket type not supported (WSAESOCKTNOSUPPORT/ESOCKTNOSUPPORT)";
	case dx_sec_addr_already_in_use: return L"Address already in use (WSAEADDRINUSE/EADDRINUSE)";
	case dx_sec_blocking_call_interrupted: return L"Interrupted socket function call (WSAEINTR/EINTR)";
	case dx_sec_nonblocking_oper_pending: return L"Socket operation already in progress (WSAEALREADY/EALREADY)";
	case dx_sec_addr_not_valid: return L"Cannot assign requested address. The requested address is not valid in its context (WSAEADDRNOTAVAIL/EADDRNOTAVAIL)";
	case dx_sec_connection_refused: return L"Connection refused (WSAECONNREFUSED/ECONNREFUSED)";
	case dx_sec_invalid_ptr_arg: return L"Bad address. The system detected an invalid pointer address in attempting to use a pointer argument of a call (WSAEFAULT/EFAULT)";
	case dx_sec_invalid_arg: return L"Invalid argument (WSAEINVAL/EINVAL)";
	case dx_sec_sock_already_connected: return L"Socket is already connected (WSAEISCONN/EISCONN)";
	case dx_sec_network_is_unreachable: return L"Network is unreachable (WSAENETUNREACH/ENETUNREACH)";
	case dx_sec_sock_oper_on_nonsocket: return L"Socket operation on nonsocket (WSAENOTSOCK/ENOTSOCK)";
	case dx_sec_connection_timed_out: return L"Connection timed out (WSAETIMEDOUT/ETIMEDOUT)";
	case dx_sec_res_temporarily_unavail: return L"Resource temporarily unavailable (WSAEWOULDBLOCK/EWOULDBLOCK)";
	case dx_sec_permission_denied: return L"Permission to socket operation denied (WSAEACCES/EACCES)";
	case dx_sec_network_dropped_connection: return L"Network dropped connection on reset. The connection has been broken due to keep-alive activity detecting a failure while the operation was in progress. (WSAENETRESET/ENETRESET)";
	case dx_sec_socket_not_connected: return L"Socket is not connected. Connection to the specified address failed (WSAENOTCONN/ENOTCONN)";
	case dx_sec_operation_not_supported: return L"Socket operation not supported (WSAEOPNOTSUPP/EOPNOTSUPP)";
	case dx_sec_socket_shutdown: return L"Cannot send after socket shutdown (WSAESHUTDOWN/ESHUTDOWN)";
	case dx_sec_message_too_long: return L"Message too long. A message sent on a datagram socket was larger than the internal message buffer or some other network limit(WSAEMSGSIZE/EMSGSIZE)";
	case dx_sec_no_route_to_host: return L"No route to host. A socket operation was attempted to an unreachable host (WSAEHOSTUNREACH/EHOSTUNREACH)";
	case dx_sec_connection_aborted: return L"Connection aborted. An established connection was aborted by the software in your host computer (WSAECONNABORTED/ECONNABORTED)";
	case dx_sec_connection_reset: return L"Connection reset by peer. An existing connection was forcibly closed by the remote host (WSAECONNRESET/ECONNRESET)";
	case dx_sec_persistent_temp_error: return L"Nonauthoritative host not found. This is usually a temporary error during host name resolution and means that the local server did not receive a response from an authoritative server. (WSATRY_AGAIN/EAI_AGAIN)";
	case dx_sec_unrecoverable_error: return L"A nonrecoverable error (WSANO_RECOVERY). Check hosts, services, protocols files and DNS";
	case dx_sec_not_enough_memory: return L"Not enough memory to complete a socket operation (WSA_NOT_ENOUGH_MEMORY/EAI_MEMORY/EAI_OVERFLOW)";
	case dx_sec_no_data_on_host: return L"Valid name, no data record of requested type. The requested name is valid and was found in the database (WSANO_DATA). Check DNS";
	case dx_sec_host_not_found: return L"Host not found (WSAHOST_NOT_FOUND/EAI_NONAME)";

	case dx_sec_generic_error: return L"Unrecognized socket error";

	/* thread error codes */

	case dx_tec_not_enough_sys_resources: return L"Not enough system resources to perform thread operation";
	case dx_tec_permission_denied: return L"Permission to thread operation denied";
	case dx_tec_invalid_res_operation: return L"Thread resource temporarily unavailable";
	case dx_tec_invalid_resource_id: return L"Invalid argument or resource id for thread operation";
	case dx_tec_deadlock_detected: return L"Thread resource deadlock detected";
	case dx_tec_not_enough_memory: return L"Not enough memory to perform thread operation";
	case dx_tec_resource_busy: return L"Mutex resource is busy";

	case dx_tec_generic_error: return L"Unrecognized thread error";

	/* network error codes */

	case dx_nec_invalid_port_value: return L"Server address has invalid port value";
	case dx_nec_invalid_function_arg: return L"Network subsystem: invalid function arg";
	case dx_nec_connection_closed: return L"Attempt to use an already closed connection or an internal connection error";
	case dx_nec_open_connection_error: return L"Failed to establish connection with address";
	case dx_nec_unknown_codec: return L"One of codecs in your address string is unknown or not supported";

	/* buffered I/O error codes */

	case dx_bioec_buffer_overflow: return L"I/O: buffer overflow";
	case dx_bioec_buffer_not_initialized: return L"I/O: buffer not initialized";
	case dx_bioec_index_out_of_bounds: return L"I/O: index out of bounds";
	case dx_bioec_buffer_underflow: return L"I/O: buffer underflow";

	/* UTF error codes */

	case dx_utfec_bad_utf_data_format: return L"Client data contains bad formatted UTF string";
	case dx_utfec_bad_utf_data_format_server: return L"Server data contains bad formatted UTF string";

	/* penta codec error codes */

	case dx_pcec_reserved_bit_sequence: return L"Server data symbol format error";
	case dx_pcec_invalid_symbol_length: return L"Server data symbol length error";
	case dx_pcec_invalid_event_flag: return L"Duplicated event flags prefix";

	/* event subscription error codes */

	case dx_esec_invalid_event_type: return L"Invalid event type";
	case dx_esec_invalid_subscr_id: return L"Invalid subscription descriptor";
	case dx_esec_invalid_symbol_name: return L"Invalid symbol name";
	case dx_esec_invalid_listener: return L"Invalid listener";

	/* logger error codes */

	case dx_lec_failed_to_open_file: return L"Failed to open the log file";

	/* protocol messages error codes */

	case dx_pmec_invalid_message_type: return L"Invalid message type";

	/* protocol error codes */

	case dx_pec_unexpected_message_type: return L"Unexpected message type";
	case dx_pec_unexpected_message_type_internal: return L"Unexpected message type (internal)";
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
	case dx_pec_unexpected_message_sequence_internal: return L"Unexpected message sequence (internal)";
	case dx_pec_local_message_not_supported_by_server: return L"Local message is not supported by server";
	case dx_pec_inconsistent_message_support: return L"Inconsistent message support by server";
	case dx_pec_authentication_error: return L"Authentication failed";
	case dx_pec_credentials_required: return L"Server required credentials data";

	/* connection error codes */

	case dx_cec_invalid_connection_handle: return L"Invalid connection handle";
	case dx_cec_invalid_connection_handle_internal: return L"Invalid connection handle (internal)";
	case dx_cec_connection_context_not_initialized: return L"Connection context not initialized";
	case dx_cec_invalid_connection_context_subsystem_id: return L"Invalid connection context subsystem";

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

	case dx_cfgec_empty_config_string: return L"Empty config string";
	case dx_cfgec_empty_config_file_name: return L"Empty config file name";
	case dx_cfgec_toml_parser_error: return L"TOML parser error";

	case dx_plbec_invalid_symbol: return L"PLB: invalid symbol parameter";
	case dx_plbec_invalid_source: return L"PLB: invalid source parameter";
	case dx_plbec_invalid_book_ptr: return L"PLB: invalid book pointer parameter";
	case dx_plbec_invalid_book_handle: return L"PLB: invalid book handle";

	/* miscellaneous error codes */

	default: return L"Invalid error code";
	}
}

dx_log_level_t dx_get_log_level(dx_error_code_t code) {
	dx_log_level_t result;

	switch (code) {
		case dx_sec_connection_gracefully_closed:
		case dx_sec_blocking_call_interrupted:
		case dx_bioec_buffer_underflow:
			result = dx_ll_info;
			break;

		default:
			result = dx_ll_error;
	}

	return result;
}
