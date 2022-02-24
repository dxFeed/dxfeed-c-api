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

#include <string>
#include <boost/locale/encoding_utf.hpp>

namespace dx {

struct StringConverter {
	static std::wstring utf8ToWString(const std::string &utf8) noexcept {
		return boost::locale::conv::utf_to_utf<wchar_t>(utf8);
	}

	static std::wstring utf8ToWString(const char *utf8) noexcept {
		if (utf8 == nullptr) {
			return {};
		}

		return boost::locale::conv::utf_to_utf<wchar_t>(utf8);
	}

	static wchar_t utf8ToWChar(char c) noexcept {
		if (c == '\0') {
			return L'\0';
		}

		return utf8ToWString(std::string(1, c))[0];
	}

	static std::string wStringToUtf8(const std::wstring &utf16) noexcept {
		return boost::locale::conv::utf_to_utf<char>(utf16);
	}

	static std::string wStringToUtf8(const wchar_t *utf16) noexcept {
		if (utf16 == nullptr) {
			return {};
		}

		return boost::locale::conv::utf_to_utf<char>(utf16);
	}

	static char wCharToUtf8(wchar_t c) noexcept {
		if (c == L'\0') {
			return '\0';
		}

		return wStringToUtf8(std::wstring(1, c))[0];
	}
};

}
