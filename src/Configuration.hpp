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

#pragma once

#include <iostream>
#include <iterator>
#include <locale>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <toml.hpp>
#include <unordered_map>

namespace dx {
namespace algorithm {
namespace detail {

class IsIEqual {
	std::locale locale_;

public:
	explicit IsIEqual(const std::locale& locale = std::locale()) : locale_{locale} {}

	template <typename T, typename U>
	bool operator()(const T& t, const U& u) const {
		return std::tolower<T>(t, locale_) == std::tolower<U>(u, locale_);
	}
};

class IsSpace {
	std::locale locale_;

public:
	explicit IsSpace(const std::locale& locale = std::locale()) : locale_{locale} {}

	template <typename T>
	bool operator()(const T& t) const {
		return std::isspace<T>(t, locale_);
	}
};

template <typename ForwardIterator, typename Predicate>
inline ForwardIterator trimEndWithIteratorCategory(ForwardIterator begin, ForwardIterator end, Predicate isSpace,
												   std::forward_iterator_tag) {
	ForwardIterator result = begin;

	for (ForwardIterator it = begin; it != end; ++it) {
		if (!isSpace(*it)) {
			result = it;
			++result;
		}
	}

	return result;
}

template <typename BidirectionalIterator, typename Predicate>
inline BidirectionalIterator trimEndWithIteratorCategory(BidirectionalIterator begin, BidirectionalIterator end,
														 Predicate isSpace, std::bidirectional_iterator_tag) {
	for (BidirectionalIterator it = end; it != begin;) {
		if (!isSpace(*(--it))) {
			return ++it;
		}
	}

	return begin;
}

template <typename ForwardIterator, typename Predicate>
inline ForwardIterator trimBegin(ForwardIterator begin, ForwardIterator end, Predicate isSpace) {
	ForwardIterator it = begin;

	for (; it != end; ++it) {
		if (!isSpace(*it)) {
			return it;
		}
	}

	return it;
}

template <typename Iterator, typename Predicate>
inline Iterator trimEnd(Iterator begin, Iterator end, Predicate isSpace) {
	return trimEndWithIteratorCategory(begin, end, isSpace,
									   typename std::iterator_traits<Iterator>::iterator_category());
}

}  // namespace detail

template <typename Range1, typename Range2, typename Predicate>
inline bool equals(const Range1& first, const Range2& second, Predicate cmp) {
	auto firstIt = std::begin(first);
	auto secondIt = std::begin(second);

	for (; firstIt != std::end(first) && secondIt != std::end(second); ++firstIt, secondIt++) {
		if (!cmp(*firstIt, *secondIt)) {
			return false;
		}
	}

	return (secondIt == std::end(second)) && (firstIt == std::end(first));
}

template <typename Range1, typename Range2>
inline bool iEquals(const Range1& first, const Range2& second, const std::locale& locale = std::locale()) {
	return equals(first, second, detail::IsIEqual(locale));
}

template <typename Range, typename Predicate>
inline Range trimCopyIf(const Range& range, Predicate isSpace) {
	auto trimEnd = detail::trimEnd(std::begin(range), std::end(range), isSpace);

	return Range(detail::trimBegin(std::begin(range), trimEnd, isSpace), trimEnd);
}

template <typename Range>
inline Range trimCopy(const Range& range, const std::locale& locale = std::locale()) {
	return trimCopyIf(range, detail::IsSpace(locale));
}

}  // namespace algorithm

struct Configuration : std::enable_shared_from_this<Configuration> {
	enum class Type { None, String, File };

private:
	Type type_;
	std::string config_;
	mutable std::mutex mutex_;
	bool loaded_ = false;
	toml::value properties_;

	Configuration() : type_{Type::None}, config_{}, mutex_{}, properties_{} {}

public:
	static std::shared_ptr<Configuration> getInstance() {
		static std::shared_ptr<Configuration> instance{new Configuration()};

		return instance;
	}

	bool isLoaded() const {
		std::lock_guard<std::mutex> lock(mutex_);

		return loaded_;
	}

	bool loadFromFile(const std::string& fileName) {
		std::lock_guard<std::mutex> lock(mutex_);

		if (loaded_ || algorithm::trimCopy(fileName).empty()) {
			return false;
		}

		try {
			properties_ = toml::parse(fileName);
			loaded_ = true;
			type_ = Type::File;
			config_ = fileName;

			return true;
		} catch (const std::exception& e) {
			std::cerr << e.what() << "\n";
			loaded_ = false;
		}

		return false;
	}

	bool loadFromString(const std::string& config) {
		std::lock_guard<std::mutex> lock(mutex_);

		if (loaded_ || algorithm::trimCopy(config).empty()) {
			return false;
		}

		try {
			std::istringstream iss(config);

			properties_ = toml::parse(iss);
			loaded_ = true;
			type_ = Type::String;
			config_ = config;

			return true;
		} catch (const std::exception& e) {
			std::cerr << e.what() << "\n";
			loaded_ = false;
		}

		return false;
	}

	template <typename T>
	T getProperty(const std::string& groupName, const std::string& fieldName, T defaultValue) const {
		std::lock_guard<std::mutex> lock(mutex_);

		if (!loaded_) {
			return defaultValue;
		}

		try {
			auto group = toml::find(properties_, groupName);
			auto value = toml::find_or(group, fieldName, defaultValue);
			return value;
		} catch (const std::exception&) {
			return defaultValue;
		}
	}

	int getHeartbeatPeriod(int defaultValue = 10) const {
		return getProperty("network", "heartbeatPeriod", defaultValue);
	}

	int getHeartbeatTimeout(int defaultValue = 120) const {
		return getProperty("network", "heartbeatTimeout", defaultValue);
	}
};
}  // namespace dx
