#include "test_framework.hpp"
#include "common/scaled_int.hpp"

TEST(scaled_int_integer) {
    REQUIRE_EQ(bncap::parse_scaled("100", 8), 10000000000LL);
}

TEST(scaled_int_decimal) {
    REQUIRE_EQ(bncap::parse_scaled("103456.78", 8), 10345678000000LL);
}

TEST(scaled_int_small) {
    REQUIRE_EQ(bncap::parse_scaled("0.00001234", 8), 1234LL);
}

TEST(scaled_int_fraction_only) {
    REQUIRE_EQ(bncap::parse_scaled("0.1", 8), 10000000LL);
}

TEST(scaled_int_zero) {
    REQUIRE_EQ(bncap::parse_scaled("0", 8), 0LL);
    REQUIRE_EQ(bncap::parse_scaled("0.00000000", 8), 0LL);
}

TEST(scaled_int_no_dot) {
    REQUIRE_EQ(bncap::parse_scaled("1", 8), 100000000LL);
}

TEST(scaled_int_full_precision) {
    // 8 fractional digits, exactly at scale boundary
    REQUIRE_EQ(bncap::parse_scaled("0.12345678", 8), 12345678LL);
}

TEST(scaled_int_truncates_excess_digits) {
    // More than 8 fractional digits: truncated (floor)
    REQUIRE_EQ(bncap::parse_scaled("0.123456789", 8), 12345678LL);
}

TEST(scaled_int_large_price) {
    // BTC-like price ~60000
    REQUIRE_EQ(bncap::parse_scaled("60000.12345678", 8), 6000012345678LL);
}
