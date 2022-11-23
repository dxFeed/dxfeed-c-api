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

extern "C" {

#include "DXAddressParser.h"
#include "DXFeed.h"
#include "DXNetwork.h"
#include "DXSockets.h"
#include "DXThreads.h"
#include "EventData.h"
}

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace dx {

namespace StringUtils {
static inline std::string ipToString(const sockaddr* sa) {
	const std::size_t size = 256;
	char buf[size] = {};

	switch (sa->sa_family) {
		case AF_INET:
			inet_ntop(AF_INET, &(((struct sockaddr_in*)sa)->sin_addr), buf, size - 1);
			break;

		case AF_INET6:
			inet_ntop(AF_INET6, &(((struct sockaddr_in6*)sa)->sin6_addr), buf, size - 1);
			break;

		default:
			return "";
	}

	return buf;
}

static inline std::string toString(const char* cString) {
	if (cString == nullptr) {
		return "";
	}

	return cString;
}
}  // namespace StringUtils

namespace System {
inline static std::int64_t currentTimeMillis() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
		.count();
}
}  // namespace System

namespace Math {
inline static double random() {
	static std::random_device randomDevice{};
	static std::mt19937 engine{randomDevice()};
	static std::uniform_real_distribution<> distribution{0, 1};

	return distribution(engine);
}
};	// namespace Math

namespace Thread {
inline static void sleep(std::int64_t millis) { std::this_thread::sleep_for(std::chrono::milliseconds{millis}); }
}  // namespace Thread

/// Not thread-safe
class ReconnectHelper {
	std::int64_t delay_;
	std::int64_t startTime_{};

public:
	explicit ReconnectHelper(std::int64_t delay = 10000) : delay_{delay} {}

	void sleepBeforeConnection() {
		std::int64_t worked = System::currentTimeMillis() - startTime_;
		std::int64_t sleepTime = worked >= delay_
			? 0
			: static_cast<std::int64_t>(static_cast<double>(delay_ - worked) * (1.0 + Math::random()));

		if (sleepTime > 0) {
			Thread::sleep(sleepTime);
		}

		startTime_ = System::currentTimeMillis();
	}

	void reset() { startTime_ = 0; }
};

struct Address {
	struct TlsData {
		bool enabled;
		std::string keyStore;
		std::string keyStorePassword;
		std::string trustStore;
		std::string trustStorePassword;
	};

	struct GzipData {
		bool enabled;
	};

	std::string host;
	std::string port;
	std::string username;
	std::string password;
	TlsData tlsData;
	GzipData gzipData;

#ifdef DXFEED_CODEC_TLS_ENABLED
	uint8_t* keyStoreMem;
	size_t keyStoreLen;
	uint8_t* trustStoreMem;
	size_t trustStoreLen;
#endif	// DXFEED_CODEC_TLS_ENABLED
};

struct SocketAddress {
	const static SocketAddress INVALID;

	int addressFamily;
	int addressSocketType;
	int addressProtocol;
	sockaddr nativeSocketAddress;
	std::size_t nativeSocketAddressLength;
	std::string resolvedIp;
	std::size_t addressIdx;
	bool isConnectionFailed;

	template <typename F>
	static std::vector<SocketAddress> getAllInetAddresses(const std::string& host, const std::string& port,
														  F&& addressIndexProvider) {
		addrinfo hints = {};

		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		addrinfo* addr = nullptr;

		if (!dx_getaddrinfo(host.c_str(), port.c_str(), &hints, &addr)) {
			return {};
		}

		std::vector<SocketAddress> result;

		for (; addr != nullptr; addr = addr->ai_next) {
			if (addr->ai_family != AF_INET && addr->ai_family != AF_INET6) {
				continue;
			}

			result.push_back(SocketAddress{addr->ai_family, addr->ai_socktype, addr->ai_protocol, *(addr->ai_addr),
										   addr->ai_addrlen, StringUtils::ipToString(addr->ai_addr),
										   addressIndexProvider(), false});
		}

		dx_freeaddrinfo(addr);

		return result;
	}

