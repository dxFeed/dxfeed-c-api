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
#include "DXErrorCodes.h"
#include "DXThreads.h"
#include "DXAlgorithms.h"

/* -------------------------------------------------------------------------- */
/*
 *	Socket error code functions
 */
/* -------------------------------------------------------------------------- */

#ifdef _WIN32
dx_error_code_t dx_wsa_error_code_to_internal (int wsa_code) {
	switch (wsa_code) {
	case WSANOTINITIALISED:
		return dx_sec_socket_subsystem_init_required;
	case WSAENETDOWN:
		return dx_sec_network_is_down;
	case WSAEINPROGRESS:
		return dx_sec_blocking_call_in_progress;
	case WSAEAFNOSUPPORT:
		return dx_sec_addr_family_not_supported;
	case WSAEMFILE:
		return dx_sec_no_sockets_available;
	case WSAENOBUFS:
		return dx_sec_no_buffer_space_available;
	case WSAEPROTONOSUPPORT:
		return dx_sec_proto_not_supported;
	case WSAEPROTOTYPE:
		return dx_sec_socket_type_proto_incompat;
	case WSAESOCKTNOSUPPORT:
		return dx_sec_socket_type_addrfam_incompat;
	case WSAEADDRINUSE:
		return dx_sec_addr_already_in_use;
	case WSAEINTR:
		return dx_sec_blocking_call_interrupted;
	case WSAEALREADY:
		return dx_sec_nonblocking_oper_pending;
	case WSAEADDRNOTAVAIL:
		return dx_sec_addr_not_valid;
	case WSAECONNREFUSED:
		return dx_sec_connection_refused;
	case WSAEFAULT:
		return dx_sec_invalid_ptr_arg;
	case WSAEINVAL:
		return dx_sec_invalid_arg;
	case WSAEISCONN:
		return dx_sec_sock_already_connected;
	case WSAENETUNREACH:
		return dx_sec_network_is_unreachable;
	case WSAENOTSOCK:
		return dx_sec_sock_oper_on_nonsocket;
	case WSAETIMEDOUT:
		return dx_sec_connection_timed_out;
	case WSAEWOULDBLOCK:
		return dx_sec_res_temporarily_unavail;
	case WSAEACCES:
		return dx_sec_permission_denied;
	case WSAENETRESET:
		return dx_sec_network_dropped_connection;
	case WSAENOTCONN:
		return dx_sec_socket_not_connected;
	case WSAEOPNOTSUPP:
		return dx_sec_operation_not_supported;
	case WSAESHUTDOWN:
		return dx_sec_socket_shutdown;
	case WSAEMSGSIZE:
		return dx_sec_message_too_long;
	case WSAEHOSTUNREACH:
		return dx_sec_no_route_to_host;
	case WSAECONNABORTED:
		return dx_sec_connection_aborted;
	case WSAECONNRESET:
		return dx_sec_connection_reset;
	case WSATRY_AGAIN:
		return dx_sec_persistent_temp_error;
	case WSANO_RECOVERY:
		return dx_sec_unrecoverable_error;
	case WSA_NOT_ENOUGH_MEMORY:
		return dx_sec_not_enough_memory;
	case WSANO_DATA:
		return dx_sec_no_data_on_host;
	case WSAHOST_NOT_FOUND:
		return dx_sec_host_not_found;
	case WSATYPE_NOT_FOUND:
		return dx_sec_socket_type_proto_incompat;
	default:
		return dx_sec_generic_error;
	}
}

#else
dx_error_code_t dx_errno_code_to_internal () {
	switch (errno) {
	case ENETDOWN:
		return dx_sec_network_is_down;
	case EINPROGRESS:
		return dx_sec_blocking_call_in_progress;
	case EAFNOSUPPORT:
		return dx_sec_addr_family_not_supported;
	case EMFILE:
		return dx_sec_no_sockets_available;
	case ENOBUFS:
		return dx_sec_no_buffer_space_available;
	case EPROTONOSUPPORT:
		return dx_sec_proto_not_supported;
	case EPROTOTYPE:
		return dx_sec_socket_type_proto_incompat;
	case ESOCKTNOSUPPORT:
		return dx_sec_socket_type_addrfam_incompat;
	case EADDRINUSE:
		return dx_sec_addr_already_in_use;
	case EINTR:
		return dx_sec_blocking_call_interrupted;
	case EALREADY:
		return dx_sec_nonblocking_oper_pending;
	case EADDRNOTAVAIL:
		return dx_sec_addr_not_valid;
	case ECONNREFUSED:
		return dx_sec_connection_refused;
	case EFAULT:
		return dx_sec_invalid_ptr_arg;
	case EINVAL:
		return dx_sec_invalid_arg;
	case EISCONN:
		return dx_sec_sock_already_connected;
	case ENETUNREACH:
		return dx_sec_network_is_unreachable;
	case ENOTSOCK:
		return dx_sec_sock_oper_on_nonsocket;
	case ETIMEDOUT:
		return dx_sec_connection_timed_out;
	case EWOULDBLOCK:
		return dx_sec_res_temporarily_unavail;
	case EACCES:
		return dx_sec_permission_denied;
	case ENETRESET:
		return dx_sec_network_dropped_connection;
	case ENOTCONN:
		return dx_sec_socket_not_connected;
	case EOPNOTSUPP:
		return dx_sec_operation_not_supported;
	case ESHUTDOWN:
		return dx_sec_socket_shutdown;
	case EMSGSIZE:
		return dx_sec_message_too_long;
	case EHOSTUNREACH:
		return dx_sec_no_route_to_host;
	case ECONNABORTED:
		return dx_sec_connection_aborted;
	case ECONNRESET:
		return dx_sec_connection_reset;
	default:
		return dx_sec_generic_error;
	}
}

