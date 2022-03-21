#include <WideDecimal.h>

#include <WideDecimal.cpp>
#include <catch2/catch.hpp>
#include <string>

void checkWide(const std::string& expected, dxf_long_t significand, dxf_int_t exponent) {
	double expectedDouble = dx::Double::parseDouble(expected);
	long rawWide = (significand << 8) | ((128 - exponent) & 0xFF);
	long theWide = composeWide(significand, -exponent);
}
