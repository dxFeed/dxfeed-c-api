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
#include "Logger.h"
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
#include <iterator>

#include "Result.hpp"
#include "StringUtils.hpp"

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

struct Property {
	std::string keyValue;
	bool used;
};

struct HostPort {
	std::string host;
	std::string port;
};

struct ParsedAddress {
	std::string specification;
	std::vector<std::vector<std::string>> codecs;  // one std::vector<std::string> per codec of format: [<codec-name>,
												   // <codec-property-1>, ..., <codec-property-N>]
	std::string address;
	std::vector<Property> properties;
	std::string defaultPort;
	std::vector<HostPort> hostsAndPorts;

	static std::pair<std::vector<HostPort>, Result> parseAddressList(const std::string& hostNames,
																	 const std::string& defaultPort) {
		std::vector<HostPort> result;

		for (const auto& addressString : StringUtils::split(hostNames, ",")) {
			auto host = addressString;
			auto port = defaultPort;
			auto colonPos = addressString.find_last_of(':');
			auto closingBracketPos = addressString.find_last_of(']');

			if (colonPos != std::string::npos &&
				(closingBracketPos == std::string::npos || colonPos > closingBracketPos)) {
				port = addressString.substr(colonPos + 1);

				try {
					int portValue = std::stoi(port);
					(void)(portValue);
				} catch (const std::exception& e) {
					return {{},
							AddressSyntaxError("Failed to parse port from address \"" +
											   StringUtils::hideCredentials(addressString) + "\": " + e.what())};
				}

				host = addressString.substr(0, colonPos);
			}

			if (StringUtils::startsWith(host, '[')) {
				if (!StringUtils::endsWith(host, ']')) {
					return {{},
							AddressSyntaxError("An expected closing square bracket is not found in address \"" +
											   StringUtils::hideCredentials(addressString) + "\"")};
				}

				host = host.substr(1, host.size() - 2);
			} else if (host.find(':') != std::string::npos) {
				return {{},
						AddressSyntaxError("IPv6 numeric address must be enclosed in square brackets in address \"" +
										   StringUtils::hideCredentials(addressString) + "\"")};
			}

			result.push_back({host, port});
		}

		return {result, Ok{}};
	}