dx_error_code_t dx_eai_code_to_internal(int code) {
	switch (code) {
	case EAI_AGAIN:
		return dx_sec_persistent_temp_error;
	case EAI_BADFLAGS:
		return dx_sec_generic_error;
	case EAI_FAIL:
		return dx_sec_generic_error;
	case EAI_FAMILY:
		return dx_sec_addr_family_not_supported;
	case EAI_MEMORY:
		return dx_sec_not_enough_memory;
	case EAI_NONAME:
		return dx_sec_host_not_found;
	case EAI_OVERFLOW:
		return dx_sec_not_enough_memory;
	case EAI_SERVICE:
		return dx_sec_socket_type_proto_incompat;
	case EAI_SOCKTYPE:
		return dx_sec_socket_type_proto_incompat;
	case EAI_SYSTEM:
		return dx_errno_code_to_internal();
	default:
		return dx_sec_generic_error;
	}
}

#endif

/* -------------------------------------------------------------------------- */
/*
 *	Cross-implementation data
 */
/* -------------------------------------------------------------------------- */

static const int g_name_resolution_attempt_count = 5;
static const unsigned g_connect_timeout = 5; /* timeout in seconds */

static int g_connection_count = 0;
static dx_mutex_t g_count_guard;
static bool g_count_guard_initialized = false;

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

