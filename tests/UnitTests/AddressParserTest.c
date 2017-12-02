#include "AddressParserTest.h"
#include "DXAddressParser.h"
#include "DXAlgorithms.h"
#include "TestHelper.h"

/* To add TLS codec support for test add 'DXFEED_CODEC_TLS_ENABLED' string to
 * C/C++ compiller definition.
 *
 * Note: the DXFeed library also must support 'DXFEED_CODEC_TLS_ENABLED'
 * definition to successful test passing.
 */
#ifdef DXFEED_CODEC_TLS_ENABLED
#define WITH_TLS_RESULT true
#else
#define WITH_TLS_RESULT false
#endif

/* To add GZIP codec support for test add 'DXFEED_CODEC_GZIP_ENABLED' string to
 * C/C++ compiller definition.
 *
 * Note: the DXFeed library also must support 'DXFEED_CODEC_GZIP_ENABLED'
 * definition to successful test passing.
 */
#ifdef DXFEED_CODEC_GZIP_ENABLED
#define WITH_GZIP_RESULT true
#else
#define WITH_GZIP_RESULT false
#endif

typedef struct {
	const char* collection;
	dx_address_array_t expected;
	bool result;
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
	{ "192.168.1.1", "4242", "xxx", "yyyy", { true, NULL, NULL, "C:/data/CA.pem", NULL }, { true } },
	//8 - tls[trustStore=C:/data/CA.pem,trustStorePassword=123]+192.168.1.1:4242
	{ "192.168.1.1", "4242", NULL, NULL, { true, NULL, NULL, "C:/data/CA.pem", "123" }, { false } },
	//9 - file:/path/to/file
	{ "file:/path/to/file", NULL, NULL, NULL, { false, NULL, NULL, NULL, NULL }, { false } },
	//10 - tls[trustStore=C:/data/CA.pem]+http://demo.dxfeed.com
	{ "http://demo.dxfeed.com", NULL, NULL, NULL, { true, NULL, NULL, "C:/data/CA.pem", NULL }, { false } },
	//11 - https://192.168.1.1:4242[username=xxx,password=yyyy]
	{ "https://192.168.1.1", "4242", "xxx", "yyyy", { false, NULL, NULL, NULL, NULL }, { false } },
	//12 - tls[trustStore=C:/data/CA.pem]+http://demo.dxfeed.com
	{ "http://demo.dxfeed.com", NULL, NULL, NULL, { true, NULL, NULL, "C:/data/CA.pem", NULL }, { false } },
	//13 - tls[trustStore=C:/data/CA.pem]+https://192.168.1.1:4242[username=xxx,password=yyyy]
	{ "https://192.168.1.1", "4242", "xxx", "yyyy", { true, NULL, NULL, "C:/data/CA.pem", NULL }, { false } }
};

