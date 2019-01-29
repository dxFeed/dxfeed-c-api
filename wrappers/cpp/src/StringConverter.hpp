#pragma once

#include <locale>
#include <codecvt>
#include <string>

struct StringConverter {
	static std::string toString(const std::wstring &wstring) {
		static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		return converter.to_bytes(wstring);
	}

	static std::wstring toWString(const std::string &string) {
		static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		return converter.from_bytes(string);
	}
};



