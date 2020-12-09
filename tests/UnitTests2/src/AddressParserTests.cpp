#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include "DXAddressParser.h"
#include "DXAlgorithms.h"

/* To add TLS codec support for test add 'DXFEED_CODEC_TLS_ENABLED' string to
 * C/C++ compiler definition.
 *
 * Note: the DXFeed library also must support 'DXFEED_CODEC_TLS_ENABLED'
 * definition to successful test passing.
 */
#ifdef DXFEED_CODEC_TLS_ENABLED
#	define WITH_TLS_RESULT true
#else
#	define WITH_TLS_RESULT false
#endif

/* To add GZIP codec support for test add 'DXFEED_CODEC_GZIP_ENABLED' string to
 * C/C++ compiler definition.
 *
 * Note: the DXFeed library also must support 'DXFEED_CODEC_GZIP_ENABLED'
 * definition to successful test passing.
 */
#ifdef DXFEED_CODEC_GZIP_ENABLED
#	define WITH_GZIP_RESULT true
#else
#	define WITH_GZIP_RESULT false
#endif

template <class T, std::size_t N>
constexpr std::size_t size(T (&)[N]) {
	return N;
}

namespace {
const char* ADDRESS = "demo.dxfeed.com";
const char* ADDRESS2 = "192.168.1.1";
const char* ADDRESS3 = "file:/path/to/file";
const char* ADDRESS4 = "http://demo.dxfeed.com";
const char* ADDRESS5 = "https://192.168.1.1";
const char* USERNAME = "xxx";
const char* PASSWORD = "yyyy";
const char* TRUST_STORE = "C:/data/CA.pem";
const char* TRUST_STORE_PASSWORD = "123";
const dx_address_array_t EMPTY_ARRAY = {nullptr, 0, 0};

typedef struct {
	const char* collection;
	dx_address_array_t expected;
	int result;
} dx_test_case_t;

dx_address_t addresses[] = {
	// 0 - demo.dxfeed.com:7300
	{const_cast<char*>(ADDRESS), "7300", nullptr, nullptr, {false, nullptr, nullptr, nullptr, nullptr}, {false}},
	// 1 - 192.168.1.1:4242
	{const_cast<char*>(ADDRESS2), "4242", nullptr, nullptr, {false, nullptr, nullptr, nullptr, nullptr}, {false}},
	// 2 - 192.168.1.1:4242[username=xxx,password=yyyy]
	{const_cast<char*>(ADDRESS2),
	 "4242",
	 const_cast<char*>(USERNAME),
	 const_cast<char*>(PASSWORD),
	 {false, nullptr, nullptr, nullptr, nullptr},
	 {false}},
	// 3 - tls+192.168.1.1:4242
	{const_cast<char*>(ADDRESS2), "4242", nullptr, nullptr, {true, nullptr, nullptr, nullptr, nullptr}, {false}},
	// 4 - tls+gzip+192.168.1.1:4242
	{const_cast<char*>(ADDRESS2), "4242", nullptr, nullptr, {true, nullptr, nullptr, nullptr, nullptr}, {true}},
	// 5 - tls[trustStore=C:/data/CA.pem]+192.168.1.1:4242
	{const_cast<char*>(ADDRESS2),
	 "4242",
	 nullptr,
	 nullptr,
	 {true, nullptr, nullptr, const_cast<char*>(TRUST_STORE), nullptr},
	 {false}},
	// 6 - tls[trustStore=C:/data/CA.pem]+192.168.1.1:4242[username=xxx,password=yyyy]
	{const_cast<char*>(ADDRESS2),
	 "4242",
	 const_cast<char*>(USERNAME),
	 const_cast<char*>(PASSWORD),
	 {true, nullptr, nullptr, const_cast<char*>(TRUST_STORE), nullptr},
	 {false}},
	// 7 - tls[trustStore=C:/data/CA.pem]+gzip+192.168.1.1:4242[username=xxx,password=yyyy]
	{const_cast<char*>(ADDRESS2),
	 "4242",
	 const_cast<char*>(USERNAME),
	 const_cast<char*>(PASSWORD),
	 {true, nullptr, nullptr, const_cast<char*>(TRUST_STORE), nullptr},
	 {true}},
	// 8 - tls[trustStore=C:/data/CA.pem,trustStorePassword=123]+192.168.1.1:4242
	{const_cast<char*>(ADDRESS2),
	 "4242",
	 nullptr,
	 nullptr,
	 {true, nullptr, nullptr, const_cast<char*>(TRUST_STORE), const_cast<char*>(TRUST_STORE_PASSWORD)},
	 {false}},
	// 9 - file:/path/to/file
	{const_cast<char*>(ADDRESS3), nullptr, nullptr, nullptr, {false, nullptr, nullptr, nullptr, nullptr}, {false}},
	// 10 - tls[trustStore=C:/data/CA.pem]+http://demo.dxfeed.com
	{const_cast<char*>(ADDRESS4),
	 nullptr,
	 nullptr,
	 nullptr,
	 {true, nullptr, nullptr, const_cast<char*>(TRUST_STORE), nullptr},
	 {false}},
	// 11 - https://192.168.1.1:4242[username=xxx,password=yyyy]
	{const_cast<char*>(ADDRESS5),
	 "4242",
	 const_cast<char*>(USERNAME),
	 const_cast<char*>(PASSWORD),
	 {false, nullptr, nullptr, nullptr, nullptr},
	 {false}},
	// 12 - tls[trustStore=C:/data/CA.pem]+http://demo.dxfeed.com
	{const_cast<char*>(ADDRESS4),
	 nullptr,
	 nullptr,
	 nullptr,
	 {true, nullptr, nullptr, const_cast<char*>(TRUST_STORE), nullptr},
	 {false}},
	// 13 - tls[trustStore=C:/data/CA.pem]+https://192.168.1.1:4242[username=xxx,password=yyyy]
	{const_cast<char*>(ADDRESS5),
	 "4242",
	 const_cast<char*>(USERNAME),
	 const_cast<char*>(PASSWORD),
	 {true, nullptr, nullptr, const_cast<char*>(TRUST_STORE), nullptr},
	 {false}}};

dx_test_case_t valid_cases[] = {
	{"demo.dxfeed.com:7300", {&addresses[0], 1, 1}, true},
	{"192.168.1.1:4242", {&addresses[1], 1, 1}, true},
	{"192.168.1.1:4242[username=xxx,password=yyyy]", {&addresses[2], 1, 1}, true},
	{"192.168.1.1:4242[,username=xxx,,,password=yyyy,]", {&addresses[2], 1, 1}, true},
	{"192.168.1.1:4242[username=xxx][password=yyyy]", {&addresses[2], 1, 1}, true},
	{"192.168.1.1:4242[][username=xxx][][][password=yyyy][]", {&addresses[2], 1, 1}, true},
	{"tls+192.168.1.1:4242", {&addresses[3], 1, 1}, WITH_TLS_RESULT},
	{"ssl+192.168.1.1:4242", {&addresses[3], 1, 1}, WITH_TLS_RESULT},
	{"tls++192.168.1.1:4242", {&addresses[3], 1, 1}, WITH_TLS_RESULT},
	{"tls[]+192.168.1.1:4242", {&addresses[3], 1, 1}, WITH_TLS_RESULT},
	{"tls+gzip+192.168.1.1:4242", {&addresses[4], 1, 1}, WITH_TLS_RESULT&& WITH_GZIP_RESULT},
	{"tls[trustStore=C:/data/CA.pem]+192.168.1.1:4242", {&addresses[5], 1, 1}, WITH_TLS_RESULT},
	{"tls[trustStore=C:/data/CA.pem]+192.168.1.1:4242[username=xxx,password=yyyy]",
	 {&addresses[6], 1, 1},
	 WITH_TLS_RESULT},
	{"tls[trustStore=C:/data/CA.pem]+gzip+192.168.1.1:4242[username=xxx,password=yyyy]",
	 {&addresses[7], 1, 1},
	 WITH_TLS_RESULT&& WITH_GZIP_RESULT},
	{"gzip+tls[trustStore=C:/data/CA.pem]+192.168.1.1:4242[username=xxx,password=yyyy]",
	 {&addresses[7], 1, 1},
	 WITH_TLS_RESULT&& WITH_GZIP_RESULT},
	{"tls[trustStore=C:/data/CA.pem,trustStorePassword=123]+192.168.1.1:4242", {&addresses[8], 1, 1}, WITH_TLS_RESULT},
	{"tls[,trustStore=C:/data/CA.pem,,,trustStorePassword=123,]+192.168.1.1:4242",
	 {&addresses[8], 1, 1},
	 WITH_TLS_RESULT},
	{"tls[trustStore=C:/data/CA.pem][trustStorePassword=123]+192.168.1.1:4242", {&addresses[8], 1, 1}, WITH_TLS_RESULT},
	{"tls[][trustStore=C:/data/CA.pem][][][trustStorePassword=123][]+192.168.1.1:4242",
	 {&addresses[8], 1, 1},
	 WITH_TLS_RESULT},
	{"file:/path/to/file", {&addresses[9], 1, 1}, true},
	{"tls[trustStore=C:/data/CA.pem]+http://demo.dxfeed.com", {&addresses[10], 1, 1}, WITH_TLS_RESULT},
	{"https://192.168.1.1:4242[username=xxx,password=yyyy]", {&addresses[11], 1, 1}, true},
	{"+demo.dxfeed.com:7300", {&addresses[0], 1, 1}, true},
	{"(demo.dxfeed.com:7300)", {&addresses[0], 1, 1}, true},
	{"(+demo.dxfeed.com:7300[])", {&addresses[0], 1, 1}, true},
	{"()(demo.dxfeed.com:7300)", {&addresses[0], 1, 1}, true},
	{"(demo.dxfeed.com:7300)(192.168.1.1:4242)", {&addresses[0], 2, 2}, true},
	{"()(demo.dxfeed.com:7300)()()(192.168.1.1:4242)", {&addresses[0], 2, 2}, true},
	{"(tls[trustStore=C:/data/CA.pem]+http://demo.dxfeed.com)(https://192.168.1.1:4242[username=xxx,password=yyyy])",
	 {&addresses[10], 2, 2},
	 WITH_TLS_RESULT},
	{"(tls[trustStore=C:/data/CA.pem]+http://demo.dxfeed.com)(tls[trustStore=C:/data/CA.pem]+https://"
	 "192.168.1.1:4242[username=xxx,password=yyyy])",
	 {&addresses[12], 2, 2},
	 WITH_TLS_RESULT},
	{"(ssl[trustStore=C:/data/CA.pem]+http://demo.dxfeed.com)(tls[trustStore=C:/data/CA.pem]+https://"
	 "192.168.1.1:4242[username=xxx,password=yyyy])",
	 {&addresses[12], 2, 2},
	 WITH_TLS_RESULT},
};
dx_test_case_t invalid_cases[] = {
	{"", DX_EMPTY_ARRAY, false},
	{"()", DX_EMPTY_ARRAY, false},
	{"(demo.dxfeed.com:7300)()", DX_EMPTY_ARRAY, false},
	{"(demo.dxfeed.com:7300()", DX_EMPTY_ARRAY, false},
	{"192.168.1.1:4242[username=xxx][password=yyyy][", DX_EMPTY_ARRAY, false},
	{"192.168.1.1:4242[username=xxx]password=yyyy]", DX_EMPTY_ARRAY, false},
	{"192.168.1.1:4242username=xxx][password=yyyy]", DX_EMPTY_ARRAY, false},
	{"192.168.1.1:4242[username=xxx,password]", DX_EMPTY_ARRAY, false},
	{"192.168.1.1:4242[username=xxx,password=]", DX_EMPTY_ARRAY, false},
	{"192.168.1.1:4242[username=xxx,password=yyyy,unknown=abc]", DX_EMPTY_ARRAY, false},
	{"unknown+192.168.1.1:4242", DX_EMPTY_ARRAY, false},
	{"tls[unknown=abc]+192.168.1.1:4242", DX_EMPTY_ARRAY, false},
	{"tls[trustStore=C:/data/CA.pem][+192.168.1.1:4242", DX_EMPTY_ARRAY, false},
	{"tls[trustStore=C:/data/CA.pem]]+192.168.1.1:4242", DX_EMPTY_ARRAY, false},
	{"tls[[trustStore=C:/data/CA.pem]+192.168.1.1:4242", DX_EMPTY_ARRAY, false},
	{"tls][trustStore=C:/data/CA.pem]+192.168.1.1:4242", DX_EMPTY_ARRAY, false},
	{"tls[trustStore=C:/data/CA.pem,trustStorePassword]+192.168.1.1:4242", DX_EMPTY_ARRAY, false},
	{"tls[trustStore=C:/data/CA.pem,trustStorePassword=]+192.168.1.1:4242", DX_EMPTY_ARRAY, false},
};

}  // namespace

TEST_CASE("Valid addresses", "[AddressParser]") {}

TEST_CASE("Invalid addresses", "[AddressParser]") {}