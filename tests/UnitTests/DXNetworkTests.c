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

#include <stdio.h>

#include "ConnectionContextData.h"
#include "DXNetworkTests.h"
#include "DXAlgorithms.h"
#include "DXNetwork.h"
#include "DXProperties.h"
#include "TestHelper.h"

static int dx_is_equal_property_map(const dx_property_map_t* expected, const dx_property_map_t* actual) {
	size_t i;

	DX_CHECK(dx_is_equal_size_t(expected->size, actual->size));

	for (i = 0; i < expected->size; i++) {
		DX_CHECK(dx_is_equal_dxf_string_t(expected->elements[i].key, actual->elements[i].key));
		DX_CHECK(dx_is_equal_dxf_string_t(expected->elements[i].value, actual->elements[i].value));
	}
	return true;
}

/* -------------------------------------------------------------------------- */

/*
 * Test
 *
 * Tries to combine basic authorization data using user and password strings.
 *
 * Expected: application shouldn't crash; all checks should be passed;
 */
int protocol_get_basic_auth_data_test(void) {
	DX_CHECK(dx_is_equal_ansi("eHh4Onl5eXk=", dx_protocol_get_basic_auth_data("xxx", "yyyy")));
	return true;
}

/* -------------------------------------------------------------------------- */

/*
 * Test
 *
 * Tries to configure basic authorization using user and password strings.
 *
 * Expected: application shouldn't crash; all checks should be passed;
 */
int protocol_configure_basic_auth_test(void) {
	dx_property_item_t expected_array[] = {
		{ L"authorization", L"Basic eHh4Onl5eXk=" }
	};
	const size_t expected_size = SIZE_OF_ARRAY(expected_array);
	dx_property_map_t expected = { expected_array, expected_size, expected_size };
	dxf_connection_t connection = dx_init_connection();

	DX_CHECK(dx_is_true(dx_protocol_configure_basic_auth(connection, "xxx", "yyyy")));

	DX_CHECK(dx_is_equal_property_map(&expected, dx_protocol_property_get_all(connection)));

	dx_deinit_connection(connection);
	return true;
}

/* -------------------------------------------------------------------------- */

/*
 * Test
 *
 * Tries to configure custom authorization using token string.
 *
 * Expected: application shouldn't crash; all checks should be passed;
 */
int protocol_configure_custom_auth_test(void) {
	dx_property_item_t expected_array[] = {
		{ L"authorization", L"Bearer 123" }
	};
	const size_t expected_size = SIZE_OF_ARRAY(expected_array);
	dx_property_map_t expected = { expected_array, expected_size, expected_size };
	dxf_connection_t connection = dx_init_connection();

	DX_CHECK(dx_is_true(dx_protocol_configure_custom_auth(connection, "Bearer", "123")));

	DX_CHECK(dx_is_equal_property_map(&expected, dx_protocol_property_get_all(connection)));

	dx_deinit_connection(connection);
	return true;
}

/* -------------------------------------------------------------------------- */

int network_all_test(void) {
	int res = true;

	if (!protocol_get_basic_auth_data_test() ||
		!protocol_configure_basic_auth_test() ||
		!protocol_configure_custom_auth_test()) {

		res = false;
	}
	return res;
}