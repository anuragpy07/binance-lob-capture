#include "test_framework.hpp"
#include "websocket/connection_manager.hpp"
#include "common/constants.hpp"
#include <string>
#include <vector>

using namespace bncap;

TEST(build_stream_path_spot_single_symbol) {
    const auto path = build_stream_path(Venue::spot, {"BTCUSDT"});
    REQUIRE_STR_EQ(path,
        "/stream?streams=btcusdt@depth@100ms/btcusdt@depth5@100ms/btcusdt@trade");
}

TEST(build_stream_path_spot_multi_symbol) {
    const auto path = build_stream_path(Venue::spot, {"BTCUSDT", "ETHUSDT"});
    REQUIRE_STR_EQ(path,
        "/stream?streams="
        "btcusdt@depth@100ms/btcusdt@depth5@100ms/btcusdt@trade"
        "/"
        "ethusdt@depth@100ms/ethusdt@depth5@100ms/ethusdt@trade");
}

TEST(build_stream_path_usdm_single_symbol) {
    const auto path = build_stream_path(Venue::usdm, {"BTCUSDT"});
    REQUIRE_STR_EQ(path,
        "/public/stream?streams=btcusdt@depth@100ms/btcusdt@depth5@100ms/btcusdt@trade");
}

TEST(build_stream_path_symbol_lowercased) {
    const auto path = build_stream_path(Venue::spot, {"SOLUSDT"});
    REQUIRE(path.find("solusdt") != std::string::npos);
    REQUIRE(path.find("SOLUSDT") == std::string::npos);
}

TEST(build_stream_path_includes_all_three_stream_types) {
    const auto path = build_stream_path(Venue::spot, {"BNBUSDT"});
    REQUIRE(path.find("bnbusdt@depth@100ms")  != std::string::npos);
    REQUIRE(path.find("bnbusdt@depth5@100ms") != std::string::npos);
    REQUIRE(path.find("bnbusdt@trade")        != std::string::npos);
}
