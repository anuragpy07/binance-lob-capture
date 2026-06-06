#include "test_framework.hpp"
#include "marketdata/stream_parser.hpp"
#include "common/scaled_int.hpp"
#include <string>

using namespace bncap;

static RecvTs dummy_ts() { return {1700000000LL, 0}; }

// ---- depth_diff parsing -------------------------------------------------------

TEST(stream_parser_depth_diff_spot) {
    const std::string msg = R"({
        "stream":"btcusdt@depth@100ms",
        "data":{
            "e":"depthUpdate","E":1700000000000,"s":"BTCUSDT",
            "U":101,"u":105,"pu":0,
            "b":[["100.00","1.50"],["99.00","0.00"]],
            "a":[["101.00","2.00"]]
        }
    })";

    bool got_diff = false;
    EventHandlers h;
    h.on_depth_diff = [&](DepthDiffEvent ev) {
        got_diff = true;
        REQUIRE_STR_EQ(ev.symbol, "BTCUSDT");
        REQUIRE_EQ(ev.first_update_id, uint64_t{101});
        REQUIRE_EQ(ev.last_update_id,  uint64_t{105});
        REQUIRE_EQ(ev.prev_update_id,  uint64_t{0});
        REQUIRE_EQ(static_cast<int>(ev.bids.size()), 2);
        REQUIRE_EQ(static_cast<int>(ev.asks.size()), 1);
        REQUIRE_EQ(ev.bids[0].first,  parse_scaled("100.00"));
        REQUIRE_EQ(ev.bids[0].second, parse_scaled("1.50"));
        REQUIRE_EQ(ev.bids[1].second, int64_t{0}); // removal
    };

    const bool ok = StreamParser::parse(msg, dummy_ts(), 0, 0, 1, Venue::spot, h);
    REQUIRE(ok);
    REQUIRE(got_diff);
}

TEST(stream_parser_depth5_spot) {
    const std::string msg = R"({
        "stream":"btcusdt@depth5@100ms",
        "data":{
            "lastUpdateId":200,
            "bids":[["100.00","1.50"]],
            "asks":[["101.00","2.00"]]
        }
    })";

    bool got_d5 = false;
    EventHandlers h;
    h.on_depth5 = [&](Depth5Event ev) {
        got_d5 = true;
        REQUIRE_STR_EQ(ev.symbol, "BTCUSDT");
        REQUIRE_EQ(ev.last_update_id, uint64_t{200});
        REQUIRE_EQ(ev.n_bids, 1);
        REQUIRE_EQ(ev.n_asks, 1);
        REQUIRE_EQ(ev.bids[0].first, parse_scaled("100.00"));
    };

    REQUIRE(StreamParser::parse(msg, dummy_ts(), 0, 0, 2, Venue::spot, h));
    REQUIRE(got_d5);
}

TEST(stream_parser_trade_spot) {
    const std::string msg = R"({
        "stream":"btcusdt@trade",
        "data":{
            "e":"trade","E":1700000000000,"s":"BTCUSDT",
            "t":999,"p":"50000.00","q":"0.01","m":false
        }
    })";

    bool got_trade = false;
    EventHandlers h;
    h.on_trade = [&](TradeEvent ev) {
        got_trade = true;
        REQUIRE_STR_EQ(ev.symbol, "BTCUSDT");
        REQUIRE_EQ(ev.trade_id, uint64_t{999});
        REQUIRE_EQ(ev.price, parse_scaled("50000.00"));
        REQUIRE_EQ(ev.qty,   parse_scaled("0.01"));
        REQUIRE(!ev.is_buyer_market_maker);
    };

    REQUIRE(StreamParser::parse(msg, dummy_ts(), 0, 0, 3, Venue::spot, h));
    REQUIRE(got_trade);
}

TEST(stream_parser_invalid_json_returns_false) {
    EventHandlers h;
    REQUIRE(!StreamParser::parse("{bad json}", dummy_ts(), 0, 0, 0, Venue::spot, h));
}

TEST(stream_parser_missing_stream_returns_false) {
    const std::string msg = R"({"data":{"e":"depthUpdate"}})";
    EventHandlers h;
    REQUIRE(!StreamParser::parse(msg, dummy_ts(), 0, 0, 0, Venue::spot, h));
}

TEST(stream_parser_unknown_suffix_returns_false) {
    const std::string msg = R"({"stream":"btcusdt@ticker","data":{}})";
    EventHandlers h;
    REQUIRE(!StreamParser::parse(msg, dummy_ts(), 0, 0, 0, Venue::spot, h));
}

TEST(stream_parser_passes_conn_metadata) {
    const std::string msg = R"({
        "stream":"btcusdt@trade",
        "data":{"e":"trade","t":1,"p":"100.0","q":"1.0","m":true}
    })";

    EventHandlers h;
    h.on_trade = [&](TradeEvent ev) {
        REQUIRE_EQ(ev.shard_id,   int32_t{3});
        REQUIRE_EQ(ev.conn_epoch, uint32_t{2});
        REQUIRE_EQ(ev.conn_seq,   uint64_t{7});
    };

    REQUIRE(StreamParser::parse(msg, dummy_ts(), 3, 2, 7, Venue::spot, h));
}

TEST(stream_parser_symbol_uppercased) {
    const std::string msg = R"({
        "stream":"ethusdt@trade",
        "data":{"e":"trade","t":1,"p":"2000.0","q":"1.0","m":false}
    })";

    EventHandlers h;
    h.on_trade = [&](TradeEvent ev) {
        REQUIRE_STR_EQ(ev.symbol, "ETHUSDT");
    };

    REQUIRE(StreamParser::parse(msg, dummy_ts(), 0, 0, 0, Venue::spot, h));
}

TEST(stream_parser_payload_json_is_inner_object) {
    // payload_json should be the minified inner data object, not the envelope
    const std::string msg = R"({"stream":"btcusdt@trade","data":{"e":"trade","t":42,"p":"1.0","q":"1.0","m":true}})";

    EventHandlers h;
    h.on_trade = [&](TradeEvent ev) {
        // payload_json must start with '{' (inner object), not contain "stream"
        REQUIRE(ev.payload_json.front() == '{');
        REQUIRE(ev.payload_json.find("stream") == std::string::npos);
    };

    REQUIRE(StreamParser::parse(msg, dummy_ts(), 0, 0, 0, Venue::spot, h));
}

// USD-M specific: depth5 uses "u" instead of "lastUpdateId"
TEST(stream_parser_depth5_usdm_u_field) {
    const std::string msg = R"({
        "stream":"btcusdt@depth5@100ms",
        "data":{
            "e":"depthUpdate","E":1700000000000,
            "u":300,"pu":295,
            "b":[["100.00","1.50"]],
            "a":[["101.00","2.00"]]
        }
    })";

    EventHandlers h;
    h.on_depth5 = [&](Depth5Event ev) {
        REQUIRE_EQ(ev.last_update_id, uint64_t{300});
        REQUIRE_EQ(ev.n_bids, 1);
    };

    REQUIRE(StreamParser::parse(msg, dummy_ts(), 0, 0, 0, Venue::usdm, h));
}
