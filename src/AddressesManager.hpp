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

struct SocketAddress {
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

	sockaddr nativeSocketAddress;
	std::string host;
	std::string resolvedIp;
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

	bool isConnectionFailed;
};

struct ResolvedAddresses {
	// host:port -> socket address idx
	std::unordered_map<std::string, std::size_t> hostAndPortToAddress;
	std::vector<SocketAddress> socketAddresses;
	std::size_t currentSocketAddress{};
};

// singleton
class AddressesManager {
	// connection context -> addresses
	std::unordered_map<void*, std::shared_ptr<ResolvedAddresses>> resolvedAddresses_;
	std::mutex mutex_;

	AddressesManager() : resolvedAddresses_{}, mutex_{} {}

	static void sleepBeforeResolve() {
		thread_local ReconnectHelper reconnectHelper{};

		reconnectHelper.sleepBeforeConnection();
	}

	std::vector<SocketAddress> resolveAddresses(void* connectionContext) {
		auto address = dx_get_connection_address_string(connectionContext);

		if (address == nullptr) {
			return {};
		}

		dx_address_array_t addressArray;

		if (!dx_get_addresses_from_collection(address, &addressArray) || addressArray.size == 0) {
			return {};
		}

		addrinfo hints = {};

		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		std::vector<SocketAddress> socketAddresses;

		/*
typedef struct {
	char* host;
	const char* port;
	char* username;
	char* password;
	dx_codec_tls_t tls;
	dx_codec_gzip_t gzip;
} dx_address_t;
		 */

		// TODO: enum addresses
		addrinfo* addr = nullptr;

		for (std::size_t addressIndex = 0; addressIndex < addressArray.size; addressIndex++) {
			if (!dx_getaddrinfo(addressArray.elements[addressIndex].host, addressArray.elements[addressIndex].port,
								&hints, &addr)) {
				continue;
			}

			//...
		}

		dx_clear_address_array(&addressArray);

		return {};
	}

public:
	static std::shared_ptr<AddressesManager> getInstance() {
		static std::shared_ptr<AddressesManager> instance{new AddressesManager()};

		return instance;
	}

	SocketAddress getNextAddress(void* connectionContext) {
		if (connectionContext == nullptr) {
			return {};
		}

		auto resolvedAddress = [&]() -> std::shared_ptr<ResolvedAddresses> {
			auto found = resolvedAddresses_.find(connectionContext);

			if (found == resolvedAddresses_.end()) {
				auto result = resolvedAddresses_.emplace(connectionContext, std::make_shared<ResolvedAddresses>());

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

		if (resolvedAddress->currentSocketAddress >= resolvedAddress->socketAddresses.size()) {
			// TODO: sleep in the current thread only
			sleepBeforeResolve();
			resolveAddresses(connectionContext);
			resolvedAddress->currentSocketAddress = 0;
		}

		if (resolvedAddress->socketAddresses.empty()) {
			return {};
		}

		return resolvedAddress->socketAddresses[resolvedAddress->currentSocketAddress];
	}

	void clearAddresses(void* connectionContext) {}
};

}  // namespace dx