	static bool isInvalid(const SocketAddress& sa) {
		return sa.addressFamily == INVALID.addressFamily && sa.addressSocketType == INVALID.addressSocketType &&
			sa.addressProtocol == INVALID.addressProtocol;
	}
};

const SocketAddress SocketAddress::INVALID{-1, -1, -1};

struct ResolvedAddresses {
	std::vector<Address> addresses;
	std::vector<SocketAddress> socketAddresses;
	std::size_t currentSocketAddress;
};

// singleton
class AddressesManager {
	// connection -> addresses
	std::unordered_map<dxf_connection_t, std::shared_ptr<ResolvedAddresses>> resolvedAddressesByConnection_;
	std::mutex mutex_;

	AddressesManager() : resolvedAddressesByConnection_{}, mutex_{} {}

	static void sleepBeforeResolve() {
		thread_local ReconnectHelper reconnectHelper{};

		reconnectHelper.sleepBeforeConnection();
	}

	template <typename It>
	static void shuffle(It begin, It end) {
		static std::random_device randomDevice{};
		static std::mt19937 engine{randomDevice()};

		std::shuffle(begin, end, engine);
	}

	static ResolvedAddresses resolveAddresses(dxf_connection_t connection) {
		auto address = dx_get_connection_address_string(connection);

		if (address == nullptr) {
			return {};
		}

		dx_address_array_t addressArray;

		if (!dx_get_addresses_from_collection(address, &addressArray) || addressArray.size == 0) {
			return {};
		}

		std::vector<Address> addresses;
		std::vector<SocketAddress> socketAddresses;

		for (std::size_t addressIndex = 0; addressIndex < addressArray.size; addressIndex++) {
			auto host = StringUtils::toString(addressArray.elements[addressIndex].host);
			auto port = StringUtils::toString(addressArray.elements[addressIndex].port);
			auto inetAddresses =
				SocketAddress::getAllInetAddresses(host, port, [&addresses] { return addresses.size(); });

			if (inetAddresses.empty()) {
				continue;
			}

			socketAddresses.insert(socketAddresses.end(), inetAddresses.begin(), inetAddresses.end());

			addresses.push_back(Address{
				host,
				port,
				StringUtils::toString(addressArray.elements[addressIndex].username),
				StringUtils::toString(addressArray.elements[addressIndex].password),
				{addressArray.elements[addressIndex].tls.enabled != 0,
				 StringUtils::toString(addressArray.elements[addressIndex].tls.key_store),
				 StringUtils::toString(addressArray.elements[addressIndex].tls.key_store_password),
				 StringUtils::toString(addressArray.elements[addressIndex].tls.trust_store),
				 StringUtils::toString(addressArray.elements[addressIndex].tls.trust_store_password)},
				{addressArray.elements[addressIndex].gzip.enabled != 0},
#ifdef DXFEED_CODEC_TLS_ENABLED
				nullptr,
				0,
				nullptr,
				0
#endif
			});
		}

		shuffle(socketAddresses.begin(), socketAddresses.end());

		dx_clear_address_array(&addressArray);

		return ResolvedAddresses{addresses, socketAddresses, 0};
	}

	// not thread-safe
	std::shared_ptr<ResolvedAddresses> getOrCreateResolvedAddresses(dxf_connection_t connection) {
		auto found = resolvedAddressesByConnection_.find(connection);

		if (found == resolvedAddressesByConnection_.end()) {
			auto result = resolvedAddressesByConnection_.emplace(connection, std::make_shared<ResolvedAddresses>());

			if (result.second) {
				return result.first->second;
			}

			return nullptr;
		}

		return found->second;
	}

	// Non thread-safe. Checks the connection
	SocketAddress* getCurrentSocketAddressImpl(dxf_connection_t connection) {
		if (connection == nullptr) {
			return nullptr;
		}

		auto resolvedAddresses = getOrCreateResolvedAddresses(connection);

		if (!resolvedAddresses) {
			return nullptr;
		}

		if (resolvedAddresses->socketAddresses.empty()) {
			return nullptr;
		}

		return &resolvedAddresses->socketAddresses[resolvedAddresses->currentSocketAddress];
	}

