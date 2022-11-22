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

extern "C" {
#include "AddressesManager.h"
}

#include "AddressesManager.hpp"

extern "C" {

void dx_addresses_manager_get_next_address(dxf_connection_t connection) {
	auto s = dx::AddressesManager::getInstance()->getNextSocketAddress(connection);
	const auto a = dx::AddressesManager::getInstance()->getCurrentAddress(connection);
	auto s2 = dx::AddressesManager::getInstance()->getNextSocketAddress(connection);
	const auto a2 = dx::AddressesManager::getInstance()->getCurrentAddress(connection);
	auto s3 = dx::AddressesManager::getInstance()->getNextSocketAddress(connection);
	const auto a3 = dx::AddressesManager::getInstance()->getCurrentAddress(connection);
}

}