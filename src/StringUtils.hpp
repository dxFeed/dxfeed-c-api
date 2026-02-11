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

#include <algorithm>
#include <cassert>
#include <deque>
#include <limits>
#include <locale>
#include <stack>
#include <string>
#include <vector>
#include <cstdint>

#include "Result.hpp"

namespace dx {

namespace StringUtils {

static constexpr char ESCAPE_CHAR = '\\';

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

inline std::vector<std::string> split(const std::string& str, const std::string& delim, int limit = 0) {
	bool limited = limit > 0;
	std::vector<std::string> result;
	std::size_t found, index = 0;

	while(index < str.size() && (found = str.find(delim, index)) != std::string::npos) {
		if (!limited || result.size() < static_cast<unsigned>(limit - 1)) {
			result.push_back(str.substr(index, found - index + (delim.empty() ? 1 : 0)));
			index = found + (delim.empty() ? 1 : delim.size());
		} else if (result.size() == limit - 1) {
			result.push_back(str.substr(index));
			index = str.size();
		}
	}

	if (index == 0) {
		return {str};
	}

	if (!limited || result.size() < static_cast<unsigned>(limit)) {
		result.push_back(str.substr(index));
	}

	auto resultSize = result.size();

	if (limit == 0) {
		while (resultSize > 0 && result[resultSize - 1].empty()) {
			resultSize--;
		}
	}

	if (resultSize == 0) {
		return {};
	}

	return {result.begin(), result.begin() + resultSize};
}

template <typename SockAddr, int AfInet, typename SockAddrIn, int AfInet6, typename SockAddrIn6, typename InetNToPFunc>
static inline std::string ipToString(const SockAddr* sa, InetNToPFunc inetNToP) {
	const std::size_t size = 256;
	char buf[size] = {};

	switch (sa->sa_family) {
		case AfInet:
			inetNToP(AfInet, &((SockAddrIn*)sa)->sin_addr, buf, size - 1);
			break;

		case AfInet6:
			inetNToP(AfInet6, &((SockAddrIn6*)sa)->sin6_addr, buf, size - 1);
			break;

		default:
			return "";
	}

	return buf;
}

inline std::string toString(const char* cString) {
	if (cString == nullptr) {
		return "";
	}

	return cString;
}

inline bool startsWith(const std::string& str, char c) { return !str.empty() && str.find_first_of(c) == 0; }

inline bool endsWith(const std::string& str, char c) { return !str.empty() && str.find_last_of(c) == str.size() - 1; }

inline std::pair<std::vector<std::string>, Result> splitParenthesisSeparatedString(const std::string& s) noexcept {
	if (!startsWith(s, '(')) {
		return {{s}, Ok{}};
	}

	std::vector<std::string> result{};
	std::int64_t cnt = 0;
	std::int64_t startIndex = -1;

	for (std::size_t i = 0; i < s.size(); i++) {
		char c = s[i];
		switch (c) {
			case '(':
				if (cnt++ == 0) {
					startIndex = static_cast<std::int64_t>(i) + 1;
				}

				break;

			case ')':
				if (cnt == 0) {
					return {result,
							AddressSyntaxError("dx::StringUtils::splitParenthesisSeparatedString: Extra closing "
											   "parenthesis ')' in a list")};
				}

				if (--cnt == 0) {
					result.push_back(s.substr(static_cast<std::string::size_type>(startIndex), i - static_cast<std::string::size_type>(startIndex)));
				}

				break;

			default:
				if (cnt == 0 && c > ' ') {
					return {
						result,
						AddressSyntaxError(
							std::string("dx::StringUtils::splitParenthesisSeparatedString: Unexpected character '") +
							c + "' outside parenthesis in a list")};
				}

				if (c == ESCAPE_CHAR) {	 // escapes next char (skip it)
					i++;
				}
		}
	}

	if (cnt > 0) {
		return {result,
				AddressSyntaxError(
					"dx::StringUtils::splitParenthesisSeparatedString: Missing closing parenthesis ')' in a list")};
	}

	return {result, Ok{}};
}
// -> ([""], Ok | AddressSyntaxError)
inline std::pair<std::vector<std::string>, Result> splitParenthesisedStringAt(const std::string& s,
																			  char atChar) noexcept {
	std::stack<char> stack;

	for (std::size_t i = 0; i < s.size(); i++) {
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
					return {
						{s},
						AddressSyntaxError(
							std::string("dx::StringUtils::splitParenthesisedStringAt: Extra closing parenthesis: ") +
							c)};
				}

				char top = stack.top();
				stack.pop();

				if (top != c) {
					return {
						{s},
						AddressSyntaxError(
							std::string("dx::StringUtils::splitParenthesisedStringAt: Wrong closing parenthesis: ") +
							c)};
				}
			}

