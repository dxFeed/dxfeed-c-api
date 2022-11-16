#include <WideDecimal.hpp>
#include <catch2/catch.hpp>
#include <cmath>
#include <random>
#include <string>
#include <type_traits>

namespace WideDecimalTest {
const dxf_int_t MAX_RANK = 255;
}  // namespace WideDecimalTest

TEST_CASE("Test right shifts", "[WideDecimal]") {
	REQUIRE(dx::rightShift(static_cast<dxf_long_t>(-256LL), 8) == static_cast<dxf_long_t>(-1LL));
	REQUIRE(dx::rightShift(static_cast<dxf_long_t>(1LL), 8) == 0LL);
	REQUIRE(dx::rightShift(static_cast<dxf_long_t>(0x8000000000000080LL), 8) ==
			static_cast<dxf_long_t>(0xFF80000000000000LL));
	REQUIRE(dx::rightShift(-9223372036854775680LL, 8) == -36028797018963968LL);
	REQUIRE(dx::rightShift(static_cast<dxf_long_t>(0x8000000000000000LL), 8) ==
			static_cast<dxf_long_t>(0xFF80000000000000LL));
	REQUIRE(dx::WideDecimal::Consts::MIN_SIGNIFICAND < dx::WideDecimal::Consts::MAX_SIGNIFICAND);
}

TEST_CASE("Test signs", "[WideDecimal]") {
	for (dxf_long_t significand = 0; significand <= dx::WideDecimal::Consts::MAX_SIGNIFICAND;
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
	dxf_long_t rawWide = (significand << 8) | ((128LL - static_cast<dxf_long_t>(exponent)) & 0xFF);
	dxf_long_t theWide = dx::WideDecimal::composeWide(significand, -exponent);

	INFO("Expected = '" << expected << "', significand = " << significand << ", exponent = " << exponent
						<< ": expectedDouble = " << expectedDouble << ", rawWide = " << rawWide
						<< ", theWide = " << theWide);
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

template <typename G>
inline static double generate(G&& generator) {
	std::uniform_int_distribution<> distrib0to11(0, 11);
	std::uniform_real_distribution<> distrib0to1(0.0, 1.0);

	auto power = static_cast<dxf_long_t>(std::pow(10, distrib0to11(std::forward<G>(generator))));

	return std::floor(distrib0to1(std::forward<G>(generator)) * power) / power;
}

TEST_CASE("Test random", "[WideDecimal]") {
	std::random_device randomDevice;
	std::mt19937 generator(randomDevice());

	for (int i = 0; i < 10000; i++) {
		double a = generate(generator);
		double b = generate(generator);
		dxf_long_t wa = dx::WideDecimal::composeWide(a);
		dxf_long_t wb = dx::WideDecimal::composeWide(b);

		REQUIRE(0 == dx::Double::compare(a, dx::WideDecimal::toDouble(wa)));
		REQUIRE(0 == dx::Double::compare(b, dx::WideDecimal::toDouble(wb)));
		REQUIRE(dx::Double::compare(a, b) == dx::WideDecimal::compare(wa, wb));

		auto sum = a + b;
		REQUIRE(std::abs(sum - dx::WideDecimal::toDouble(dx::WideDecimal::sum(wa, wb))) <= 1e-15);
	}
}