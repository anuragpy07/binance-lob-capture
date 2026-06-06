#pragma once
#include <cstdint>
#include <string_view>

namespace bncap {

// Parse a decimal string to fixed-point int64 (scale_exp decimal places).
// No floating-point used; truncates excess fractional digits.
// parse_scaled("103456.78", 8) -> 10345678000000
int64_t parse_scaled(std::string_view s, int scale_exp = 8);

} // namespace bncap