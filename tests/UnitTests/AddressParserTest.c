#include "AddressParserTest.h"
#include "DXAddressParser.h"
#include "TestHelper.h"

typedef struct {
    const char* collection;
    dx_address_array_t expected;
} dx_test_case_t;

static dx_address_t addresses[] = {
    //0 - demo.dxfeed.com:7300
    { "demo.dxfeed.com", "7300", NULL, NULL, { false, NULL, NULL, NULL, NULL }, { false } },
    //1 - 192.168.1.1:4242
    { "192.168.1.1", "4242", NULL, NULL, { false, NULL, NULL, NULL, NULL }, { false } },
    //2 - 192.168.1.1:4242[username=xxx,password=yyyy]
    { "192.168.1.1", "4242", "xxx", "yyyy", { false, NULL, NULL, NULL, NULL }, { false } },
    //3 - tls+192.168.1.1:4242
    { "192.168.1.1", "4242", NULL, NULL, { true, NULL, NULL, NULL, NULL }, { false } },
    //4 - tls+gzip+192.168.1.1:4242
    { "192.168.1.1", "4242", NULL, NULL, { true, NULL, NULL, NULL, NULL }, { true } },
    //5 - tls[trustStore=C:/data/CA.pem]+192.168.1.1:4242
    { "192.168.1.1", "4242", NULL, NULL, { true, NULL, NULL, "C:/data/CA.pem", NULL }, { false } },
    //6 - tls[trustStore=C:/data/CA.pem]+192.168.1.1:4242[username=xxx,password=yyyy]
    { "192.168.1.1", "4242", "xxx", "yyyy", { true, NULL, NULL, "C:/data/CA.pem", NULL }, { false } },
    //7 - tls[trustStore=C:/data/CA.pem]+gzip+192.168.1.1:4242[username=xxx,password=yyyy]
    //    or
    //    gzip+tls[trustStore=C:/data/CA.pem]+192.168.1.1:4242[username=xxx,password=yyyy]
    { "192.168.1.1", "4242", "xxx", "yyyy", { true, NULL, NULL, "C:/data/CA.pem", NULL }, { true } },
};

static dx_test_case_t valid_cases[] = {
    { "demo.dxfeed.com:7300",                                                               { &addresses[0], 1, 1 } },
    { "192.168.1.1:4242",                                                                   { &addresses[1], 1, 1 } },
    { "192.168.1.1:4242[username=xxx,password=yyyy]",                                       { &addresses[2], 1, 1 } },
    { "tls+192.168.1.1:4242",                                                               { &addresses[3], 1, 1 } },
    { "tls+gzip+192.168.1.1:4242",                                                          { &addresses[4], 1, 1 } },
    { "tls[trustStore=C:/data/CA.pem]+192.168.1.1:4242",                                    { &addresses[5], 1, 1 } },
    { "tls[trustStore=C:/data/CA.pem]+192.168.1.1:4242[username=xxx,password=yyyy]",        { &addresses[6], 1, 1 } },
    { "tls[trustStore=C:/data/CA.pem]+gzip+192.168.1.1:4242[username=xxx,password=yyyy]",   { &addresses[7], 1, 1 } },
    { "gzip+tls[trustStore=C:/data/CA.pem]+192.168.1.1:4242[username=xxx,password=yyyy]",   { &addresses[7], 1, 1 } },
    { "(demo.dxfeed.com:7300)",                                                             { &addresses[0], 1, 1 } },
    { "()(demo.dxfeed.com:7300)",                                                           { &addresses[0], 1, 1 } },
    { "(demo.dxfeed.com:7300)()",                                                           { &addresses[0], 1, 1 } },
    { "(demo.dxfeed.com:7300)(192.168.1.1:4242)",                                           { &addresses[0], 2, 2 } },
    { "(demo.dxfeed.com:7300)()(192.168.1.1:4242)",                                         { &addresses[0], 2, 2 } },
};
static const size_t valid_cases_count = SIZE_OF_ARRAY(valid_cases);

/* -------------------------------------------------------------------------- */

static bool dx_is_equal_address(const dx_address_t* expected, const dx_address_t* actual) {
    DX_CHECK(dx_is_equal_ansi(expected->host, actual->host));
    DX_CHECK(dx_is_equal_const_ansi(expected->port, actual->port));
    DX_CHECK(dx_is_equal_ansi(expected->username, actual->username));
    DX_CHECK(dx_is_equal_ansi(expected->password, actual->password));

    DX_CHECK(dx_is_equal_bool(expected->tls.enabled, actual->tls.enabled));
    DX_CHECK(dx_is_equal_ansi(expected->tls.key_store, actual->tls.key_store));
    DX_CHECK(dx_is_equal_ansi(expected->tls.key_store_password, actual->tls.key_store_password));
    DX_CHECK(dx_is_equal_ansi(expected->tls.trust_store, actual->tls.trust_store));
    DX_CHECK(dx_is_equal_ansi(expected->tls.trust_store_password, actual->tls.trust_store_password));

    DX_CHECK(dx_is_equal_bool(expected->gzip.enabled, actual->gzip.enabled));
    return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_is_equal_address_array(const dx_address_array_t* expected, const dx_address_array_t* actual) {
    size_t i;
    DX_CHECK(dx_is_equal_size_t(expected->size, actual->size));
    for (i = 0; i < expected->size; i++) {
        DX_CHECK(dx_is_equal_address((const dx_address_t*)&(expected->elements[i]), (const dx_address_t*)&(actual->elements[i])));
    }
    return true;
}

/* -------------------------------------------------------------------------- */
//TODO:
/*
 * Test
 *
 * Tries to calculate a future base64 string length and compare it with etalon value.
 *
 * Expected: application shouldn't crash; all checks should be passed.
 */
static bool get_addresses_from_collection_test(void) {
    size_t i;
    for (i = 0; i < valid_cases_count; i++) {
        dx_address_array_t actual;
        dx_test_case_t c = valid_cases[i];
        DX_CHECK(dx_is_true(dx_get_addresses_from_collection(c.collection, &actual)));
        DX_CHECK(dx_is_equal_address_array((const dx_address_array_t*)&c.expected, (const dx_address_array_t*)&actual));
        dx_clear_address_array(&actual);
    }
    return true;
}
//TODO:
/*
 * Test
 *
 * Tries to calculate a future base64 string length and compare it with etalon value.
 *
 * Expected: application shouldn't crash; all checks should be passed.
 */
static bool bad_addresses_collection_test(void) {

    return false;
}

/* -------------------------------------------------------------------------- */

bool address_parser_all_tests(void) {
    bool res = true;

    if (!get_addresses_from_collection_test() ||
        !bad_addresses_collection_test()) {

        res = false;
    }
    return res;
}