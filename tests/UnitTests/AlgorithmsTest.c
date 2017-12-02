#include <stdio.h>

#include "AlgorithmsTest.h"
#include "DXAlgorithms.h"
#include "DXProperties.h"
#include "TestHelper.h"

typedef struct {
	const char* source;
	const char* encoded;
} base64_test_case;

typedef struct {
	size_t base64_size;
	char* base64_buf;
	size_t decoded_size;
	char* decoded_buf;
} base64_test_context;

static base64_test_case base64_cases[] = {
	{ "demo:demo", "ZGVtbzpkZW1v" },
	{ "demo:demo1", "ZGVtbzpkZW1vMQ==" },
	{ "demo:demo12", "ZGVtbzpkZW1vMTI=" },
	{ "demo:demo123", "ZGVtbzpkZW1vMTIz" },
	{ "demo:demo1234", "ZGVtbzpkZW1vMTIzNA==" }
};

static size_t base64_cases_count = SIZE_OF_ARRAY(base64_cases);

static void base64_free_context(base64_test_context* context) {
	if (context->base64_buf != NULL)
		dx_free(context->base64_buf);
	if (context->decoded_buf != NULL)
		dx_free(context->decoded_buf);
	context->base64_buf = NULL;
	context->decoded_buf = NULL;
	context->base64_size = 0;
	context->decoded_size = 0;
}

/*
 * Test
 *
 * Tries to calculate a future base64 string length and compare it with etalon value.
 *
 * Expected: application shouldn't crash; all checks should be passed.
 */
static bool algorithms_base64_length_test(void) {
	size_t i;
	for (i = 0; i < base64_cases_count; i++) {
		size_t expected = strlen(base64_cases[i].encoded);
		size_t actual = dx_base64_length(strlen(base64_cases[i].source));
		DX_CHECK(dx_is_equal_size_t(expected, actual));
	}
	return true;
}

/* -------------------------------------------------------------------------- */

static bool base64_run_case(const base64_test_case* test_case, base64_test_context* context) {
	size_t source_size = strlen(test_case->source);
	int i;
	for (i = 0; i < 2; i++) {
		base64_free_context(context);
		switch (i) {
			case 0:
				//allocate calculated memory
				context->base64_size = dx_base64_length(source_size);
				break;
			default:
				//allocate redundant memory
				context->base64_size = dx_base64_length(source_size) + 10;
				break;
		}
		context->base64_buf = dx_ansi_create_string(context->base64_size);
		context->decoded_size = context->base64_size;
		context->decoded_buf = dx_ansi_create_string(context->decoded_size);
		//encoding
		DX_CHECK(dx_is_true(dx_base64_encode(test_case->source, source_size, context->base64_buf, context->base64_size)));
		DX_CHECK(dx_is_equal_int(0, strcmp(context->base64_buf, test_case->encoded)));
		//decoding
		DX_CHECK(dx_is_true(dx_base64_decode((const char*)context->base64_buf, strlen(context->base64_buf), context->decoded_buf, &context->decoded_size)));
		DX_CHECK(dx_is_equal_int(0, strcmp(context->decoded_buf, test_case->source)));
	}
	base64_free_context(context);
	return true;
}

/*
 * Test
 *
 * Tries to encode and decode various cases of string.
 *
 * Expected: application shouldn't crash; all checks should be passed.
 */
static bool algorithms_base64_encode_decode_test(void) {
	size_t i;
	base64_test_context context = { 0 };
	for (i = 0; i < base64_cases_count; i++) {
		bool res = base64_run_case((const base64_test_case*)&base64_cases[i], &context);
		base64_free_context(&context);
		DX_CHECK(res);
	}
	return true;
}