	static std::pair<ParsedAddress, Result> parseAddress(std::string address) {
		// Parse configurable message adapter factory specification in address
		auto specSplitResult = StringUtils::splitParenthesisedStringAt(address, '@');

		if (!specSplitResult.second.isOk()) {
			return {{}, specSplitResult.second};
		}

		auto specSplit = specSplitResult.first;
		std::string specification = specSplit.size() == 1 ? "" : specSplit[0];
		address = specSplit.size() == 1 ? address : specSplit[1];

		// Parse additional properties at the end of address
		std::vector<std::string> propStrings{};

		auto parsePropertiesResult = StringUtils::parseProperties(address, propStrings);

		if (!parsePropertiesResult.second.isOk()) {
			return {{}, parsePropertiesResult.second};
		}

		address = parsePropertiesResult.first;

		// Parse codecs
		std::vector<std::vector<std::string>> codecs{};

		while (true) {
			auto codecSplitResult = StringUtils::splitParenthesisedStringAt(address, '+');

			if (!codecSplitResult.second.isOk()) {
				return {{}, codecSplitResult.second};
			}

			auto codecSplit = codecSplitResult.first;

			if (codecSplit.size() == 1) {
				break;
			}

			auto codecStr = codecSplit[0];
			address = codecSplit[1];

			std::vector<std::string> codecInfo{};

			codecInfo.emplace_back("");	 // reserving place for codec name

			auto parseCodecPropertiesResult = StringUtils::parseProperties(codecStr, codecInfo);

			if (!parseCodecPropertiesResult.second.isOk()) {
				return {{}, parseCodecPropertiesResult.second};
			}

			codecStr = parseCodecPropertiesResult.first;
			codecInfo[0] = codecStr;
			codecs.push_back(codecInfo);
		}

		std::reverse(codecs.begin(), codecs.end());

		std::vector<Property> properties{};

		std::transform(propStrings.begin(), propStrings.end(), std::back_inserter(properties),
					   [](const std::string& propertyString) -> Property { return {propertyString}; });

		// Parse port in address
		auto portSep = address.find_last_of(':');

		if (portSep == std::string::npos) {
			return {{},
					AddressSyntaxError("Port number is missing in \"" + StringUtils::hideCredentials(address) + "\"")};
		}

		auto defaultPort = address.substr(portSep + 1);

		try {
			int portValue = std::stoi(defaultPort);
			(void)(portValue);
		} catch (const std::exception& e) {
			return {{},
					AddressSyntaxError("Port number format error in \"" + StringUtils::hideCredentials(address) +
									   "\": " + e.what())};
		}

		address = StringUtils::trimCopy(address.substr(0, portSep));

		auto parseHostsAndPortsResult = parseAddressList(address, defaultPort);

		if (!parseHostsAndPortsResult.second.isOk()) {
			return {{}, parseHostsAndPortsResult.second};
		}

		return {{specification, codecs, address, properties, defaultPort, parseHostsAndPortsResult.first}, Ok{}};
	}
};

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

			result.push_back(
				SocketAddress{addr->ai_family, addr->ai_socktype, addr->ai_protocol, *(addr->ai_addr), addr->ai_addrlen,
							  StringUtils::ipToString<sockaddr, AF_INET, sockaddr_in, AF_INET6, sockaddr_in6>(
								  addr->ai_addr, inet_ntop),
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

	static std::pair<std::string, std::string> parseUserNameAndPasswordProperties(std::vector<Property>& properties) {
		std::string userName{};
		std::string password{};

		for (auto& p : properties) {
			if (p.used) {
				continue;
			}

			auto splitKeyValueResult = StringUtils::split(p.keyValue, "=");

			if (splitKeyValueResult.size() != 2) {
				continue;
			}

			if (splitKeyValueResult[0] == "username") {
				userName = splitKeyValueResult[1];
				p.used = true;
			} else if (splitKeyValueResult[0] == "password") {
				password = splitKeyValueResult[1];
				p.used = true;
			}
		}

		return {userName, password};
	}

	static Address::GzipData parseGzipCodecProperties(const std::vector<std::vector<std::string>>& codecs) {
		for (const auto& codec : codecs) {
			// check the codec name
			if (codec[0] == "gzip") {
				return {true};
			}
		}

		return {false};
	}

	static Address::TlsData parseTlsCodecProperties(const std::vector<std::vector<std::string>>& codecs) {
		Address::TlsData result{false};

		for (const auto& codec : codecs) {
			// check the codec name
			if (codec[0] != "tls" && codec[0] != "ssl") {
				continue;
			}

			result.enabled = true;

			if (codec.size() <= 1) {
				return result;
			}

			for (std::size_t i = 1; i < codec.size(); i++) {
				auto splitKeyValueResult = StringUtils::split(codec[i], "=");

				if (splitKeyValueResult.size() != 2) {
					continue;
				}

				if (splitKeyValueResult[0] == "keyStore") {
					result.keyStore = splitKeyValueResult[1];
				} else if (splitKeyValueResult[0] == "keyStorePassword") {
					result.keyStorePassword = splitKeyValueResult[1];
				} else if (splitKeyValueResult[0] == "trustStore") {
					result.trustStore = splitKeyValueResult[1];
				} else if (splitKeyValueResult[0] == "trustStorePassword") {
					result.trustStorePassword = splitKeyValueResult[1];
				}
			}

			break;
		}

		return result;
	}

	static ResolvedAddresses resolveAddresses(dxf_connection_t connection) {
		auto address = dx_get_connection_address_string(connection);

		if (address == nullptr) {
			dx_logging_error_f(L"AddressesManager::resolveAddresses: address is NULL");

			return {};
		}

		auto splitAddressesResult = StringUtils::splitParenthesisSeparatedString(address);

		if (!splitAddressesResult.second.isOk()) {
			dx_logging_error_f(L"AddressesManager::resolveAddresses: %hs", splitAddressesResult.second.what().c_str());

			return {};
		}

		if (splitAddressesResult.first.empty()) {
			dx_logging_error_f(L"AddressesManager::resolveAddresses: empty addresses list");

			return {};
		}

		std::vector<Address> addresses;
		std::vector<SocketAddress> socketAddresses;

		for (const auto& addressList : splitAddressesResult.first) {
			auto parseAddressResult = ParsedAddress::parseAddress(addressList);

			if (!parseAddressResult.second.isOk()) {
				continue;
			}

			auto properties = parseAddressResult.first.properties;

			auto gzipCodecProperties = parseGzipCodecProperties(parseAddressResult.first.codecs);
			auto tlsCodecProperties = parseTlsCodecProperties(parseAddressResult.first.codecs);
			auto userNameAndPassword = parseUserNameAndPasswordProperties(properties);

			for (const auto& hostAndPort : parseAddressResult.first.hostsAndPorts) {
				dx_logging_info(L"AddressesManager::resolveAddresses: [con = %p] Resolving IPs for %hs", connection,
								hostAndPort.host.c_str());

				auto inetAddresses = SocketAddress::getAllInetAddresses(hostAndPort.host, hostAndPort.port,
																		[&addresses] { return addresses.size(); });

				dx_logging_info(L"AddressesManager::resolveAddresses: [con = %p] Resolved IPs: %d", connection,
								static_cast<int>(inetAddresses.size()));

				if (inetAddresses.empty()) {
					continue;
				}

				for (const auto& a : inetAddresses) {
					dx_logging_verbose(dx_ll_debug, L"AddressesManager::resolveAddresses: [con = %p]     %hs",
									   connection, a.resolvedIp.c_str());
				}

				socketAddresses.insert(socketAddresses.end(), inetAddresses.begin(), inetAddresses.end());

				addresses.push_back(Address{
					hostAndPort.host,
					hostAndPort.port,
					userNameAndPassword.first,
					userNameAndPassword.second,
					tlsCodecProperties,
					gzipCodecProperties,
#ifdef DXFEED_CODEC_TLS_ENABLED
					nullptr,
					0,
					nullptr,
					0
#endif
				});
			}
		}

		shuffle(socketAddresses.begin(), socketAddresses.end());

		dx_logging_verbose(dx_ll_debug, L"AddressesManager::resolveAddresses: [con = %p] Shuffled addresses:",
						   connection);
		for (const auto& a : socketAddresses) {
			dx_logging_verbose(dx_ll_debug, L"AddressesManager::resolveAddresses: [con = %p]     %hs", connection,
							   a.resolvedIp.c_str());
		}

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