			break;
			case ESCAPE_CHAR:  // escapes next char (skip it)
				i++;
				break;
			default:
				if (stack.empty() && c == atChar) {
					return {{trimCopy(s.substr(0, i)), trimCopy(s.substr(i + 1))}, Ok{}};
				}
		}
	}

	return {{s}, Ok{}};	 // at char is not found
}

inline bool isEscapedCharAt(const std::string& s, std::int64_t index) noexcept {
	std::int64_t escapeCount = 0;

	while (--index >= 0 && s[static_cast<std::size_t>(index)] == ESCAPE_CHAR) {
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
inline std::pair<std::string, Result> parseProperties(std::string description,
													  std::vector<std::string>& keyValueVector) noexcept {
	assert(description.size() <= static_cast<std::size_t>((std::numeric_limits<std::int64_t>::max)()));

	if (description.size() > static_cast<std::size_t>((std::numeric_limits<std::int64_t>::max)())) {
		return {description, IllegalArgumentError("")};
	}

	description = trimCopy(description);

	std::vector<std::string> result;
	std::deque<char> deque;

	while ((endsWith(description, ')') || endsWith(description, ']')) &&
		   !isEscapedCharAt(description, description.size() - 1)) {
		std::int64_t prop_end = description.size() - 1;
		std::int64_t i = 0;
		// going backwards

		for (i = description.size(); --i >= 0;) {
			char c = description[static_cast<std::size_t>(i)];

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
						result.push_back(trimCopy(description.substr(static_cast<std::string::size_type>(i + 1), static_cast<std::string::size_type>(prop_end - i - 1))));
						prop_end = i;
					}
					break;
				case '[':
				case '(':
					if (deque.empty()) {
						return {{description}, RuntimeError("dx::StringUtils::parseProperties: empty internal deque!")};
					}

					char expect = deque.back();

					deque.pop_back();

					if (c != expect) {
						return {{description},
								InvalidFormatError(std::string("dx::StringUtils::parseProperties: Unmatched '") + c +
												   "' in a list of properties")};
					}

					if (deque.empty()) {
						result.push_back(trimCopy(description.substr(static_cast<std::string::size_type>(i + 1), static_cast<std::string::size_type>(prop_end - i - 1))));

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
					InvalidFormatError(std::string("dx::StringUtils::parseProperties: Extra '") + deque.front() +
									   "' in a list of properties")};
		}

		description = trimCopy(description.substr(0, static_cast<std::string::size_type>(i)));
	}

	std::reverse(result.begin(), result.end());	 // reverse properties into original order
	keyValueVector.insert(keyValueVector.end(), result.begin(), result.end());

	return {{description}, Ok{}};
}

template <typename LogLevel>
inline std::string loggingLevelToString(LogLevel level) {
	switch (level) {
		case LogLevel::dx_ll_trace:
			return "trace";
		case LogLevel::dx_ll_debug:
			return "debug";
		case LogLevel::dx_ll_info:
			return "info";
		case LogLevel::dx_ll_warn:
			return "warn";
		case LogLevel::dx_ll_error:
			return "error";
	}

	return "unknown";
}

template <typename LogLevel>
inline LogLevel stringToLoggingLevel(const std::string& s) {
	if (iEquals(s, std::string("trace"))) return LogLevel::dx_ll_trace;
	if (iEquals(s, std::string("debug"))) return LogLevel::dx_ll_debug;
	if (iEquals(s, std::string("info"))) return LogLevel::dx_ll_info;
	if (iEquals(s, std::string("warn"))) return LogLevel::dx_ll_warn;
	if (iEquals(s, std::string("error"))) return LogLevel::dx_ll_error;

	return LogLevel::dx_ll_info;
}

//TODO: implement
inline std::string hideCredentials(const std::string& str) {
	return str;
}

}  // namespace StringUtils

}  // namespace dx