/* -------------------------------------------------------------------------- */

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
bool properties_map_set_null_test(void) {
	dx_property_map_t props = { 0 };

	DX_CHECK(dx_is_false(dx_property_map_set(&props, NULL, NULL)));
	DX_CHECK(dx_is_false(dx_property_map_set(&props, NULL, L"A")));
	DX_CHECK(dx_is_false(dx_property_map_set(&props, L"a", NULL)));

	dx_property_map_free_collection(&props);
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
bool properties_map_set_empty_test(void) {
	dx_property_map_t props = { 0 };

	DX_CHECK(dx_is_false(dx_property_map_set(&props, L"", L"")));
	DX_CHECK(dx_is_false(dx_property_map_set(&props, L"", L"A")));
	DX_CHECK(dx_is_true(dx_property_map_set(&props, L"a", L"")));

	dx_property_map_free_collection(&props);
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
bool properties_map_get_all_test(void) {
	dx_property_map_t props = { 0 };
	size_t i;
	size_t size = source_props_size;

	for (i = size; i > 0; i--)
		DX_CHECK(dx_is_true(dx_property_map_set(&props, source_props_array[i - 1].key, source_props_array[i - 1].value)));

	DX_CHECK(dx_is_equal_property_map(&source, &props));

	dx_property_map_free_collection(&props);
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
bool properties_map_set_change_test(void) {
	dx_property_map_t props = { 0 };
	size_t i;
	size_t size = source_props_size;

	for (i = size; i > 0; i--)
		DX_CHECK(dx_is_true(dx_property_map_set(&props, source_props_array[i - 1].key, source_props_array[i - 1].value)));

	DX_CHECK(dx_is_true(dx_property_map_set(&props, L"b", L"DDD")));
	// duplicate set
	DX_CHECK(dx_is_true(dx_property_map_set(&props, L"b", L"DDD")));

	dx_property_item_t expected_array[] = {
		{ L"a", L"A" },
		{ L"b", L"DDD" },
		{ L"c", L"C" }
	};
	size_t expected_size = SIZE_OF_ARRAY(expected_array);
	dx_property_map_t expected_map = { expected_array, expected_size, expected_size };

	DX_CHECK(dx_is_equal_property_map(&expected_map, &props));

	dx_property_map_free_collection(&props);
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
bool properties_map_set_many_test(void) {
	dx_property_map_t props = { 0 };
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
		DX_CHECK(dx_is_true(dx_property_map_set(&props, source_props_array[i - 1].key, source_props_array[i - 1].value)));

	DX_CHECK(dx_is_true(dx_property_map_set_many(&props, &other_map)));

	DX_CHECK(dx_is_equal_property_map(&expected_map, &props));

	dx_property_map_free_collection(&props);
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
bool properties_map_set_many_invalid_test(void) {
	dx_property_map_t props = { 0 };
	size_t i;
	dx_property_item_t other_array[] = {
		{ L"b", L"EEE" },
		{ L"c", L"" },
		{ L"d", NULL }
	};
	size_t other_size = SIZE_OF_ARRAY(other_array);
	dx_property_map_t other_map = { other_array, other_size, other_size };

	for (i = source_props_size; i > 0; i--)
		DX_CHECK(dx_is_true(dx_property_map_set(&props, source_props_array[i - 1].key, source_props_array[i - 1].value)));

	DX_CHECK(dx_is_false(dx_property_map_set_many(&props, &other_map)));

	DX_CHECK(dx_is_equal_property_map(&source, &props));

	dx_property_map_free_collection(&props);
	return true;
}

/* -------------------------------------------------------------------------- */

/*
 * Test
 *
 * Creates context with properties map.
 * Checks containig on empty map.
 * Fills protocol properties set.
 * Tries to check map contains elements.
 * Destroys context.
 *
 * Expected: application shouldn't crash; all checks should be passed;
 */
bool properties_map_contains_test(void) {
	dx_property_map_t props = { 0 };
	size_t i;

	DX_CHECK(dx_is_false(dx_property_map_contains(&props, L"a")));

	for (i = source_props_size; i > 0; i--)
		DX_CHECK(dx_is_true(dx_property_map_set(&props, source_props_array[i - 1].key, source_props_array[i - 1].value)));

	DX_CHECK(dx_is_true(dx_property_map_contains(&props, L"a")));
	DX_CHECK(dx_is_true(dx_property_map_contains(&props, L"b")));
	DX_CHECK(dx_is_true(dx_property_map_contains(&props, L"c")));
	DX_CHECK(dx_is_false(dx_property_map_contains(&props, L"d")));

	dx_property_map_set(&props, L"d", L"DDD");
	DX_CHECK(dx_is_true(dx_property_map_contains(&props, L"d")));

	DX_CHECK(dx_is_false(dx_property_map_contains(&props, NULL)));
	DX_CHECK(dx_is_false(dx_property_map_contains(&props, L"")));

	dx_property_map_free_collection(&props);
	return true;
}

/* -------------------------------------------------------------------------- */

/*
 * Test
 *
 * Fills protocol properties set.
 * Tries to clone initial properties set.
 * Destroy initial properties set.
 * Check that copy is actual.
 * Make copy of another property map.
 * Check copy of another property map.
 * Destroy copy of another property map.
 *
 * Expected: application shouldn't crash; all checks should be passed;
 */
bool property_map_clone_test(void) {
	dx_property_map_t props = { 0 };
	dx_property_map_t copy = { 0 };
	size_t i;
	dx_property_item_t other_array[] = {
		{ L"b", L"EEE" },
		{ L"c", L"FFF" },
		{ L"d", L"DDD" }
	};
	size_t other_size = SIZE_OF_ARRAY(other_array);
	dx_property_map_t other_map = { other_array, other_size, other_size };

	for (i = source_props_size; i > 0; i--)
		DX_CHECK(dx_is_true(dx_property_map_set(&props, source_props_array[i - 1].key, source_props_array[i - 1].value)));

	DX_CHECK(dx_is_true(dx_property_map_clone(&props, &copy)));
	dx_property_map_free_collection(&props);
	DX_CHECK(dx_is_equal_property_map(&source, &copy));

	DX_CHECK(dx_is_true(dx_property_map_clone(&other_map, &copy)));
	DX_CHECK(dx_is_equal_property_map(&other_map, &copy));
	dx_property_map_free_collection(&copy);
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
bool property_map_try_get_value_test(void) {
	dx_property_map_t props = { 0 };
	size_t i;
	dxf_const_string_t value;

	DX_CHECK(dx_is_false(dx_property_map_try_get_value(&props, L"a", &value)));

	for (i = source_props_size; i > 0; i--)
		DX_CHECK(dx_is_true(dx_property_map_set(&props, source_props_array[i - 1].key, source_props_array[i - 1].value)));

	DX_CHECK(dx_is_true(dx_property_map_try_get_value(&props, L"a", &value)));
	DX_CHECK(dx_is_equal_dxf_const_string_t(L"A", value));

	DX_CHECK(dx_is_false(dx_property_map_try_get_value(&props, L"d", &value)));

	dx_property_map_free_collection(&props);
	return true;
}

/* -------------------------------------------------------------------------- */

bool algorithms_all_tests(void) {
	bool res = true;

	if (!algorithms_base64_length_test() ||
		!algorithms_base64_encode_decode_test() ||
		!properties_map_set_null_test() ||
		!properties_map_set_empty_test() ||
		!properties_map_get_all_test() ||
		!properties_map_set_change_test() ||
		!properties_map_set_many_test() ||
		!properties_map_set_many_invalid_test() ||
		!properties_map_contains_test() ||
		!property_map_clone_test() ||
		!property_map_try_get_value_test()) {

		res = false;
	}
	return res;
}