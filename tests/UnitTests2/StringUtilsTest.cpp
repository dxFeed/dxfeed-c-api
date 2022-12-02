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
	REQUIRE(dx::StringUtils::startsWith("1 1  ", '1') == true);
}

TEST_CASE("Ends With Test", "[StringUtils]") {
	REQUIRE(dx::StringUtils::endsWith("", '1') == false);
	REQUIRE(dx::StringUtils::endsWith(" ", '1') == false);
	REQUIRE(dx::StringUtils::endsWith("   1", '1') == true);
	REQUIRE(dx::StringUtils::endsWith("(other)test(prop1,prop2)", ')') == true);
}

void checkParseProperties(const std::string& description, const std::string& result, const std::vector<std::string>& properties) {
	std::vector<std::string> parsedProperties;
	auto parseResult = dx::StringUtils::parseProperties(description, parsedProperties);

	REQUIRE(result == parseResult.first);
	REQUIRE(properties == parsedProperties);
}

TEST_CASE("Parse Properties Test", "[StringUtils]") {
	checkParseProperties("test", "test", {});
	checkParseProperties("test(prop)", "test", {"prop"});
	checkParseProperties("test[prop]", "test", {"prop"});
	checkParseProperties("test[prop1](prop2)", "test", {"prop1", "prop2"});
	checkParseProperties("test[prop1,prop2]", "test", {"prop1", "prop2"});
	checkParseProperties("test(prop1,prop2)", "test", {"prop1", "prop2"});
	checkParseProperties("(other)test(prop1,prop2)", "(other)test", {"prop1", "prop2"});
	checkParseProperties("[other]test[prop1,prop2]", "[other]test", {"prop1", "prop2"});
	checkParseProperties("[[]]", "", {"[]"});
	checkParseProperties("[()]", "", {"()"});
	checkParseProperties("(())", "", {"()"});
	checkParseProperties("([])", "", {"[]"});
	checkParseProperties("a[]b", "a[]b", {});
	checkParseProperties("a[,,]", "a", {"", "", ""});
	checkParseProperties(" b b ( , , ) ", "b b", {"", "", ""});
}