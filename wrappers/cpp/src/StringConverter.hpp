#pragma once

#include <locale>
#include <string>

struct StringConverter {
	static std::string toString(const std::wstring &wstring) {
		return std::string(wstring.begin(), wstring.end());
	}

	static std::wstring toWString(const std::string &string) {
		return std::wstring(string.begin(), string.end());
	}
};



