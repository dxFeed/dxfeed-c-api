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

extern "C" {
#include "AddressesManager.h"
}

#include "AddressesManager.hpp"

extern "C" {

int dx_am_next_socket_address(dxf_connection_t connection) {
	return dx::AddressesManager::getInstance()->nextSocketAddress(connection);
}

int dx_am_is_current_address_tls_enabled(dxf_connection_t connection) {
	return dx::AddressesManager::getInstance()->isCurrentAddressTlsEnabled(connection);
}

int dx_am_is_current_socket_address_connection_failed(dxf_connection_t connection) {
	return dx::AddressesManager::getInstance()->isCurrentSocketAddressConnectionFailed(connection);
}

void dx_am_set_current_socket_address_connection_failed(dxf_connection_t connection, int is_connection_failed) {
	return dx::AddressesManager::getInstance()->setCurrentSocketAddressConnectionFailed(connection,
																						is_connection_failed != 0);
}

const char* dx_am_get_current_address_username(dxf_connection_t connection) {
	thread_local std::string currentAddressUsername{};

	currentAddressUsername = dx::AddressesManager::getInstance()->getCurrentAddressUsername(connection);

	return currentAddressUsername.c_str();
}

const char* dx_am_get_current_address_password(dxf_connection_t connection) {
	thread_local std::string currentAddressPassword{};

	currentAddressPassword = dx::AddressesManager::getInstance()->getCurrentAddressPassword(connection);

	return currentAddressPassword.c_str();
}

const char* dx_am_get_current_address_host(dxf_connection_t connection) {
	thread_local std::string currentAddressHost{};

	currentAddressHost = dx::AddressesManager::getInstance()->getCurrentAddressHost(connection);

	return currentAddressHost.c_str();
}

const char* dx_am_get_current_address_port(dxf_connection_t connection) {
	thread_local std::string currentAddressPort{};

	currentAddressPort = dx::AddressesManager::getInstance()->getCurrentAddressPort(connection);

	return currentAddressPort.c_str();
}

int dx_ma_get_current_socket_address_info(dxf_connection_t connection, int* address_family, int* address_socket_type,
										  int* address_protocol, struct sockaddr* native_socket_address,
										  size_t* native_socket_address_length) {
	return dx::AddressesManager::getInstance()->getCurrentSocketAddressInfo(
		connection, address_family, address_socket_type, address_protocol, native_socket_address,
		native_socket_address_length);
}

const char* dx_am_get_current_address_tls_trust_store(dxf_connection_t connection) {
	thread_local std::string currentAddressTlsTrustStore{};

	return currentAddressTlsTrustStore.c_str();
}

const char* dx_am_get_current_address_tls_trust_store_password(dxf_connection_t connection) {
	thread_local std::string currentAddressTlsTrustStorePassword{};

	return currentAddressTlsTrustStorePassword.c_str();
}

const char* dx_am_get_current_address_tls_key_store(dxf_connection_t connection) {
	thread_local std::string currentAddressTlsKeyStore{};

	return currentAddressTlsKeyStore.c_str();
}

const char* dx_am_get_current_address_tls_key_store_password(dxf_connection_t connection) {
	thread_local std::string currentAddressTlsKeyStorePassword{};

	return currentAddressTlsKeyStorePassword.c_str();
}

uint8_t* dx_am_get_current_address_tls_trust_store_mem(dxf_connection_t connection) { return nullptr; }

void dx_am_set_current_address_tls_trust_store_mem(dxf_connection_t connection, uint8_t* trust_store_mem) {}

size_t dx_am_get_current_address_tls_trust_store_len(dxf_connection_t connection) { return 0; }

void dx_am_set_current_address_tls_trust_store_len(dxf_connection_t connection, size_t trust_store_len) {}

uint8_t* dx_am_get_current_address_tls_key_store_mem(dxf_connection_t connection) { return nullptr; }

void dx_am_set_current_address_tls_key_store_mem(dxf_connection_t connection, uint8_t* key_store_mem) {}

size_t dx_am_get_current_address_tls_key_store_len(dxf_connection_t connection) { return 0; }

void dx_am_set_current_address_tls_key_store_len(dxf_connection_t connection, size_t key_store_len) {}

const char* dx_am_get_current_connected_address(dxf_connection_t connection) {
	thread_local std::string currentConnectedAddress{};

	currentConnectedAddress = dx::AddressesManager::getInstance()->getCurrentAddressHost(connection) + ":" +
		dx::AddressesManager::getInstance()->getCurrentAddressPort(connection);

	return currentConnectedAddress.c_str();
}
}