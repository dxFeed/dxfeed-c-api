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