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

#include <DXFeed.h>
#include <string>
#include <nonstd/string_view.hpp>
#include <fmt/format.h>
#include "StringConverter.hpp"

struct Error {
	const int code;
	const nonstd::wstring_view description;

	static Error getLast() {
		int code;
		dxf_const_string_t description;

		if (dxf_get_last_error(&code, &description) == DXF_SUCCESS) {
			return {code, description};
		}

		return {-1, nonstd::wstring_view()};
	}

	std::string toString() const {
		if (description.empty()) {
			return "";
		}

		return fmt::format("[{}] {}", code, StringConverter::toString(description.to_string()));
	}

	template <typename OutStream>
	friend OutStream& operator << (OutStream& os, const Error& error) {
		os << error.toString();

		return os;
	}
};