	// Non thread-safe. Checks the connection
	Address* getCurrentAddressImpl(dxf_connection_t connection) {
		if (connection == nullptr) {
			return nullptr;
		}

		auto resolvedAddresses = getOrCreateResolvedAddresses(connection);

		if (!resolvedAddresses) {
			return nullptr;
		}

		if (resolvedAddresses->socketAddresses.empty()) {
			return nullptr;
		}

		return &resolvedAddresses
					->addresses[resolvedAddresses->socketAddresses[resolvedAddresses->currentSocketAddress].addressIdx];
	}

public:
	static std::shared_ptr<AddressesManager> getInstance() {
		static std::shared_ptr<AddressesManager> instance{new AddressesManager()};

		return instance;
	}

	SocketAddress getNextSocketAddress(dxf_connection_t connection) {
		if (connection == nullptr) {
			return SocketAddress::INVALID;
		}

		{
			std::lock_guard<std::mutex> guard(mutex_);

			auto resolvedAddresses = getOrCreateResolvedAddresses(connection);

			if (!resolvedAddresses) {
				return SocketAddress::INVALID;
			}

			resolvedAddresses->currentSocketAddress++;

			if (resolvedAddresses->currentSocketAddress < resolvedAddresses->socketAddresses.size()) {
				return resolvedAddresses->socketAddresses[resolvedAddresses->currentSocketAddress];
			}
		}

		// don't freeze all threads
		sleepBeforeResolve();

		auto newResolvedAddresses = resolveAddresses(connection);

		{
			std::lock_guard<std::mutex> guard(mutex_);

			*resolvedAddressesByConnection_[connection] = newResolvedAddresses;

			if (newResolvedAddresses.socketAddresses.empty()) {
				return SocketAddress::INVALID;
			}

			return resolvedAddressesByConnection_[connection]->socketAddresses[0];
		}
	}

	bool nextSocketAddress(dxf_connection_t connection) {
		return !SocketAddress::isInvalid(getNextSocketAddress(connection));
	}

	void clearAddresses(dxf_connection_t connection) {
		std::lock_guard<std::mutex> guard(mutex_);

		resolvedAddressesByConnection_.erase(connection);
	}

	bool isCurrentSocketAddressConnectionFailed(dxf_connection_t connection) {
		std::lock_guard<std::mutex> guard(mutex_);
		const auto* sa = getCurrentSocketAddressImpl(connection);

		if (!sa) {
			return true;
		}

		return sa->isConnectionFailed;
	}

	void setCurrentSocketAddressConnectionFailed(dxf_connection_t connection, bool connectionFailed) {
		std::lock_guard<std::mutex> guard(mutex_);
		auto* sa = getCurrentSocketAddressImpl(connection);

		if (!sa) {
			return;
		}

		sa->isConnectionFailed = connectionFailed;
	}

	std::string getCurrentAddressUsername(dxf_connection_t connection) {
		std::lock_guard<std::mutex> guard(mutex_);
		const auto* a = getCurrentAddressImpl(connection);

		if (!a) {
			return "";
		}

		return a->username;
	}

	std::string getCurrentAddressPassword(dxf_connection_t connection) {
		std::lock_guard<std::mutex> guard(mutex_);
		const auto* a = getCurrentAddressImpl(connection);

		if (!a) {
			return "";
		}

		return a->password;
	}

	std::string getCurrentSocketIpAddress(dxf_connection_t connection) {
		std::lock_guard<std::mutex> guard(mutex_);
		const auto* sa = getCurrentSocketAddressImpl(connection);

		if (!sa) {
			return "";
		}

		return sa->resolvedIp;
	}

	std::string getCurrentAddressHost(dxf_connection_t connection) {
		std::lock_guard<std::mutex> guard(mutex_);
		const auto* a = getCurrentAddressImpl(connection);

		if (!a) {
			return "";
		}

		return a->host;
	}