static dx_test_case_t all_cases[] = {
	/* valid cases */
	{ "demo.dxfeed.com:7300",                                                               { &addresses[0], 1, 1 }, true },
	{ "192.168.1.1:4242",                                                                   { &addresses[1], 1, 1 }, true },
	{ "192.168.1.1:4242[username=xxx,password=yyyy]",                                       { &addresses[2], 1, 1 }, true },
	{ "192.168.1.1:4242[,username=xxx,,,password=yyyy,]",                                   { &addresses[2], 1, 1 }, true },
	{ "192.168.1.1:4242[username=xxx][password=yyyy]",                                      { &addresses[2], 1, 1 }, true },
	{ "192.168.1.1:4242[][username=xxx][][][password=yyyy][]",                              { &addresses[2], 1, 1 }, true },
	{ "tls+192.168.1.1:4242",                                                               { &addresses[3], 1, 1 }, WITH_TLS_RESULT },
	{ "ssl+192.168.1.1:4242",                                                               { &addresses[3], 1, 1 }, WITH_TLS_RESULT },
	{ "tls++192.168.1.1:4242",                                                              { &addresses[3], 1, 1 }, WITH_TLS_RESULT },
	{ "tls[]+192.168.1.1:4242",                                                             { &addresses[3], 1, 1 }, WITH_TLS_RESULT },
	{ "tls+gzip+192.168.1.1:4242",                                                          { &addresses[4], 1, 1 }, WITH_TLS_RESULT && WITH_GZIP_RESULT },
	{ "tls[trustStore=C:/data/CA.pem]+192.168.1.1:4242",                                    { &addresses[5], 1, 1 }, WITH_TLS_RESULT },
	{ "tls[trustStore=C:/data/CA.pem]+192.168.1.1:4242[username=xxx,password=yyyy]",        { &addresses[6], 1, 1 }, WITH_TLS_RESULT },
	{ "tls[trustStore=C:/data/CA.pem]+gzip+192.168.1.1:4242[username=xxx,password=yyyy]",   { &addresses[7], 1, 1 }, WITH_TLS_RESULT && WITH_GZIP_RESULT },
	{ "gzip+tls[trustStore=C:/data/CA.pem]+192.168.1.1:4242[username=xxx,password=yyyy]",   { &addresses[7], 1, 1 }, WITH_TLS_RESULT && WITH_GZIP_RESULT },
	{ "tls[trustStore=C:/data/CA.pem,trustStorePassword=123]+192.168.1.1:4242",             { &addresses[8], 1, 1 }, WITH_TLS_RESULT },
	{ "tls[,trustStore=C:/data/CA.pem,,,trustStorePassword=123,]+192.168.1.1:4242",         { &addresses[8], 1, 1 }, WITH_TLS_RESULT },
	{ "tls[trustStore=C:/data/CA.pem][trustStorePassword=123]+192.168.1.1:4242",            { &addresses[8], 1, 1 }, WITH_TLS_RESULT },
	{ "tls[][trustStore=C:/data/CA.pem][][][trustStorePassword=123][]+192.168.1.1:4242",    { &addresses[8], 1, 1 }, WITH_TLS_RESULT },
	{ "file:/path/to/file",                                                                 { &addresses[9], 1, 1 }, true },
	{ "tls[trustStore=C:/data/CA.pem]+http://demo.dxfeed.com",                              { &addresses[10], 1, 1 }, WITH_TLS_RESULT },
	{ "https://192.168.1.1:4242[username=xxx,password=yyyy]",                               { &addresses[11], 1, 1 }, true },
	{ "+demo.dxfeed.com:7300",                                                              { &addresses[0], 1, 1 }, true },
	{ "(demo.dxfeed.com:7300)",                                                             { &addresses[0], 1, 1 }, true },
	{ "(+demo.dxfeed.com:7300[])",                                                          { &addresses[0], 1, 1 }, true },
	{ "()(demo.dxfeed.com:7300)",                                                           { &addresses[0], 1, 1 }, true },
	{ "(demo.dxfeed.com:7300)(192.168.1.1:4242)",                                           { &addresses[0], 2, 2 }, true },
	{ "()(demo.dxfeed.com:7300)()()(192.168.1.1:4242)",                                     { &addresses[0], 2, 2 }, true },
	{ "(tls[trustStore=C:/data/CA.pem]+http://demo.dxfeed.com)(https://192.168.1.1:4242[username=xxx,password=yyyy])", { &addresses[10], 2, 2 }, WITH_TLS_RESULT },
	{ "(tls[trustStore=C:/data/CA.pem]+http://demo.dxfeed.com)(tls[trustStore=C:/data/CA.pem]+https://192.168.1.1:4242[username=xxx,password=yyyy])", { &addresses[12], 2, 2 }, WITH_TLS_RESULT },
	{ "(ssl[trustStore=C:/data/CA.pem]+http://demo.dxfeed.com)(tls[trustStore=C:/data/CA.pem]+https://192.168.1.1:4242[username=xxx,password=yyyy])", { &addresses[12], 2, 2 }, WITH_TLS_RESULT },

	/* invalid cases */
	{ "",                                                                                   DX_EMPTY_ARRAY, false },
	{ "()",                                                                                 DX_EMPTY_ARRAY, false },
	{ "(demo.dxfeed.com:7300)()",                                                           DX_EMPTY_ARRAY, false },
	{ "(demo.dxfeed.com:7300()",                                                            DX_EMPTY_ARRAY, false },
	{ "192.168.1.1:4242[username=xxx][password=yyyy][",                                     DX_EMPTY_ARRAY, false },
	{ "192.168.1.1:4242[username=xxx]password=yyyy]",                                       DX_EMPTY_ARRAY, false },
	{ "192.168.1.1:4242username=xxx][password=yyyy]",                                       DX_EMPTY_ARRAY, false },
	{ "192.168.1.1:4242[username=xxx,password]",                                            DX_EMPTY_ARRAY, false },
	{ "192.168.1.1:4242[username=xxx,password=]",                                           DX_EMPTY_ARRAY, false },
	{ "192.168.1.1:4242[username=xxx,password=yyyy,unknown=abc]",                           DX_EMPTY_ARRAY, false },
	{ "unknown+192.168.1.1:4242",                                                           DX_EMPTY_ARRAY, false },
	{ "tls[unknown=abc]+192.168.1.1:4242",                                                  DX_EMPTY_ARRAY, false },
	{ "tls[trustStore=C:/data/CA.pem][+192.168.1.1:4242",                                   DX_EMPTY_ARRAY, false },
	{ "tls[trustStore=C:/data/CA.pem]]+192.168.1.1:4242",                                   DX_EMPTY_ARRAY, false },
	{ "tls[[trustStore=C:/data/CA.pem]+192.168.1.1:4242",                                   DX_EMPTY_ARRAY, false },
	{ "tls][trustStore=C:/data/CA.pem]+192.168.1.1:4242",                                   DX_EMPTY_ARRAY, false },
	{ "tls[trustStore=C:/data/CA.pem,trustStorePassword]+192.168.1.1:4242",                 DX_EMPTY_ARRAY, false },
	{ "tls[trustStore=C:/data/CA.pem,trustStorePassword=]+192.168.1.1:4242",                DX_EMPTY_ARRAY, false },
};
static const size_t all_cases_count = SIZE_OF_ARRAY(all_cases);

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

/*
 * Test
 *
 * Tries to parse various addresses listed in all_cases.
 *
 * Expected: application shouldn't crash; all checks should be passed.
 */
static bool get_addresses_from_collection_test(void) {
	size_t i;
	for (i = 0; i < all_cases_count; i++) {
		dx_address_array_t actual = DX_EMPTY_ARRAY;
		dx_test_case_t c = all_cases[i];
		dx_pop_last_error();
		DX_CHECK_MESSAGE(dx_is_equal_bool(c.result, dx_get_addresses_from_collection(c.collection, &actual)), c.collection);
		if (c.result)
			DX_CHECK(dx_is_equal_address_array((const dx_address_array_t*)&c.expected, (const dx_address_array_t*)&actual));
		dx_clear_address_array(&actual);
	}
	return true;
}

/* -------------------------------------------------------------------------- */

bool address_parser_all_tests(void) {
	bool res = true;

	if (!get_addresses_from_collection_test()) {

		res = false;
	}
	return res;
}