bool dx_init_socket_subsystem (void) {
	WORD wVersionRequested;
	WSADATA wsaData;


	wVersionRequested = MAKEWORD(2, 0);

	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		return dx_set_error_code(dx_sec_socket_subsystem_init_failed);
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 0) {
		dx_set_error_code(dx_sec_socket_subsystem_incompatible_version);

		WSACleanup();

		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_deinit_socket_subsystem (void) {
	if (WSACleanup() == SOCKET_ERROR) {
		return dx_set_error_code(dx_wsa_error_code_to_internal(WSAGetLastError()));
	}

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Socket subsystem initialization implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_on_connection_created (void) {
	if (!g_count_guard_initialized) {
		CHECKED_CALL(dx_mutex_create, &g_count_guard);

		g_count_guard_initialized = true;
	}

	CHECKED_CALL(dx_mutex_lock, &g_count_guard);

	if (g_connection_count == 0) {
		if (!dx_init_socket_subsystem()) {
			dx_mutex_unlock(&g_count_guard);

			return false;
		}
	}

	++g_connection_count;

	CHECKED_CALL(dx_mutex_unlock, &g_count_guard);

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_on_connection_destroyed (void) {
	CHECKED_CALL(dx_mutex_lock, &g_count_guard);

	if (g_connection_count > 0 && --g_connection_count == 0) {
		if (!dx_deinit_socket_subsystem()) {
			dx_mutex_unlock(&g_count_guard);

			return false;
		}
	}

	CHECKED_CALL(dx_mutex_unlock, &g_count_guard);

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	DX socket API implementation
 */
/* -------------------------------------------------------------------------- */

dx_socket_t dx_socket (int family, int type, int protocol) {
	dx_socket_t s = INVALID_SOCKET;

	if ((s = socket(family, type, protocol)) == INVALID_SOCKET) {
		dx_set_error_code(dx_wsa_error_code_to_internal(WSAGetLastError()));
	}

	return s;
}

/* -------------------------------------------------------------------------- */

bool dx_connect (dx_socket_t s, const struct sockaddr* addr, socklen_t addrlen) {
	if (connect(s, addr, addrlen) == SOCKET_ERROR) {
		return dx_set_error_code(dx_wsa_error_code_to_internal(WSAGetLastError()));
	}

	return true;
}

/* -------------------------------------------------------------------------- */

int dx_send (dx_socket_t s, const void* buffer, int buflen) {
	int res = send(s, (const char*)buffer, buflen, 0);

	if (res == SOCKET_ERROR) {
		dx_set_error_code(dx_wsa_error_code_to_internal(WSAGetLastError()));

		return INVALID_DATA_SIZE;
	}

	return res;
}

/* -------------------------------------------------------------------------- */

int dx_recv (dx_socket_t s, void* buffer, int buflen) {
	int res = recv(s, (char*)buffer, buflen, 0);

	switch (res) {
	case 0:
		dx_set_error_code(dx_sec_connection_gracefully_closed);

		break;
	case SOCKET_ERROR:
		dx_set_error_code(dx_wsa_error_code_to_internal(WSAGetLastError()));

		break;
	default:
		return res;
	}

	return INVALID_DATA_SIZE;
}

/* -------------------------------------------------------------------------- */

bool dx_close (dx_socket_t s) {
	if (shutdown(s, SD_BOTH) == INVALID_SOCKET) {
		return dx_set_error_code(dx_wsa_error_code_to_internal(WSAGetLastError()));
	}

	if (closesocket(s) == INVALID_SOCKET) {
		return dx_set_error_code(dx_wsa_error_code_to_internal(WSAGetLastError()));
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_getaddrinfo (const char* nodename, const char* servname,
					const struct addrinfo* hints, struct addrinfo** res) {
	int funres = 0;
	int iter_count = 0;

	for (; iter_count < g_name_resolution_attempt_count; ++iter_count) {
		funres = getaddrinfo(nodename, servname, hints, res);

		if (funres == WSATRY_AGAIN) {
			continue;
		}

		break;
	}

	if (funres != 0) {
		return dx_set_error_code(dx_wsa_error_code_to_internal(funres));
	}

	return true;
}

/* -------------------------------------------------------------------------- */

void dx_freeaddrinfo (struct addrinfo* res) {
	freeaddrinfo(res);
}

#else

/* ---------------------------------- */
/*
 *	Auxiliary stuff
 */
/* ---------------------------------- */

bool dx_init_socket_subsystem (void) {
	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_deinit_socket_subsystem (void) {
	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Socket subsystem initialization implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_on_connection_created (void) {
	if (!g_count_guard_initialized) {
		CHECKED_CALL(dx_mutex_create, &g_count_guard);

		g_count_guard_initialized = true;
	}

	CHECKED_CALL(dx_mutex_lock, &g_count_guard);

	if (g_connection_count == 0) {
		if (!dx_init_socket_subsystem()) {
			dx_mutex_unlock(&g_count_guard);

			return false;
		}
	}

	++g_connection_count;

	CHECKED_CALL(dx_mutex_unlock, &g_count_guard);

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_on_connection_destroyed (void) {
	CHECKED_CALL(dx_mutex_lock, &g_count_guard);

	if (g_connection_count > 0 && --g_connection_count == 0) {
		if (!dx_deinit_socket_subsystem()) {
			dx_mutex_unlock(&g_count_guard);

			return false;
		}
	}

	CHECKED_CALL(dx_mutex_unlock, &g_count_guard);

	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	DX socket API implementation
 */
/* -------------------------------------------------------------------------- */

dx_socket_t dx_socket (int family, int type, int protocol) {
	dx_socket_t s = INVALID_SOCKET;

	if ((s = socket(family, type, protocol)) == INVALID_SOCKET) {
		dx_set_error_code(dx_errno_code_to_internal());
	}

	return s;
}

/* -------------------------------------------------------------------------- */

bool dx_connect (dx_socket_t s, const struct sockaddr* addr, socklen_t addrlen) {
	if (connect(s, addr, addrlen) == SOCKET_ERROR) {
		return dx_set_error_code(dx_errno_code_to_internal());
	}

	return true;
}

/* -------------------------------------------------------------------------- */

int dx_send (dx_socket_t s, const void* buffer, int buflen) {
	int res = send(s, (const char*)buffer, buflen, 0);

	if (res == SOCKET_ERROR) {
		dx_set_error_code(dx_errno_code_to_internal());

		return INVALID_DATA_SIZE;
	}

	return res;
}

/* -------------------------------------------------------------------------- */

int dx_recv (dx_socket_t s, void* buffer, int buflen) {
	int res = recv(s, (char*)buffer, buflen, 0);

	switch (res) {
	case 0:
		dx_set_error_code(dx_sec_connection_gracefully_closed);

		break;
	case SOCKET_ERROR:
		dx_set_error_code(dx_errno_code_to_internal());

		break;
	default:
		return res;
	}

	return INVALID_DATA_SIZE;
}

/* -------------------------------------------------------------------------- */

bool dx_close (dx_socket_t s) {
	if (shutdown(s, SHUT_RDWR) == INVALID_SOCKET) {
		return dx_set_error_code(dx_errno_code_to_internal());
	}

	if (close(s) == INVALID_SOCKET) {
		return dx_set_error_code(dx_errno_code_to_internal());
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_getaddrinfo (const char* nodename, const char* servname,
					const struct addrinfo* hints, struct addrinfo** res) {
	int funres = 0;
	int iter_count = 0;

	for (; iter_count < g_name_resolution_attempt_count; ++iter_count) {
		funres = getaddrinfo(nodename, servname, hints, res);

		if (funres == EAI_AGAIN) {
			continue;
		}

		break;
	}

	if (funres != 0) {
		return dx_set_error_code(dx_eai_code_to_internal(funres));
	}

	return true;
}

/* -------------------------------------------------------------------------- */

void dx_freeaddrinfo (struct addrinfo* res) {
	freeaddrinfo(res);
}

#endif // _WIN32