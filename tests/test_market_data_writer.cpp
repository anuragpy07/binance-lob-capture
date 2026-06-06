#include "test_framework.hpp"
#include "csv/market_data_writer.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>

static std::string slurp(const std::string& path) {
    std::ifstream f(path);
    return {std::istreambuf_iterator<char>(f), {}};
}

TEST(market_data_writer_depth_diff_row) {
    const std::string path = "/tmp/test_mdw_diff.csv";
    {
        bncap::MarketDataWriter w;
        w.open(path);
        bncap::DepthDiffEvent ev;
        ev.recv_ts    = {1700000000LL, 123456789};
        ev.venue      = bncap::Venue::spot;
        ev.shard_id   = 0;
        ev.conn_epoch = 0;
        ev.conn_seq   = 1;
        ev.symbol     = "BTCUSDT";
        ev.payload_json = R"({"e":"depthUpdate","U":1,"u":2,"b":[],"a":[]})";
        w.write_depth_diff(ev);
        REQUIRE_EQ(w.rows_written(), uint64_t{1});
    }
    const std::string content = slurp(path);
    // Should contain "depth_diff" and "BTCUSDT"
    REQUIRE(content.find("depth_diff") != std::string::npos);
    REQUIRE(content.find("BTCUSDT")    != std::string::npos);
    REQUIRE(content.find("1700000000") != std::string::npos);
    std::remove(path.c_str());
}

TEST(market_data_writer_trade_row) {
    const std::string path = "/tmp/test_mdw_trade.csv";
    {
        bncap::MarketDataWriter w;
        w.open(path);
        bncap::TradeEvent ev;
        ev.recv_ts  = {1700000001LL, 0};
        ev.venue    = bncap::Venue::spot;
        ev.symbol   = "ETHUSDT";
        ev.payload_json = R"({"e":"trade","s":"ETHUSDT","p":"2000.00","q":"0.5"})";
        w.write_trade(ev);
    }
    const std::string content = slurp(path);
    REQUIRE(content.find("trade")   != std::string::npos);
    REQUIRE(content.find("ETHUSDT") != std::string::npos);
    std::remove(path.c_str());
}

TEST(market_data_writer_payload_json_escaped) {
    const std::string path = "/tmp/test_mdw_escape.csv";
    {
        bncap::MarketDataWriter w;
        w.open(path);
        bncap::Depth5Event ev;
        ev.recv_ts  = {1700000002LL, 0};
        ev.venue    = bncap::Venue::spot;
        ev.symbol   = "BTCUSDT";
        ev.n_bids = ev.n_asks = 0;
        ev.payload_json = R"({"lastUpdateId":100,"bids":[],"asks":[]})";
        w.write_depth5(ev);
    }
    const std::string content = slurp(path);
    // payload_json contains commas -> should be quoted in the CSV
    REQUIRE(content.find("\"") != std::string::npos);
    std::remove(path.c_str());
}