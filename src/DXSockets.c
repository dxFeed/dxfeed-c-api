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

#include "DXSockets.h"
#include "DXErrorHandling.h"
#include "DXSubsystemRoster.h"

/* -------------------------------------------------------------------------- */
/*
 *	Socket error codes
 */
/* -------------------------------------------------------------------------- */

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

static struct dx_error_code_descr_t g_socket_errors[] = {
    { sec_socket_subsystem_init_failed, "Socket subsystem initialization failed" },
    { sec_socket_subsystem_init_required, "Internal software error" },
    { sec_socket_subsystem_incompatible_version, "Incompatible version of socket subsystem" },
    { sec_connection_gracefully_closed, "Connection gracefully closed by peer" },
    { sec_network_is_down, "Network is down" },
    { sec_blocking_call_in_progress, "Internal software error" },
    { sec_addr_family_not_supported, "Internal software error" },
    { sec_no_sockets_available, "No sockets available" },
    { sec_no_buffer_space_available, "Not enough space in socket buffers" },
    { sec_proto_not_supported, "Internal software error" },
    { sec_socket_type_proto_incompat, "Internal software error" },
    { sec_socket_type_addrfam_incompat, "Internal software error" },
    { sec_addr_already_in_use, "Internal software error" },
    { sec_blocking_call_interrupted, "Internal software error" },
    { sec_nonblocking_oper_pending, "Internal software error" },
    { sec_addr_not_valid, "Internal software error" },
    { sec_connection_refused, "Connection refused" },
    { sec_invalid_ptr_arg, "Internal software error" },
    { sec_invalid_arg, "Internal software error" },
    { sec_sock_already_connected, "Internal software error" },
    { sec_network_is_unreachable, "Network is unreachable" },
    { sec_sock_oper_on_nonsocket, "Internal software error" },
    { sec_connection_timed_out, "Connection timed out" },
    { sec_res_temporarily_unavail, "Internal software error" },
    { sec_permission_denied, "Permission denied" },
    { sec_network_dropped_connection, "Network dropped connection on reset" },
    { sec_socket_not_connected, "Internal software error" },
    { sec_operation_not_supported, "Internal software error" },
    { sec_socket_shutdown, "Internal software error" },
    { sec_message_too_long, "Internal software error" },
    { sec_no_route_to_host, "No route to host" },
    { sec_connection_aborted, "Connection aborted" },
    { sec_connection_reset, "Connection reset" },
    { sec_persistent_temp_error, "Temporary failure in name resolution persists for too long" },
    { sec_unrecoverable_error, "Internal software error" },
    { sec_not_enough_memory, "Not enough memory to complete a socket operation" },
    { sec_no_data_on_host, "Internal software error" },
    { sec_host_not_found, "Host not found" },

    { sec_generic_error, "Unrecognized socket error" },
    
    { ERROR_CODE_FOOTER, ERROR_DESCR_FOOTER }
};

const struct dx_error_code_descr_t* socket_error_roster = g_socket_errors;

/* -------------------------------------------------------------------------- */

enum dx_socket_error_code_t dx_wsa_error_code_to_internal (int wsa_code) {
    switch (wsa_code) {
    case WSANOTINITIALISED:
        return sec_socket_subsystem_init_required;
    case WSAENETDOWN:
        return sec_network_is_down;
    case WSAEINPROGRESS:
        return sec_blocking_call_in_progress;
    case WSAEAFNOSUPPORT:
        return sec_addr_family_not_supported;
    case WSAEMFILE:
        return sec_no_sockets_available;
    case WSAENOBUFS:
        return sec_no_buffer_space_available;
    case WSAEPROTONOSUPPORT:
        return sec_proto_not_supported;
    case WSAEPROTOTYPE:
        return sec_socket_type_proto_incompat;
    case WSAESOCKTNOSUPPORT:
        return sec_socket_type_addrfam_incompat;
    case WSAEADDRINUSE:
        return sec_addr_already_in_use;
    case WSAEINTR:
        return sec_blocking_call_interrupted;
    case WSAEALREADY:
        return sec_nonblocking_oper_pending;
    case WSAEADDRNOTAVAIL:
        return sec_addr_not_valid;
    case WSAECONNREFUSED:
        return sec_connection_refused;
    case WSAEFAULT:
        return sec_invalid_ptr_arg;
    case WSAEINVAL:
        return sec_invalid_arg;
    case WSAEISCONN:
        return sec_sock_already_connected;
    case WSAENETUNREACH:
        return sec_network_is_unreachable;
    case WSAENOTSOCK:
        return sec_sock_oper_on_nonsocket;
    case WSAETIMEDOUT:
        return sec_connection_timed_out;
    case WSAEWOULDBLOCK:
        return sec_res_temporarily_unavail;
    case WSAEACCES:
        return sec_permission_denied;
    case WSAENETRESET:
        return sec_network_dropped_connection;
    case WSAENOTCONN:
        return sec_socket_not_connected;
    case WSAEOPNOTSUPP:
        return sec_operation_not_supported;
    case WSAESHUTDOWN:
        return sec_socket_shutdown;
    case WSAEMSGSIZE:
        return sec_message_too_long;
    case WSAEHOSTUNREACH:
        return sec_no_route_to_host;
    case WSAECONNABORTED:
        return sec_connection_aborted;
    case WSAECONNRESET:
        return sec_connection_reset;
    case WSATRY_AGAIN:
        return sec_persistent_temp_error;
    case WSANO_RECOVERY:
        return sec_unrecoverable_error;
    case WSA_NOT_ENOUGH_MEMORY:
        return sec_not_enough_memory;
    case WSANO_DATA:
        return sec_no_data_on_host;
    case WSAHOST_NOT_FOUND:
        return sec_host_not_found;
    case WSATYPE_NOT_FOUND:
        return sec_socket_type_proto_incompat;
    default:
        return sec_generic_error;    
    }
}

