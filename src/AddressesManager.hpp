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
#include "DXFeed.h"
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
};

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
			std::this_thread::sleep_for(std::chrono::milliseconds{sleepTime});
		}

		startTime_ = System::currentTimeMillis();
	}

	void reset() {
		startTime_ = 0;
	}
};

struct SocketAddress {
	sockaddr nativeSocketAddress;
	std::string host;
	std::string resolvedIp;
	std::string port;
};

struct ResolvedAddresses {
	std::unordered_map<std::string, SocketAddress> hostAndPortToAddress;
	// std::hash
	// std::unordered_map<SocketAddress, std::string> address;
	std::vector<SocketAddress> addresses;
};

// singleton
class AddressesManager {
	// connection context -> addresses
	std::unordered_map<void*, ResolvedAddresses> resolvedAddresses_;
	// connection context -> helper
	std::unordered_map<void*, ReconnectHelper> reconnectHelpers_;
	std::mutex mutex_;

	AddressesManager() : resolvedAddresses_{}, reconnectHelpers_{}, mutex_{} {}

public:
	static std::shared_ptr<AddressesManager> getInstance() {
		static std::shared_ptr<AddressesManager> instance{new AddressesManager()};

		return instance;
	}

	std::vector<SocketAddress> resolveAddresses(void* connectionContext) { return {}; }

	SocketAddress getNextAddress(void* connectionContext) { return {}; }

	void clearAddresses(void* connectionContext) {
	}
};

}  // namespace dx