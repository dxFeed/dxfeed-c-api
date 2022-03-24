#include <WideDecimal.hpp>
#include <catch2/catch.hpp>
#include <limits>
#include <string>

namespace WideDecimalTest {
inline static dxf_long_t MAX_SIGNIFICAND = dx::rightShift(std::numeric_limits<dxf_long_t>::max(), 8);
inline static dxf_int_t MAX_RANK = 255;
}  // namespace WideDecimalTest

TEST_CASE("Test right shifts", "[WideDecimal]") {
	REQUIRE(dx::rightShift(dxf_long_t(-256), 8) == dxf_long_t(-1));
	REQUIRE(dx::rightShift(dxf_long_t(1), 8) == dxf_long_t(0));
	REQUIRE(dx::rightShift(dxf_long_t(0x8000000000000080), 8) == dxf_long_t(0xFF80000000000000));
}

TEST_CASE("Test signs", "[WideDecimal]") {
	for (dxf_long_t significand = 0; significand <= WideDecimalTest::MAX_SIGNIFICAND;
		 significand += significand / 2 + 1) {
		for (dxf_int_t rank = 0; rank <= WideDecimalTest::MAX_RANK; rank += 16) {
			dxf_long_t rawWide = (significand << 8) | (rank & static_cast<dxf_int_t>(0xFF));
			dxf_long_t rawNeg = (-significand << 8) | (rank & static_cast<dxf_int_t>(0xFF));
			REQUIRE(rawWide == dx::WideDecimal::abs(rawWide));
			REQUIRE(rawNeg == dx::WideDecimal::neg(rawWide));
		}
	}
}

inline static void checkWide(const std::string& expected, dxf_long_t significand, dxf_int_t exponent) {
	double expectedDouble = dx::Double::parseDouble(expected);
	dxf_long_t rawWide = (significand << 8) | ((128 - exponent) & 0xFF);
	dxf_long_t theWide = dx::WideDecimal::composeWide(significand, -exponent);

	INFO("Expected = '" << expected << "', significand = " << significand << ", exponent = " << exponent
						<< ": expectedDouble = " << expectedDouble << ", rawWide = " << rawWide << ", theWide = " << theWide);
	REQUIRE(0 == dx::WideDecimal::compare(rawWide, theWide));
	REQUIRE(dx::Double::compare(expectedDouble, dx::WideDecimal::toDouble(rawWide)) == 0);
	REQUIRE(dx::Double::compare(expectedDouble, dx::WideDecimal::toDouble(theWide)) == 0);
	REQUIRE(expected == dx::WideDecimal::toString(rawWide));
	REQUIRE(expected == dx::WideDecimal::toString(theWide));
}

TEST_CASE("Test wide", "[WideDecimal]") {
	checkWide("NaN", 0, 128);
	checkWide("Infinity", 1, 128);
	checkWide("Infinity", 123456, 128);
	checkWide("-Infinity", -1, 128);
	checkWide("-Infinity", -123456, 128);
	checkWide("0", 0, 0);
	checkWide("0", 0, -10);
	checkWide("0", 0, 10);
	checkWide("1", 1, 0);
	checkWide("1", 1000, -3);
	checkWide("1000000", 1000, 3);
	checkWide("1000000", 1000000, 0);
	checkWide("1000000", 1000000000, -3);
	checkWide("1E7", 10000, 3);
	checkWide("1E7", 10000000, 0);
	checkWide("1E7", 10000000000L, -3);
	checkWide("123.456", 123456, -3);
	checkWide("0.123456", 123456, -6);
	checkWide("0.000123456", 123456, -9);
	checkWide("0.000000123456", 123456, -12);
	checkWide("1.23456E-8", 123456, -13);
	checkWide("1.23456E-9", 123456, -14);
	checkWide("1.23456E-10", 123456, -15);
}
