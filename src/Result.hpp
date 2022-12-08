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

namespace dx {

// TODO: std::expected
class Result {
	std::string data_;

public:
	explicit Result(const std::string& data) noexcept : data_{data} {}

	virtual const std::string& what() const noexcept { return data_; }

	virtual ~Result() = default;

	virtual bool isOk() const { return data_ == "Ok"; }
};

struct Ok : Result {
	explicit Ok() noexcept : Result("Ok") {}
};

struct Error : Result {
	explicit Error(const std::string& error) noexcept : Result(error) {}
};

struct RuntimeError : Error {
	explicit RuntimeError(const std::string& error) noexcept : Error(error) {}
};

struct IllegalArgumentError : RuntimeError {
	explicit IllegalArgumentError(const std::string& error) noexcept : RuntimeError(error) {}
};

struct InvalidFormatError : IllegalArgumentError {
	explicit InvalidFormatError(const std::string& error) noexcept : IllegalArgumentError(error) {}
};

struct AddressSyntaxError : InvalidFormatError {
	explicit AddressSyntaxError(const std::string& error) noexcept : InvalidFormatError(error) {}
};

}  // namespace dx
