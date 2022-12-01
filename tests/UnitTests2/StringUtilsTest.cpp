#include <AddressesManager.hpp>
#include <catch2/catch.hpp>
#include <string>
#include <vector>

TEST_CASE("Split Parenthesis Separated Test", "[StringUtils]") {
	using sv = std::vector<std::string>;
	using s = std::string;

	REQUIRE(sv{{"a"}} == dx::StringUtils::splitParenthesisSeparatedString("a").first);
	REQUIRE(sv{{"a(b)c"}} == dx::StringUtils::splitParenthesisSeparatedString("a(b)c").first);
	REQUIRE(sv{{"(a)"}} == dx::StringUtils::splitParenthesisSeparatedString("((a))").first);
	REQUIRE(sv{{"a"}, {"b"}} == dx::StringUtils::splitParenthesisSeparatedString("(a)(b)").first);
	REQUIRE(sv{{"a"}, {"(b)"}, {"c"}} == dx::StringUtils::splitParenthesisSeparatedString("(a)((b))(c)").first);
	REQUIRE(sv{{"a(b)c"}} == dx::StringUtils::splitParenthesisSeparatedString("(a(b)c)").first);

	REQUIRE(s{"Ok"} != dx::StringUtils::splitParenthesisSeparatedString("(abc").second.what());
	REQUIRE(s{"Ok"} != dx::StringUtils::splitParenthesisSeparatedString("(abc)de").second.what());
	REQUIRE(s{"Ok"} != dx::StringUtils::splitParenthesisSeparatedString("(:Quote&(IBM,MSFT)@:12345").second.what());
}

TEST_CASE("Starts With Test", "[StringUtils]") {
	REQUIRE(dx::StringUtils::startsWith("", '1') == false);
	REQUIRE(dx::StringUtils::startsWith(" ", '1') == false);
	REQUIRE(dx::StringUtils::startsWith("1 ", '1') == true);
}

TEST_CASE("Ends With Test", "[StringUtils]") {
	REQUIRE(dx::StringUtils::endsWith("", '1') == false);
	REQUIRE(dx::StringUtils::endsWith(" ", '1') == false);
	REQUIRE(dx::StringUtils::endsWith("   1", '1') == true);
}