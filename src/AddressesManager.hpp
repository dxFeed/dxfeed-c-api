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
#include "Logger.h"
}

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <random>
#include <stack>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Configuration.hpp"

namespace dx {

// TODO: std::expected
class Result {
	std::string data_;

public:
	explicit Result(const std::string& data) noexcept : data_{data} {}

	virtual const std::string& what() const noexcept { return data_; }

	virtual ~Result() = default;

	virtual bool isOk() const { return false; }
};

struct Ok : Result {
	explicit Ok() noexcept : Result("Ok") {}

	bool isOk() const override { return true; }
};

struct RuntimeError : Result {
	explicit RuntimeError(const std::string& error) noexcept : Result(error) {}
};

struct InvalidFormatError : RuntimeError {
	explicit InvalidFormatError(const std::string& error) noexcept : RuntimeError(error) {}
};

struct AddressSyntaxError : InvalidFormatError {
	explicit AddressSyntaxError(const std::string& error) noexcept : InvalidFormatError(error) {}
};

namespace StringUtils {
static const char ESCAPE_CHAR = '\\';

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

static inline bool startsWith(const std::string& str, char c) { return !str.empty() && str.find_first_of(c) == 0; }

static inline bool endsWith(const std::string& str, char c) { return !str.empty() && str.find_last_of(c) == str.size() - 1; }

static inline std::pair<std::vector<std::string>, Result> splitParenthesisSeparatedString(
	const std::string& s) noexcept {
	if (!startsWith(s, '(')) {
		return {{s}, Ok{}};
	}

	std::vector<std::string> result{};
	int cnt = 0;
	int startIndex = -1;

	for (int i = 0; i < s.size(); i++) {
		char c = s[i];
		switch (c) {
			case '(':
				if (cnt++ == 0) {
					startIndex = i + 1;
				}

				break;

			case ')':
				if (cnt == 0) {
					return {result, AddressSyntaxError("Extra closing parenthesis ')' in a list")};
				}

				if (--cnt == 0) {
					result.push_back(s.substr(startIndex, i - startIndex));
				}

				break;

			default:
				if (cnt == 0 && c > ' ') {
					return {result,
							AddressSyntaxError(std::string("Unexpected character '") + c +
											   "' outside parenthesis in a list")};
				}

				if (c == ESCAPE_CHAR) {	 // escapes next char (skip it)
					i++;
				}
		}
	}

	if (cnt > 0) {
		return {result, AddressSyntaxError("Missing closing parenthesis ')' in a list")};
	}

	return {result, Ok{}};
}
// -> ([""], Ok | AddressSyntaxError)
static inline std::pair<std::vector<std::string>, Result> splitParenthesisedStringAt(const std::string& s,
																					 char atChar) noexcept {
	std::stack<char> stack;

	for (int i = 0; i < s.size(); i++) {
		char c = s[i];

		switch (c) {
			case '(':
				stack.push(')');
				break;
			case '[':
				stack.push(']');
				break;
			case ')':
			case ']': {
				if (stack.empty()) {
					return {{s}, AddressSyntaxError(std::string("Extra closing parenthesis: ") + c)};
				}

				char top = stack.top();
				stack.pop();

				if (top != c) {
					return {{s}, AddressSyntaxError(std::string("Wrong closing parenthesis: ") + c)};
				}
			}

			break;
			case ESCAPE_CHAR:  // escapes next char (skip it)
				i++;
				break;
			default:
				if (stack.empty() && c == atChar) {
					return {{algorithm::trimCopy(s.substr(0, i)), algorithm::trimCopy(s.substr(i + 1))}, Ok{}};
				}
		}
	}

	return {{s}, Ok{}};	 // at char is not found
}

static inline bool isEscapedCharAt(const std::string& s, std::int64_t index) noexcept {
	std::int64_t escapeCount = 0;

	while (--index >= 0 && s[index] == ESCAPE_CHAR) {
		escapeCount++;
	}

	return escapeCount % 2 != 0;
}

/**
 * Parses additional properties at the end of the given description string.
 * Properties can be enclosed in pairs of matching '[...]' or '(...)' (the latter is deprecated).
 * Multiple properties can be specified with multiple pair of braces or be comma-separated inside
 * a single pair, so both "something[prop1,prop2]" and "something[prop1][prop2]" are
 * valid properties specifications. All braces must go in matching pairs.
 * Original string and all properties string are trimmed from extra space, so
 * extra spaces around or inside braces are ignored.
 * Special characters can be escaped with a backslash.
 *
 * @param description Description string to parse.
 * @param keyValueVector Collection of strings where parsed properties are added to.
 * @return The resulting description string without properties + Ok | std::runtime_error | InvalidFormatError.
 */
static inline std::pair<std::string, Result> parseProperties(std::string description,
															 std::vector<std::string>& keyValueVector) noexcept {
	description = algorithm::trimCopy(description);

	std::vector<std::string> result;
	std::deque<char> deque;

	while ((endsWith(description, ')') || endsWith(description, ']')) &&
		   !isEscapedCharAt(description, description.size() - 1)) {
		std::int64_t prop_end = description.size() - 1;
		std::int64_t i = 0;
		// going backwards

		for (i = description.size(); --i >= 0;) {
			char c = description[i];

			if (c == ESCAPE_CHAR || isEscapedCharAt(description, i)) {
				continue;
			}

			bool done = false;

			switch (c) {
				case ']':
					deque.push_back('[');
					break;
				case ')':
					deque.push_back('(');
					break;
				case ',':
					if (deque.size() == 1) {
						// this is a top-level comma
						result.push_back(algorithm::trimCopy(description.substr(i + 1, prop_end - i - 1)));
						prop_end = i;
					}
					break;
				case '[':
				case '(':
					if (deque.empty()) {
						return {{description}, RuntimeError("StringUtils::parseProperties: empty internal deque!")};
					}

					char expect = deque.back();

					deque.pop_back();

					if (c != expect) {
						return {{description},
								InvalidFormatError(std::string("Unmatched '") + c + "' in a list of properties")};
					}

					if (deque.empty()) {
						result.push_back(algorithm::trimCopy(description.substr(i + 1, prop_end - i - 1)));

						done = true;
					}

					break;
			}

			if (done) {
				break;
			}
		}

		if (i < 0) {
			return {{description},
					InvalidFormatError(std::string("Extra '") + deque.front() + "' in a list of properties")};
		}

		description = algorithm::trimCopy(description.substr(0, i));
	}

	std::reverse(result.begin(), result.end());	 // reverse properties into original order
	keyValueVector.insert(keyValueVector.end(), result.begin(), result.end());

	return {{description}, Ok{}};
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

struct Property {
	std::string keyValue;
	bool used;
};

struct ParsedAddress {
	std::string specification;
	std::vector<std::vector<std::string>> codecs;  // one std::vector<std::string> per codec of format: [<codec-name>,
												   // <codec-property-1>, ..., <codec-property-N>]
	std::string address;
	std::vector<Property> properties;

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

		std::transform(propStrings.begin(), propStrings.end(), properties.begin(),
					   [](const std::string& propertyString) -> Property { return {propertyString}; });

		//TODO: parse default port and split addresses + set port

		return {{specification, codecs, address, properties}, Ok{}};
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

			dx_logging_info(L"AddressesManager - [con = %p] Resolving IPs for %hs", connection, host.c_str());

			auto inetAddresses =
				SocketAddress::getAllInetAddresses(host, port, [&addresses] { return addresses.size(); });

			dx_logging_info(L"AddressesManager - [con = %p] Resolved IPs: %d", connection,
							static_cast<int>(inetAddresses.size()));

			if (inetAddresses.empty()) {
				continue;
			}

			for (const auto& a : inetAddresses) {
				dx_logging_verbose(dx_ll_debug, L"AddressesManager - [con = %p]     %hs", connection,
								   a.resolvedIp.c_str());
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

		dx_logging_verbose(dx_ll_debug, L"AddressesManager - [con = %p] Shuffled addresses:", connection);
		for (const auto& a : socketAddresses) {
			dx_logging_verbose(dx_ll_debug, L"AddressesManager - [con = %p]     %hs", connection, a.resolvedIp.c_str());
		}

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