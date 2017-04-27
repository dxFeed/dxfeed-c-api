#include <stdio.h>

#include "ConnectionContextData.h"
#include "DataCollectionsTests.h"
#include "DXNetwork.h"
#include "TestHelper.h"

static dx_property_item_t expected_props_list[] = {
    { L"a", L"A" },
    { L"b", L"B" },
    { L"c", L"C" }
};
static size_t expected_props_size = SIZE_OF_ARRAY(expected_props_list);

/*
 * Test
 *
 * Tries to create context with properties map.
 * Tries to set NULL key-value pairs.
 * Tries to destroy context.
 *
 * Expected: application shouldn't crash; all checks should be passed.
 */
bool protocol_properties_set_null_test(void) {
    dxf_connection_t connection = dx_init_connection();

    DX_CHECK(dx_is_false(dx_protocol_property_set(connection, NULL, NULL)));
    DX_CHECK(dx_is_false(dx_protocol_property_set(connection, NULL, L"A")));
    DX_CHECK(dx_is_false(dx_protocol_property_set(connection, L"a", NULL)));

    dx_deinit_connection(connection);
    return true;
}

/* -------------------------------------------------------------------------- */

/*
 * Test
 *
 * Tries to create context with properties map.
 * Tries to set empty key-value pairs.
 * Tries to destroy context.
 *
 * Expected: application shouldn't crash; all checks should be passed.
 */
bool protocol_properties_set_empty_test(void) {
    dxf_connection_t connection = dx_init_connection();

    DX_CHECK(dx_is_false(dx_protocol_property_set(connection, L"", L"")));

    dx_deinit_connection(connection);
    return true;
}

/* -------------------------------------------------------------------------- */

/*
 * Test
 *
 * Tries to create context with properties map.
 * Tries to fill protocol properties set.
 * Compare properties set with expected values.
 * Tries to destroy context.
 *
 * Expected: application shouldn't crash; all checks should be passed.
 */
bool protocol_properties_get_all_test(void) {
    dxf_connection_t connection = dx_init_connection();
    size_t i;
    size_t size = expected_props_size;
    const dx_property_map_t* actual;

    for (i = size; i > 0; i--)
        DX_CHECK(dx_is_true(dx_protocol_property_set(connection, expected_props_list[i - 1].key, expected_props_list[i - 1].value)));

    actual = dx_protocol_property_get_all(connection);
    DX_CHECK(dx_is_equal_size_t(size, actual->size));

    for (i = 0; i < size; i++) {
        DX_CHECK(dx_is_equal_dxf_string_t(expected_props_list[i].key, actual->elements[i].key));
        DX_CHECK(dx_is_equal_dxf_string_t(expected_props_list[i].value, actual->elements[i].value));
    }

    dx_deinit_connection(connection);
    return true;
}

/* -------------------------------------------------------------------------- */

/*
 * Test
 *
 * Tries to create context with properties map.
 * Tries to fill protocol properties set.
 * Tries to change value.
 * Tries to set the same key and value pair.
 * Tries to destroy context.
 *
 * Expected: application shouldn't crash; all checks should be passed.
 */
bool protocol_properties_set_change_test(void) {
    dxf_connection_t connection = dx_init_connection();
    size_t i;
    size_t size = expected_props_size;
    const dx_property_map_t* actual;

    for (i = size; i > 0; i--)
        DX_CHECK(dx_is_true(dx_protocol_property_set(connection, expected_props_list[i - 1].key, expected_props_list[i - 1].value)));

    DX_CHECK(dx_is_true(dx_protocol_property_set(connection, L"b", L"DDD")));
    // duplicate set
    DX_CHECK(dx_is_true(dx_protocol_property_set(connection, L"b", L"DDD")));

    dx_property_item_t expected[] = {
        { L"a", L"A" },
        { L"b", L"DDD" },
        { L"c", L"C" }
    };
    actual = dx_protocol_property_get_all(connection);
    DX_CHECK(dx_is_equal_size_t(size, actual->size));

    for (i = 0; i < size; i++) {
        DX_CHECK(dx_is_equal_dxf_string_t(expected[i].key, actual->elements[i].key));
        DX_CHECK(dx_is_equal_dxf_string_t(expected[i].value, actual->elements[i].value));
    }

    dx_deinit_connection(connection);
    return true;
}

/* -------------------------------------------------------------------------- */

bool data_collections_all_test(void) {
    bool res = true;

    if (!protocol_properties_set_null_test() ||
        !protocol_properties_set_empty_test() ||
        !protocol_properties_get_all_test() ||
        !protocol_properties_set_change_test()) {

        res = false;
    }
    return res;
}