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
 *	Contains the socket API that must be used across the project.
 *  The API is based on the Berkeley socket API, but is made a separate entity
 *  since not all the target systems support it fully and in genuine form
 */

#ifndef DX_SOCKETS_H_INCLUDED
#define DX_SOCKETS_H_INCLUDED

#ifdef _WIN32
	#include <WinSock2.h>
	#include <WS2tcpip.h>

	typedef SOCKET dx_socket_t;
#else
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netdb.h>
	#include <errno.h>

	#define INVALID_SOCKET -1
	#define SOCKET_ERROR -1
	typedef int dx_socket_t;
#endif /* _WIN32 */

#include "PrimitiveTypes.h"

#define INVALID_DATA_SIZE (-1)

/* -------------------------------------------------------------------------- */
/*
 *	Socket subsystem initialization
 */
/* -------------------------------------------------------------------------- */

bool dx_on_connection_created (void);
bool dx_on_connection_destroyed (void);

/* -------------------------------------------------------------------------- */
/*
 *	Socket function wrappers
 */
/* -------------------------------------------------------------------------- */

dx_socket_t dx_socket (int family, int type, int protocol);
bool dx_connect (dx_socket_t s, const struct sockaddr* addr, socklen_t addrlen);
int dx_send (dx_socket_t s, const void* buffer, int buflen);
int dx_recv (dx_socket_t s, void* buffer, int buflen);
bool dx_close (dx_socket_t s);
bool dx_getaddrinfo (const char* nodename, const char* servname,
					const struct addrinfo* hints, struct addrinfo** res);
void dx_freeaddrinfo (struct addrinfo* res);

#endif /* DX_SOCKETS_H_INCLUDED */