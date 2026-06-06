#include "test_framework.hpp"
#include "marketdata/sequence_validator.hpp"

TEST(seq_validator_first_message_accepted) {
    bncap::SequenceValidator v;
    REQUIRE(v.check("BTCUSDT", 1, 5, 0));
}

TEST(seq_validator_spot_in_sequence) {
    bncap::SequenceValidator v;
    REQUIRE(v.check("BTCUSDT", 1,  5, 0)); // init: last_u = 5
    REQUIRE(v.check("BTCUSDT", 6, 10, 0)); // U=6 == last_u+1=6 -> ok
    REQUIRE(v.check("BTCUSDT", 11,15, 0));
}

TEST(seq_validator_spot_gap_detected) {
    bncap::SequenceValidator v;
    REQUIRE(v.check("BTCUSDT", 1, 5, 0));
    REQUIRE(!v.check("BTCUSDT", 7, 10, 0)); // U=7 != last_u+1=6 -> gap
}

TEST(seq_validator_usdm_in_sequence) {
    bncap::SequenceValidator v;
    REQUIRE(v.check("BTCUSDT", 1, 5, 0)); // init
    REQUIRE(v.check("BTCUSDT", 6, 10, 5)); // pu=5 == last_u=5 -> ok
}

TEST(seq_validator_usdm_gap_detected) {
    bncap::SequenceValidator v;
    REQUIRE(v.check("BTCUSDT", 1, 5, 0));
    REQUIRE(!v.check("BTCUSDT", 7, 10, 4)); // pu=4 != last_u=5 -> gap
}

TEST(seq_validator_reset) {
    bncap::SequenceValidator v;
    REQUIRE(v.check("BTCUSDT", 1, 5, 0));
    v.reset("BTCUSDT");
    // After reset, first message accepted again
    REQUIRE(v.check("BTCUSDT", 100, 200, 0));
}

TEST(seq_validator_independent_symbols) {
    bncap::SequenceValidator v;
    REQUIRE(v.check("BTCUSDT", 1, 5, 0));
    REQUIRE(v.check("ETHUSDT", 1, 3, 0)); // separate symbol, starts fresh
    REQUIRE(v.check("BTCUSDT", 6, 8, 0));
    REQUIRE(v.check("ETHUSDT", 4, 6, 0));
}