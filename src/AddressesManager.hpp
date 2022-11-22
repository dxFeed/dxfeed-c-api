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
	ReconnectHelper(std::int64_t delay = 100) : delay_{delay} {}

	void setDelay(std::int64_t delay) { delay_ = delay; }

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
	std::string user;
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
	sockaddr nativeSocketAddress;
	std::string resolvedIp;
	std::size_t addressIdx;
	bool isConnectionFailed;
};

struct ResolvedAddresses {
	std::vector<Address> addresses;
	std::vector<SocketAddress> socketAddresses;
	std::size_t currentSocketAddress;
};

// struct InetAddress {
//	int family;
//	int type;
//	int protocol;
//
//	static std::vector<>
// };

std::string ipToString(const sockaddr* sa) {
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

std::string toString(const char* cString) {
	if (cString == nullptr) {
		return "";
	}

	return cString;
}

// singleton
class AddressesManager {
	// connection -> addresses
	std::unordered_map<dxf_connection_t, std::shared_ptr<ResolvedAddresses>> resolvedAddresses_;
	std::mutex mutex_;

	AddressesManager() : resolvedAddresses_{}, mutex_{} {}

	static void sleepBeforeResolve() {
		thread_local ReconnectHelper reconnectHelper{};

		reconnectHelper.sleepBeforeConnection();
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

		addrinfo hints = {};

		hints.ai_family = AF_UNSPEC;
		// hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		std::vector<Address> addresses;
		std::vector<SocketAddress> socketAddresses;
		addrinfo* addr = nullptr;

		for (std::size_t addressIndex = 0; addressIndex < addressArray.size; addressIndex++) {
			if (!dx_getaddrinfo(addressArray.elements[addressIndex].host, addressArray.elements[addressIndex].port,
								&hints, &addr)) {
				continue;
			}

			for (; addr != nullptr; addr = addr->ai_next) {
				if (addr->ai_family != AF_INET && addr->ai_family != AF_INET6) {
					continue;
				}

				socketAddresses.push_back(
					SocketAddress{*(addr->ai_addr), ipToString(addr->ai_addr), addresses.size(), false});
			}

			addresses.push_back(Address{toString(addressArray.elements[addressIndex].host),
										toString(addressArray.elements[addressIndex].port),
										toString(addressArray.elements[addressIndex].username),
										toString(addressArray.elements[addressIndex].password),
										{addressArray.elements[addressIndex].tls.enabled != 0,
										 toString(addressArray.elements[addressIndex].tls.key_store),
										 toString(addressArray.elements[addressIndex].tls.key_store_password),
										 toString(addressArray.elements[addressIndex].tls.trust_store),
										 toString(addressArray.elements[addressIndex].tls.trust_store_password)},
										{addressArray.elements[addressIndex].gzip.enabled != 0}});
		}

		auto randomEngine = std::default_random_engine{};
		std::shuffle(socketAddresses.begin(), socketAddresses.end(), randomEngine);

		dx_clear_address_array(&addressArray);

		return ResolvedAddresses{addresses, socketAddresses, 0};
	}

public:
	static std::shared_ptr<AddressesManager> getInstance() {
		static std::shared_ptr<AddressesManager> instance{new AddressesManager()};

		return instance;
	}

	SocketAddress getNextAddress(dxf_connection_t connection) {
		if (connection == nullptr) {
			return {};
		}

		{
			std::lock_guard<std::mutex> guard(mutex_);

			auto resolvedAddress = [&]() -> std::shared_ptr<ResolvedAddresses> {
				auto found = resolvedAddresses_.find(connection);

				if (found == resolvedAddresses_.end()) {
					auto result = resolvedAddresses_.emplace(connection, std::make_shared<ResolvedAddresses>());

					if (result.second) {
						return result.first->second;
					}

					return nullptr;
				}

				return found->second;
			}();

			if (!resolvedAddress) {
				return {};
			}

			resolvedAddress->currentSocketAddress++;

			if (resolvedAddress->currentSocketAddress < resolvedAddress->socketAddresses.size()) {
				return resolvedAddress->socketAddresses[resolvedAddress->currentSocketAddress];
			}
		}

		sleepBeforeResolve();

		auto newResolvedAddresses = resolveAddresses(connection);

		{
			std::lock_guard<std::mutex> guard(mutex_);

			*resolvedAddresses_[connection] = newResolvedAddresses;

			if (newResolvedAddresses.socketAddresses.empty()) {
				return {};
			}

			return resolvedAddresses_[connection]->socketAddresses[0];
		}
	}

	void clearAddresses(dxf_connection_t connection) {}
};

}  // namespace dx