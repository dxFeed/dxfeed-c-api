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

#ifndef DX_ERROR_CODES_H_INCLUDED
#define DX_ERROR_CODES_H_INCLUDED

/* -------------------------------------------------------------------------- */
/*
 *	Subsystem codes
 
 *  a complete roster of subsystem codes used throughout the project
 */
/* -------------------------------------------------------------------------- */

enum dx_subsystem_code_t {
    sc_memory,
    sc_sockets,
    sc_threads,
    sc_network,
    
    /*  add new subsystem codes above this line 
        also don't forget to modify DXErrorHandling.c to manually
        aggregate the new subsystem's error codes into the error
        handling system */
    
    sc_subsystem_count,
    sc_invalid_subsystem = -1
};

/* -------------------------------------------------------------------------- */
/*
 *	Error codes
 
 *  a list of error code rosters belonging to different subsystems
 */
/* -------------------------------------------------------------------------- */
/*
 *	Memory subsystem error codes
 */
/* ---------------------------------- */

enum dx_memory_error_code_t {
    dx_mec_insufficient_memory
};

/* ---------------------------------- */
/*
 *	Socket subsystem error codes
 */
/* ---------------------------------- */

enum dx_socket_error_code_t {
    dx_sec_socket_subsystem_init_failed, /* Win32-specific */
    dx_sec_socket_subsystem_init_required, /* Win32-specific */
    dx_sec_socket_subsystem_incompatible_version, /* Win32-specific */
    dx_sec_connection_gracefully_closed,
    dx_sec_network_is_down,
    dx_sec_blocking_call_in_progress,
    dx_sec_addr_family_not_supported,
    dx_sec_no_sockets_available,
    dx_sec_no_buffer_space_available,
    dx_sec_proto_not_supported,
    dx_sec_socket_type_proto_incompat,
    dx_sec_socket_type_addrfam_incompat,
    dx_sec_addr_already_in_use,
    dx_sec_blocking_call_interrupted,
    dx_sec_nonblocking_oper_pending,
    dx_sec_addr_not_valid,
    dx_sec_connection_refused,
    dx_sec_invalid_ptr_arg,
    dx_sec_invalid_arg,
    dx_sec_sock_already_connected,
    dx_sec_network_is_unreachable,
    dx_sec_sock_oper_on_nonsocket,
    dx_sec_connection_timed_out,
    dx_sec_res_temporarily_unavail,
    dx_sec_permission_denied,
    dx_sec_network_dropped_connection,
    dx_sec_socket_not_connected,
    dx_sec_operation_not_supported,
    dx_sec_socket_shutdown,
    dx_sec_message_too_long,
    dx_sec_no_route_to_host,
    dx_sec_connection_aborted,
    dx_sec_connection_reset,
    dx_sec_persistent_temp_error,
    dx_sec_unrecoverable_error,
    dx_sec_not_enough_memory,
    dx_sec_no_data_on_host,
    dx_sec_host_not_found,

    dx_sec_generic_error
};

/* ---------------------------------- */
/*
 *	Thread subsystrem error codes
 */
/* ---------------------------------- */

enum dx_thread_error_code_t {
    dx_tec_not_enough_sys_resources,
    dx_tec_permission_denied,
    dx_tec_invalid_res_operation,
    dx_tec_invalid_resource_id,
    dx_tec_deadlock_detected,
    dx_tec_not_enough_memory,

    dx_tec_generic_error
};

/* ---------------------------------- */
/*
 *	Network subsystem error codes
 */
/* ---------------------------------- */

enum dx_network_error_code_t {
    dx_nec_invalid_port_value,
    dx_nec_invalid_function_arg,
    dx_nec_conn_not_established    
};

#endif /* DX_ERROR_CODES_H_INCLUDED */