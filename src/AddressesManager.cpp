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

namespace dx {

const ConnectOrder ConnectOrder::SHUFFLE("shuffle", true, false);
const ConnectOrder ConnectOrder::RANDOM("random", true, true);
const ConnectOrder ConnectOrder::ORDERED("ordered", false, false);
const ConnectOrder ConnectOrder::PRIORITY("priority", false, true);

const std::unordered_map<std::string, ConnectOrder> ConnectOrder::VALUES_{
	{SHUFFLE.getName(), SHUFFLE},
	{RANDOM.getName(), RANDOM},
	{ORDERED.getName(), ORDERED},
	{PRIORITY.getName(), PRIORITY},
};

const ConnectOrder ResolvedAddresses::DEFAULT_CONNECT_ORDER{ConnectOrder::SHUFFLE};

}  // namespace dx

extern "C" {

int dx_am_next_socket_address(dxf_connection_t connection) {
	return dx::AddressesManager::getInstance()->nextSocketAddress(connection);
}

void dx_am_clear_addresses(dxf_connection_t connection) {
	return dx::AddressesManager::getInstance()->clearAddresses(connection);
}

void dx_am_reset(dxf_connection_t connection) {
	return dx::AddressesManager::getInstance()->reset(connection);
}

int dx_am_is_reset_on_connect(dxf_connection_t connection) {
	return dx::AddressesManager::getInstance()->isResetOnConnect(connection);
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
};

int dx_ma_get_current_socket_address_info(dxf_connection_t connection, int* address_family, int* address_socket_type,
										  int* address_protocol, struct sockaddr* native_socket_address,
										  size_t* native_socket_address_length) {
	return dx::AddressesManager::getInstance()->getCurrentSocketAddressInfo(
		connection, address_family, address_socket_type, address_protocol, native_socket_address,
		native_socket_address_length);
}

const char* dx_am_get_current_address_tls_trust_store(dxf_connection_t connection) {
	thread_local std::string currentAddressTlsTrustStore{};

	currentAddressTlsTrustStore = dx::AddressesManager::getInstance()->getCurrentAddressTlsTrustStore(connection);

	return currentAddressTlsTrustStore.c_str();
}

const char* dx_am_get_current_address_tls_trust_store_password(dxf_connection_t connection) {
	thread_local std::string currentAddressTlsTrustStorePassword{};

	currentAddressTlsTrustStorePassword =
		dx::AddressesManager::getInstance()->getCurrentAddressTlsTrustStorePassword(connection);

	return currentAddressTlsTrustStorePassword.c_str();
}

const char* dx_am_get_current_address_tls_key_store(dxf_connection_t connection) {
	thread_local std::string currentAddressTlsKeyStore{};

	currentAddressTlsKeyStore = dx::AddressesManager::getInstance()->getCurrentAddressTlsKeyStore(connection);

	return currentAddressTlsKeyStore.c_str();
}

const char* dx_am_get_current_address_tls_key_store_password(dxf_connection_t connection) {
	thread_local std::string currentAddressTlsKeyStorePassword{};

	currentAddressTlsKeyStorePassword =
		dx::AddressesManager::getInstance()->getCurrentAddressTlsKeyStorePassword(connection);

	return currentAddressTlsKeyStorePassword.c_str();
}

#ifdef DXFEED_CODEC_TLS_ENABLED
void dx_am_set_current_address_tls_trust_store_mem(dxf_connection_t connection, uint8_t* trust_store_mem) {
	dx::AddressesManager::getInstance()->setCurrentAddressTlsTrustStoreMem(connection, trust_store_mem);
}

void dx_am_set_current_address_tls_trust_store_len(dxf_connection_t connection, size_t trust_store_len) {
	dx::AddressesManager::getInstance()->setCurrentAddressTlsTrustStoreLen(connection, trust_store_len);
}

void dx_am_set_current_address_tls_key_store_mem(dxf_connection_t connection, uint8_t* key_store_mem) {
	dx::AddressesManager::getInstance()->setCurrentAddressTlsKeyStoreMem(connection, key_store_mem);
}

void dx_am_set_current_address_tls_key_store_len(dxf_connection_t connection, size_t key_store_len) {
	dx::AddressesManager::getInstance()->setCurrentAddressTlsKeyStoreLen(connection, key_store_len);
}
#endif

const char* dx_am_get_current_connected_address(dxf_connection_t connection) {
	thread_local std::string currentConnectedAddress{};

	currentConnectedAddress = dx::AddressesManager::getInstance()->getCurrentAddressHost(connection) + ":" +
		dx::AddressesManager::getInstance()->getCurrentAddressPort(connection);

	return currentConnectedAddress.c_str();
}

const char* dx_am_get_current_connected_socket_address(dxf_connection_t connection) {
	thread_local std::string currentConnectedSocketAddress{};

	currentConnectedSocketAddress = dx::AddressesManager::getInstance()->getCurrentSocketIpAddress(connection) + ":" +
		dx::AddressesManager::getInstance()->getCurrentAddressPort(connection);

	return currentConnectedSocketAddress.c_str();
}
}