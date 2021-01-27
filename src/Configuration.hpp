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

#include <iterator>
#include <locale>
#include <stdexcept>
#include <string>
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

class Configuration {
	std::unordered_map<std::string, std::string> properties_{};

public:
	Configuration() = default;

	Configuration(const std::unordered_map<std::string, std::string>& properties) : properties_{properties} {}

	std::string getString(const std::string& key, const std::string& defaultValue) {
		auto found = properties_.find(key);

		if (found == properties_.end()) {
			return defaultValue;
		}

		return found->second;
	}

	int getInt(const std::string& key, int defaultValue) {
		auto found = properties_.find(key);

		if (found == properties_.end()) {
			return defaultValue;
		}

		auto foundValue = found->second;

		try {
			return std::stoi(foundValue, nullptr, 10);
		} catch (const std::exception&) {
			return defaultValue;
		}
	}

	long getLong(const std::string& key, long defaultValue) {
		auto found = properties_.find(key);

		if (found == properties_.end()) {
			return defaultValue;
		}

		auto foundValue = found->second;

		try {
			return std::stol(foundValue, nullptr, 10);
		} catch (const std::exception&) {
			return defaultValue;
		}
	}

	long long getLongLong(const std::string& key, long long defaultValue) {
		auto found = properties_.find(key);

		if (found == properties_.end()) {
			return defaultValue;
		}

		auto foundValue = found->second;

		try {
			return std::stoll(foundValue, nullptr, 10);
		} catch (const std::exception&) {
			return defaultValue;
		}
	}

	double getDouble(const std::string& key, double defaultValue) {
		auto found = properties_.find(key);

		if (found == properties_.end()) {
			return defaultValue;
		}

		auto foundValue = found->second;

		try {
			return std::stod(foundValue, nullptr);
		} catch (const std::exception&) {
			return defaultValue;
		}
	}

	bool getBool(const std::string& key, bool defaultValue) {
		auto found = properties_.find(key);

		if (found == properties_.end()) {
			return defaultValue;
		}

		auto foundValue = found->second;

		return algorithm::iEquals("true", algorithm::trimCopy(foundValue));
	}
};
}  // namespace dx
