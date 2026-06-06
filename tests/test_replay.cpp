#include "test_framework.hpp"
#include "replay/csv_reader.hpp"
#include "csv/csv_escape.hpp"
#include <fstream>
#include <string>
#include <cstdio>

// Properly RFC-4180-escape the JSON payload so split_csv_line round-trips it.
static std::string esc(const std::string& json) {
    return bncap::csv_escape(json);
}

TEST(replay_csv_reader_depth_diff) {
    const std::string path = "/tmp/test_replay.csv";
    const std::string payload =
        R"({"e":"depthUpdate","U":1,"u":2,"pu":0,"b":[["100.0","1.5"]],"a":[["101.0","1.0"]]})";
    {
        std::ofstream f(path);
        f << "recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json\n";
        f << "1700000000,0,spot,depth_diff,0,0,1,BTCUSDT," << esc(payload) << "\n";
    }

    int diff_count = 0;
    bncap::ReplayHandlers h;
    h.on_depth_diff = [&](bncap::DepthDiffEvent ev) {
        ++diff_count;
        REQUIRE_STR_EQ(ev.symbol, "BTCUSDT");
        REQUIRE_EQ(ev.first_update_id, uint64_t{1});
        REQUIRE_EQ(ev.last_update_id,  uint64_t{2});
        REQUIRE_EQ(static_cast<int>(ev.bids.size()), 1);
        REQUIRE_EQ(static_cast<int>(ev.asks.size()), 1);
    };

    const uint64_t rows = bncap::read_market_csv(path, h);
    REQUIRE_EQ(rows, uint64_t{1});
    REQUIRE_EQ(diff_count, 1);
    std::remove(path.c_str());
}

TEST(replay_csv_reader_depth5) {
    const std::string path = "/tmp/test_replay_d5.csv";
    const std::string payload =
        R"({"lastUpdateId":100,"bids":[["100.0","1.5"]],"asks":[["101.0","1.0"]]})";
    {
        std::ofstream f(path);
        f << "recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json\n";
        f << "1700000001,0,spot,depth5,0,0,2,BTCUSDT," << esc(payload) << "\n";
    }

    int d5_count = 0;
    bncap::ReplayHandlers h;
    h.on_depth5 = [&](bncap::Depth5Event ev) {
        ++d5_count;
        REQUIRE_EQ(ev.last_update_id, uint64_t{100});
        REQUIRE_EQ(ev.n_bids, 1);
        REQUIRE_EQ(ev.n_asks, 1);
    };

    const uint64_t rows = bncap::read_market_csv(path, h);
    REQUIRE_EQ(rows, uint64_t{1});
    REQUIRE_EQ(d5_count, 1);
    std::remove(path.c_str());
}

TEST(replay_csv_reader_trade) {
    const std::string path = "/tmp/test_replay_trade.csv";
    const std::string payload =
        R"({"e":"trade","E":1700000002000,"s":"BTCUSDT","t":999,"p":"50000.00","q":"0.01","m":false})";
    {
        std::ofstream f(path);
        f << "recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json\n";
        f << "1700000002,0,spot,trade,0,0,3,BTCUSDT," << esc(payload) << "\n";
    }

    int trade_count = 0;
    bncap::ReplayHandlers h;
    h.on_trade = [&](bncap::TradeEvent ev) {
        ++trade_count;
        REQUIRE_STR_EQ(ev.symbol, "BTCUSDT");
        REQUIRE_EQ(ev.trade_id, uint64_t{999});
    };

    const uint64_t rows = bncap::read_market_csv(path, h);
    REQUIRE_EQ(rows, uint64_t{1});
    REQUIRE_EQ(trade_count, 1);
    std::remove(path.c_str());
}

TEST(replay_csv_reader_skips_unknown_kind) {
    const std::string path = "/tmp/test_replay_unk.csv";
    {
        std::ofstream f(path);
        f << "recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json\n";
        f << "1700000000,0,spot,unknown_kind,0,0,1,BTCUSDT,\"{}\"\n";
    }

    int any_called = 0;
    bncap::ReplayHandlers h;
    h.on_depth_diff = [&](bncap::DepthDiffEvent){ ++any_called; };
    h.on_depth5     = [&](bncap::Depth5Event)   { ++any_called; };
    h.on_trade      = [&](bncap::TradeEvent)    { ++any_called; };

    bncap::read_market_csv(path, h);
    REQUIRE_EQ(any_called, 0);
    std::remove(path.c_str());
}

TEST(replay_csv_reader_multi_row) {
    const std::string path = "/tmp/test_replay_multi.csv";
    {
        std::ofstream f(path);
        f << "recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json\n";
        f << "1700000000,0,spot,depth5,0,0,0,BTCUSDT,"
          << esc(R"({"lastUpdateId":10,"bids":[["100.0","1.0"]],"asks":[["101.0","1.0"]]})") << "\n";
        f << "1700000001,0,spot,depth_diff,0,0,1,BTCUSDT,"
          << esc(R"({"e":"depthUpdate","U":11,"u":12,"pu":0,"b":[["100.5","0.5"]],"a":[]})") << "\n";
    }

    int d5 = 0, diff = 0;
    bncap::ReplayHandlers h;
    h.on_depth5     = [&](bncap::Depth5Event)    { ++d5; };
    h.on_depth_diff = [&](bncap::DepthDiffEvent) { ++diff; };

    const uint64_t rows = bncap::read_market_csv(path, h);
    REQUIRE_EQ(rows, uint64_t{2});
    REQUIRE_EQ(d5, 1);
    REQUIRE_EQ(diff, 1);
    std::remove(path.c_str());
}