/* -------------------------------------------------------------------------- */
/*
 *	Socket function wrappers
 
 *  Win32 implementation
 */
/* -------------------------------------------------------------------------- */

#ifdef _WIN32

/* ---------------------------------- */
/*
 *	Auxiliary stuff
 */
/* ---------------------------------- */

static bool g_sock_subsystem_initialized = false;

bool dx_init_socket_subsystem () {
    WORD wVersionRequested;
    WSADATA wsaData;
        
    if (g_sock_subsystem_initialized) {
        return true;
    }

    wVersionRequested = MAKEWORD(2, 0);

    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
        dx_set_last_error(sc_sockets, sec_socket_subsystem_init_failed);
        
        return false;
    }
    
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 0) {
        dx_set_last_error(sc_sockets, sec_socket_subsystem_incompatible_version);
        
        WSACleanup();
        
        return false;
    }

    return (g_sock_subsystem_initialized = true);
}

/* -------------------------------------------------------------------------- */

bool dx_deinit_socket_subsystem () {
    if (WSACleanup() == SOCKET_ERROR) {
        dx_set_last_error(sc_sockets, (int)dx_wsa_error_code_to_internal(WSAGetLastError()));
        
        return false;
    }
    
    g_sock_subsystem_initialized = false;
    
    return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	DX socket API implementation
 */
/* -------------------------------------------------------------------------- */

dx_socket_t dx_socket (int family, int type, int protocol) {
    dx_socket_t res = INVALID_SOCKET;
    
    if (!dx_init_socket_subsystem()) {
        return res;
    }
    
    res = socket(family, type, protocol);
    
    if (res == INVALID_SOCKET) {
        dx_set_last_error(sc_sockets, (int)dx_wsa_error_code_to_internal(WSAGetLastError()));
    }
    
    return res;
}

/* -------------------------------------------------------------------------- */

bool dx_connect (dx_socket_t s, const struct sockaddr* addr, socklen_t addrlen) {
    if (connect(s, addr, addrlen) != 0) {
        dx_set_last_error(sc_sockets, (int)dx_wsa_error_code_to_internal(WSAGetLastError()));
        
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

unsigned dx_send (dx_socket_t s, const void* buffer, int buflen) {
    int res = send(s, (const char*)buffer, buflen, 0);
    
    if (res == SOCKET_ERROR) {
        dx_set_last_error(sc_sockets, (int)dx_wsa_error_code_to_internal(WSAGetLastError()));
        
        return 0;
    }
    
    return (unsigned)res;
}

/* -------------------------------------------------------------------------- */

unsigned dx_recv (dx_socket_t s, void* buffer, int buflen) {
    int res = recv(s, (char*)buffer, buflen, 0);
    
    switch (res) {
    case 0:
        dx_set_last_error(sc_sockets, sec_connection_gracefully_closed);
        break;
    case SOCKET_ERROR:
        dx_set_last_error(sc_sockets, (int)dx_wsa_error_code_to_internal(WSAGetLastError()));
        res = 0;
        break;
    }
    
    return res;
}

/* -------------------------------------------------------------------------- */

bool dx_close (dx_socket_t s) {
    if (closesocket(s) == INVALID_SOCKET) {
        dx_set_last_error(sc_sockets, (int)dx_wsa_error_code_to_internal(WSAGetLastError()));
        
        return false;
    }
    
    if (!dx_deinit_socket_subsystem()) {
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

const size_t dx_name_resolution_attempt_count = 5;

bool dx_getaddrinfo (const char* nodename, const char* servname,
                     const struct addrinfo* hints, struct addrinfo** res) {
    int funres = 0;
    size_t iter_count = 0;
    
    if (!dx_init_socket_subsystem()) {
        return false;
    }
    
    for (; iter_count < dx_name_resolution_attempt_count; ++iter_count) {
        funres = getaddrinfo(nodename, servname, hints, res);
        
        if (funres == WSATRY_AGAIN) {
            continue;
        }
    }
    
    if (funres != 0) {
        dx_set_last_error(sc_sockets, (int)dx_wsa_error_code_to_internal(funres));
        
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

void dx_freeaddrinfo (struct addrinfo* res) {
    freeaddrinfo(res);
}

#endif // _WIN32