	std::string getCurrentAddressPort(dxf_connection_t connection) {
		std::lock_guard<std::mutex> guard(mutex_);
		const auto* a = getCurrentAddressImpl(connection);

		if (!a) {
			return "";
		}

		return a->port;
	}

	bool isCurrentAddressTlsEnabled(dxf_connection_t connection) {
		std::lock_guard<std::mutex> guard(mutex_);
		const auto* a = getCurrentAddressImpl(connection);

		if (!a) {
			return false;
		}

		return a->tlsData.enabled;
	}

	bool getCurrentSocketAddressInfo(dxf_connection_t connection, int* addressFamily, int* addressSocketType,
									 int* addressProtocol, sockaddr* nativeSocketAddress,
									 std::size_t* nativeSocketAddressLength) {
		std::lock_guard<std::mutex> guard(mutex_);
		const auto* sa = getCurrentSocketAddressImpl(connection);

		if (!sa || !addressFamily || !addressSocketType || !addressProtocol || !nativeSocketAddress ||
			!nativeSocketAddressLength) {
			return false;
		}

		*addressFamily = sa->addressFamily;
		*addressSocketType = sa->addressSocketType;
		*addressProtocol = sa->addressProtocol;
		*nativeSocketAddress = sa->nativeSocketAddress;
		*nativeSocketAddressLength = sa->nativeSocketAddressLength;

		return true;
	}

	std::string getCurrentAddressTlsTrustStore(dxf_connection_t connection) {
		std::lock_guard<std::mutex> guard(mutex_);

		const auto* a = getCurrentAddressImpl(connection);

		if (!a) {
			return "";
		}

		return a->tlsData.trustStore;
	}

	std::string getCurrentAddressTlsTrustStorePassword(dxf_connection_t connection) {
		std::lock_guard<std::mutex> guard(mutex_);

		const auto* a = getCurrentAddressImpl(connection);

		if (!a) {
			return "";
		}

		return a->tlsData.trustStorePassword;
	}

	std::string getCurrentAddressTlsKeyStore(dxf_connection_t connection) {
		std::lock_guard<std::mutex> guard(mutex_);

		const auto* a = getCurrentAddressImpl(connection);

		if (!a) {
			return "";
		}

		return a->tlsData.keyStore;
	}

	std::string getCurrentAddressTlsKeyStorePassword(dxf_connection_t connection) {
		std::lock_guard<std::mutex> guard(mutex_);

		const auto* a = getCurrentAddressImpl(connection);

		if (!a) {
			return "";
		}

		return a->tlsData.keyStorePassword;
	}

	Address* getCurrentAddress(dxf_connection_t connection) {
		std::lock_guard<std::mutex> guard(mutex_);

		return getCurrentAddressImpl(connection);
	}

#ifdef DXFEED_CODEC_TLS_ENABLED
	void setCurrentAddressTlsTrustStoreMem(dxf_connection_t connection, uint8_t* trustStoreMem) {
		std::lock_guard<std::mutex> guard(mutex_);
		auto* a = getCurrentAddressImpl(connection);

		if (!a) {
			return;
		}

		a->trustStoreMem = trustStoreMem;
	}

	void setCurrentAddressTlsTrustStoreLen(dxf_connection_t connection, size_t trustStoreLen) {
		std::lock_guard<std::mutex> guard(mutex_);
		auto* a = getCurrentAddressImpl(connection);

		if (!a) {
			return;
		}

		a->trustStoreLen = trustStoreLen;
	}

	void setCurrentAddressTlsKeyStoreMem(dxf_connection_t connection, uint8_t* keyStoreMem) {
		std::lock_guard<std::mutex> guard(mutex_);
		auto* a = getCurrentAddressImpl(connection);

		if (!a) {
			return;
		}

		a->keyStoreMem = keyStoreMem;
	}

	void setCurrentAddressTlsKeyStoreLen(dxf_connection_t connection, size_t keyStoreLen) {
		std::lock_guard<std::mutex> guard(mutex_);
		auto* a = getCurrentAddressImpl(connection);

		if (!a) {
			return;
		}

		a->keyStoreLen = keyStoreLen;
	}
#endif
};

}  // namespace dx