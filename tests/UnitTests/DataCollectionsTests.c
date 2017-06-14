#include <stdio.h>

#include "ConnectionContextData.h"
#include "DataCollectionsTests.h"
#include "DXNetwork.h"
#include "TestHelper.h"

static dx_property_item_t source_props_array[] = {
    { L"a", L"A" },
    { L"b", L"B" },
    { L"c", L"C" }
};
static size_t source_props_size = SIZE_OF_ARRAY(source_props_array);
static dx_property_map_t source = { source_props_array, SIZE_OF_ARRAY(source_props_array), SIZE_OF_ARRAY(source_props_array) };

/* -------------------------------------------------------------------------- */

static bool dx_is_equal_property_map(const dx_property_map_t* expected, const dx_property_map_t* actual) {
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
    size_t size = source_props_size;

    for (i = size; i > 0; i--)
        DX_CHECK(dx_is_true(dx_protocol_property_set(connection, source_props_array[i - 1].key, source_props_array[i - 1].value)));

    DX_CHECK(dx_is_equal_property_map(&source, dx_protocol_property_get_all(connection)));

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
    size_t size = source_props_size;

    for (i = size; i > 0; i--)
        DX_CHECK(dx_is_true(dx_protocol_property_set(connection, source_props_array[i - 1].key, source_props_array[i - 1].value)));

    DX_CHECK(dx_is_true(dx_protocol_property_set(connection, L"b", L"DDD")));
    // duplicate set
    DX_CHECK(dx_is_true(dx_protocol_property_set(connection, L"b", L"DDD")));

    dx_property_item_t expected_array[] = {
        { L"a", L"A" },
        { L"b", L"DDD" },
        { L"c", L"C" }
    };
    size_t expected_size = SIZE_OF_ARRAY(expected_array);
    dx_property_map_t expected_map = { expected_array, expected_size, expected_size };

    DX_CHECK(dx_is_equal_property_map(&expected_map, dx_protocol_property_get_all(connection)));

    dx_deinit_connection(connection);
    return true;
}

/* -------------------------------------------------------------------------- */

/*
 * Test
 *
 * Creates context with properties map.
 * Fills protocol properties set.
 * Tries to set multiple key-value pairs into protocol properties.
 * Destroys context.
 *
 * Expected: application shouldn't crash; all checks should be passed.
*/
bool protocol_properties_set_many_test(void) {
    dxf_connection_t connection = dx_init_connection();
    size_t i;
    dx_property_item_t other_array[] = {
        { L"b", L"EEE" },
        { L"c", L"FFF" },
        { L"d", L"DDD" }
    };
    size_t other_size = SIZE_OF_ARRAY(other_array);
    dx_property_map_t other_map = { other_array, other_size, other_size };
    dx_property_item_t expected_array[] = {
        { L"a", L"A" },
        { L"b", L"EEE" },
        { L"c", L"FFF" },
        { L"d", L"DDD" }
    };
    size_t expected_size = SIZE_OF_ARRAY(expected_array);
    dx_property_map_t expected_map = { expected_array, expected_size, expected_size };

    for (i = source_props_size; i > 0; i--)
        DX_CHECK(dx_is_true(dx_protocol_property_set(connection, source_props_array[i - 1].key, source_props_array[i - 1].value)));

    DX_CHECK(dx_is_true(dx_protocol_property_set_many(connection, &other_map)));

    DX_CHECK(dx_is_equal_property_map(&expected_map, dx_protocol_property_get_all(connection)));

    dx_deinit_connection(connection);
    return true;
}

/* -------------------------------------------------------------------------- */

/*
 * Test
 *
 * Creates context with properties map.
 * Fills protocol properties set.
 * Tries to set multiple key-value pairs having invalid values into protocol properties.
 * Destroys context.
 *
 * Expected: application shouldn't crash; all checks should be passed; the 
 * source properties set shouldn't modified.
 */
bool protocol_properties_set_many_invalid_test(void) {
    dxf_connection_t connection = dx_init_connection();
    size_t i;
    dx_property_item_t other_array[] = {
        { L"b", L"EEE" },
        { L"c", L"FFF" },
        { L"d", NULL }
    };
    size_t other_size = SIZE_OF_ARRAY(other_array);
    dx_property_map_t other_map = { other_array, other_size, other_size };

    for (i = source_props_size; i > 0; i--)
        DX_CHECK(dx_is_true(dx_protocol_property_set(connection, source_props_array[i - 1].key, source_props_array[i - 1].value)));

    DX_CHECK(dx_is_false(dx_protocol_property_set_many(connection, &other_map)));

    DX_CHECK(dx_is_equal_property_map(&source, dx_protocol_property_get_all(connection)));

    dx_deinit_connection(connection);
    return true;
}

/* -------------------------------------------------------------------------- */

/*
 * Test
 *
 * Creates context with properties map.
 * Fills protocol properties set.
 * Tries to check map contains elements.
 * Destroys context.
 *
 * Expected: application shouldn't crash; all checks should be passed;
 */
bool protocol_properties_contains_test(void) {
    dxf_connection_t connection = dx_init_connection();
    size_t i;

    for (i = source_props_size; i > 0; i--)
        DX_CHECK(dx_is_true(dx_protocol_property_set(connection, source_props_array[i - 1].key, source_props_array[i - 1].value)));

    DX_CHECK(dx_is_true(dx_protocol_property_contains(connection, L"a")));
    DX_CHECK(dx_is_true(dx_protocol_property_contains(connection, L"b")));
    DX_CHECK(dx_is_true(dx_protocol_property_contains(connection, L"c")));
    DX_CHECK(dx_is_false(dx_protocol_property_contains(connection, L"d")));

    dx_protocol_property_set(connection, L"d", L"DDD");
    DX_CHECK(dx_is_true(dx_protocol_property_contains(connection, L"d")));

    DX_CHECK(dx_is_false(dx_protocol_property_contains(connection, NULL)));
    DX_CHECK(dx_is_false(dx_protocol_property_contains(connection, L"")));

    dx_deinit_connection(connection);
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
bool protocol_get_basic_auth_data_test(void) {
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
bool protocol_configure_basic_auth_test(void) {
    dx_property_item_t expected_array[] = {
        { L"authorization", L"basic eHh4Onl5eXk=" }
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
bool protocol_configure_custom_auth_test(void) {
    dx_property_item_t expected_array[] = {
        { L"authorization", L"bearer 123" }
    };
    const size_t expected_size = SIZE_OF_ARRAY(expected_array);
    dx_property_map_t expected = { expected_array, expected_size, expected_size };
    dxf_connection_t connection = dx_init_connection();

    DX_CHECK(dx_is_true(dx_protocol_configure_custom_auth(connection, "bearer", "123")));

    DX_CHECK(dx_is_equal_property_map(&expected, dx_protocol_property_get_all(connection)));

    dx_deinit_connection(connection);
    return true;
}

/* -------------------------------------------------------------------------- */

bool data_collections_all_test(void) {
    bool res = true;

    if (!protocol_properties_set_null_test() ||
        !protocol_properties_set_empty_test() ||
        !protocol_properties_get_all_test() ||
        !protocol_properties_set_change_test() ||
        !protocol_properties_set_many_test() ||
        !protocol_properties_set_many_invalid_test() ||
        !protocol_properties_contains_test() ||
        !protocol_get_basic_auth_data_test() ||
        !protocol_configure_basic_auth_test() ||
        !protocol_configure_custom_auth_test()) {

        res = false;
    }
    return res;
}