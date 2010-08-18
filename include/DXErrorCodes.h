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
    mec_insufficient_memory
};

/* ---------------------------------- */
/*
 *	Socket subsystem error codes
 */
/* ---------------------------------- */

enum dx_socket_error_code_t {
    sec_socket_subsystem_init_failed, /* Win32-specific */
    sec_socket_subsystem_init_required, /* Win32-specific */
    sec_socket_subsystem_incompatible_version, /* Win32-specific */
    sec_connection_gracefully_closed,
    sec_network_is_down,
    sec_blocking_call_in_progress,
    sec_addr_family_not_supported,
    sec_no_sockets_available,
    sec_no_buffer_space_available,
    sec_proto_not_supported,
    sec_socket_type_proto_incompat,
    sec_socket_type_addrfam_incompat,
    sec_addr_already_in_use,
    sec_blocking_call_interrupted,
    sec_nonblocking_oper_pending,
    sec_addr_not_valid,
    sec_connection_refused,
    sec_invalid_ptr_arg,
    sec_invalid_arg,
    sec_sock_already_connected,
    sec_network_is_unreachable,
    sec_sock_oper_on_nonsocket,
    sec_connection_timed_out,
    sec_res_temporarily_unavail,
    sec_permission_denied,
    sec_network_dropped_connection,
    sec_socket_not_connected,
    sec_operation_not_supported,
    sec_socket_shutdown,
    sec_message_too_long,
    sec_no_route_to_host,
    sec_connection_aborted,
    sec_connection_reset,
    sec_persistent_temp_error,
    sec_unrecoverable_error,
    sec_not_enough_memory,
    sec_no_data_on_host,
    sec_host_not_found,

    sec_generic_error
};

/* ---------------------------------- */
/*
 *	Thread subsystrem error codes
 */
/* ---------------------------------- */

enum dx_thread_error_code_t {
    tec_not_enough_sys_resources,
    tec_permission_denied,
    tec_invalid_res_operation,
    tec_invalid_resource_id,
    tec_deadlock_detected,
    tec_not_enough_memory,

    tec_generic_error
};

/* ---------------------------------- */
/*
 *	Network subsystem error codes
 */
/* ---------------------------------- */

enum dx_network_error_code_t {
    nec_invalid_port_value,
    nec_invalid_function_arg,
    nec_conn_not_established    
};

#endif /* DX_ERROR_CODES_H_INCLUDED */