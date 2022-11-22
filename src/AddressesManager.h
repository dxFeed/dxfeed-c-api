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

#pragma once

#include "DXSockets.h"
#include "DXTypes.h"
#include "EventData.h"
#include "PrimitiveTypes.h"

#ifdef __cplusplus
extern "C" {
#	include <cstdint>
#else
#	include <stdint.h>
#endif

int dx_am_next_socket_address(dxf_connection_t connection);
int dx_am_is_current_address_tls_enabled(dxf_connection_t connection);
int dx_am_is_current_socket_address_connection_failed(dxf_connection_t connection);
void dx_am_set_current_socket_address_connection_failed(dxf_connection_t connection, int is_connection_failed);
const char* dx_am_get_current_address_username(dxf_connection_t connection);
const char* dx_am_get_current_address_password(dxf_connection_t connection);
const char* dx_am_get_current_address_host(dxf_connection_t connection);
const char* dx_am_get_current_address_port(dxf_connection_t connection);
int dx_ma_get_current_socket_address_info(dxf_connection_t connection, int* address_family, int* address_socket_type,
										  int* address_protocol, struct sockaddr* native_socket_address,
										  size_t* native_socket_address_length);
const char* dx_am_get_current_address_tls_trust_store(dxf_connection_t connection);
const char* dx_am_get_current_address_tls_trust_store_password(dxf_connection_t connection);
const char* dx_am_get_current_address_tls_key_store(dxf_connection_t connection);
const char* dx_am_get_current_address_tls_key_store_password(dxf_connection_t connection);
void dx_am_set_current_address_tls_trust_store_mem(dxf_connection_t connection, uint8_t* trust_store_mem);
void dx_am_set_current_address_tls_trust_store_len(dxf_connection_t connection, size_t trust_store_len);
void dx_am_set_current_address_tls_key_store_mem(dxf_connection_t connection, uint8_t* key_store_mem);
void dx_am_set_current_address_tls_key_store_len(dxf_connection_t connection, size_t key_store_len);
const char* dx_am_get_current_connected_address(dxf_connection_t connection);
const char* dx_am_get_current_connected_socket_address(dxf_connection_t connection);

#ifdef __cplusplus
}
#endif