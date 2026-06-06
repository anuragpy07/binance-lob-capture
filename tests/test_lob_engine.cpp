#include "test_framework.hpp"
#include "orderbook/lob_engine.hpp"
#include "csv/market_data_writer.hpp"
#include "csv/orderbook_writer.hpp"
#include "metrics/metrics.hpp"
#include "common/scaled_int.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace bncap;

// ---- helpers ----------------------------------------------------------------

static Depth5Event make_depth5(const std::string& sym, uint64_t uid,
                                std::vector<std::pair<double,double>> bids,
                                std::vector<std::pair<double,double>> asks,
                                RecvTs ts = {1700000000LL, 0}) {
    Depth5Event ev;
    ev.symbol          = sym;
    ev.last_update_id  = uid;
    ev.recv_ts         = ts;
    ev.venue           = Venue::spot;
    ev.n_bids = static_cast<int>(bids.size());
    ev.n_asks = static_cast<int>(asks.size());
    for (int i = 0; i < ev.n_bids; ++i) {
        std::ostringstream ps, qs;
        ps << bids[i].first; qs << bids[i].second;
        ev.bids[i] = {parse_scaled(ps.str()), parse_scaled(qs.str())};
    }
    for (int i = 0; i < ev.n_asks; ++i) {
        std::ostringstream ps, qs;
        ps << asks[i].first; qs << asks[i].second;
        ev.asks[i] = {parse_scaled(ps.str()), parse_scaled(qs.str())};
    }
    ev.payload_json = "{}";
    return ev;
}

static DepthDiffEvent make_diff(const std::string& sym,
                                 uint64_t U, uint64_t u, uint64_t pu,
                                 std::vector<std::pair<double,double>> bids = {},
                                 std::vector<std::pair<double,double>> asks = {},
                                 RecvTs ts = {1700000001LL, 0}) {
    DepthDiffEvent ev;
    ev.symbol           = sym;
    ev.first_update_id  = U;
    ev.last_update_id   = u;
    ev.prev_update_id   = pu;
    ev.recv_ts          = ts;
    ev.venue            = Venue::spot;
    ev.payload_json     = "{}";
    for (auto& [p, q] : bids) {
        std::ostringstream ps, qs;
        ps << p; qs << q;
        ev.bids.push_back({parse_scaled(ps.str()), parse_scaled(qs.str())});
    }
    for (auto& [p, q] : asks) {
        std::ostringstream ps, qs;
        ps << p; qs << q;
        ev.asks.push_back({parse_scaled(ps.str()), parse_scaled(qs.str())});
    }
    return ev;
}

// A LobEngine with null writers (no file I/O needed for pure logic tests)
static LobEngine make_engine_no_writers(Metrics& m, const std::string& sym) {
    LobEngine eng(m);
    SymbolWriters sw;
    sw.md = nullptr;
    sw.ob = nullptr;
    eng.add_symbol(sym, std::move(sw));
    return eng;
}

// ---- tests ------------------------------------------------------------------

TEST(lob_engine_initial_ignores_diff) {
    Metrics m;
    auto eng = make_engine_no_writers(m, "BTCUSDT");
    // diff on uninitialized book → dropped, no crash
    eng.on_depth_diff(make_diff("BTCUSDT", 1, 5, 0));
    REQUIRE_EQ(m.rows_written_orderbook.load(), uint64_t{0});
}

TEST(lob_engine_depth5_initializes_book) {
    Metrics m;
    auto eng = make_engine_no_writers(m, "BTCUSDT");
    eng.on_depth5(make_depth5("BTCUSDT", 100,
        {{100.0, 1.0}}, {{101.0, 1.0}}));
    REQUIRE_EQ(m.books_initialized.load(), uint64_t{1});
}

TEST(lob_engine_diff_after_depth5_emits_snapshot) {
    Metrics m;
    auto eng = make_engine_no_writers(m, "BTCUSDT");
    eng.on_depth5(make_depth5("BTCUSDT", 100,
        {{100.0, 1.0}}, {{101.0, 1.0}}));
    // next diff: U=101, u=105, pu=0 (spot: U == last_u+1 == 101)
    eng.on_depth_diff(make_diff("BTCUSDT", 101, 105, 0));
    // one snapshot from depth5, one from diff
    REQUIRE_EQ(m.rows_written_orderbook.load(), uint64_t{0}); // no writers
    REQUIRE_EQ(m.gaps_detected.load(), uint64_t{0});
}

TEST(lob_engine_gap_detected_and_book_reset) {
    Metrics m;
    auto eng = make_engine_no_writers(m, "BTCUSDT");
    eng.on_depth5(make_depth5("BTCUSDT", 100,
        {{100.0, 1.0}}, {{101.0, 1.0}}));
    // First diff after depth5: always accepted as first-message regardless of U.
    // In live Binance combined streams the depth5 lastUpdateId is often slightly
    // behind the diff stream, so no anchor is set from depth5. The first diff
    // establishes the new sequence anchor.
    eng.on_depth_diff(make_diff("BTCUSDT", 200, 205, 0));
    REQUIRE_EQ(m.gaps_detected.load(), uint64_t{0}); // first diff always accepted
    // Second diff with a real gap (U=210 != 205+1=206): gap detected
    eng.on_depth_diff(make_diff("BTCUSDT", 210, 215, 0));
    REQUIRE_EQ(m.gaps_detected.load(), uint64_t{1});
    // book now uninitialized; subsequent diff dropped (not another gap)
    eng.on_depth_diff(make_diff("BTCUSDT", 211, 216, 0));
    REQUIRE_EQ(m.gaps_detected.load(), uint64_t{1});
}

// ---- KEY REGRESSION TEST: depth5 on valid book must resync validator ----
TEST(lob_engine_depth5_refresh_resyncs_validator) {
    Metrics m;
    auto eng = make_engine_no_writers(m, "BTCUSDT");

    // Initial depth5 seeds validator at u=100
    eng.on_depth5(make_depth5("BTCUSDT", 100,
        {{100.0, 1.0}}, {{101.0, 1.0}}));

    // Good diff: U=101, u=105
    eng.on_depth_diff(make_diff("BTCUSDT", 101, 105, 0));
    REQUIRE_EQ(m.gaps_detected.load(), uint64_t{0});

    // Second depth5 arrives (refresh) with lastUpdateId=200 (jumped forward)
    eng.on_depth5(make_depth5("BTCUSDT", 200,
        {{102.0, 2.0}}, {{103.0, 2.0}}));

    // Next diff must reference U=201, u=205
    // WITHOUT the fix this would detect a false gap (U=201 vs expected 106)
    eng.on_depth_diff(make_diff("BTCUSDT", 201, 205, 0));
    REQUIRE_EQ(m.gaps_detected.load(), uint64_t{0}); // no false gap
}

// After gap is detected, the next depth5 re-initializes the book
TEST(lob_engine_reinit_after_gap) {
    Metrics m;
    auto eng = make_engine_no_writers(m, "BTCUSDT");

    eng.on_depth5(make_depth5("BTCUSDT", 100,
        {{100.0, 1.0}}, {{101.0, 1.0}}));
    // First diff: accepted as anchor (last_u=105)
    eng.on_depth_diff(make_diff("BTCUSDT", 101, 105, 0));
    REQUIRE_EQ(m.gaps_detected.load(), uint64_t{0});
    // Inject real gap: U=999 != 105+1=106
    eng.on_depth_diff(make_diff("BTCUSDT", 999, 1000, 0));
    REQUIRE_EQ(m.gaps_detected.load(), uint64_t{1});

    // depth5 should reinitialize (books_initialized increments again)
    eng.on_depth5(make_depth5("BTCUSDT", 1050,
        {{99.0, 3.0}}, {{100.0, 3.0}}));
    REQUIRE_EQ(m.books_initialized.load(), uint64_t{2});

    // First diff after reinit: always accepted (no gap)
    eng.on_depth_diff(make_diff("BTCUSDT", 1999, 2000, 0));
    REQUIRE_EQ(m.gaps_detected.load(), uint64_t{1}); // no new gap
}

// on_reconnect() resets books and validators
TEST(lob_engine_on_reconnect_resets) {
    Metrics m;
    auto eng = make_engine_no_writers(m, "BTCUSDT");
    eng.on_depth5(make_depth5("BTCUSDT", 100,
        {{100.0, 1.0}}, {{101.0, 1.0}}));
    eng.on_depth_diff(make_diff("BTCUSDT", 101, 105, 0));

    // Reconnect: validator and book reset
    eng.on_reconnect();

    // Now a diff with U=1 should be silently dropped (book uninitialized)
    eng.on_depth_diff(make_diff("BTCUSDT", 1, 3, 0));
    REQUIRE_EQ(m.gaps_detected.load(), uint64_t{0}); // dropped, not a gap

    // depth5 re-inits; next diff must pass
    eng.on_depth5(make_depth5("BTCUSDT", 50,
        {{98.0, 1.0}}, {{99.0, 1.0}}));
    eng.on_depth_diff(make_diff("BTCUSDT", 51, 55, 0));
    REQUIRE_EQ(m.gaps_detected.load(), uint64_t{0});
}

// Trade event annotates book snapshot; does not change LOB levels
TEST(lob_engine_trade_does_not_modify_book) {
    Metrics m;
    auto eng = make_engine_no_writers(m, "BTCUSDT");
    eng.on_depth5(make_depth5("BTCUSDT", 100,
        {{100.0, 1.0}}, {{101.0, 1.0}}));

    TradeEvent tr;
    tr.symbol  = "BTCUSDT";
    tr.recv_ts = {1700000002LL, 0};
    tr.venue   = Venue::spot;
    tr.price   = parse_scaled("101.0");
    tr.qty     = parse_scaled("0.5");
    tr.payload_json = "{}";
    eng.on_trade(tr);

    // Gap counter stays 0; book still valid
    REQUIRE_EQ(m.gaps_detected.load(), uint64_t{0});
}

// Instrument IDs are assigned in registration order
TEST(lob_engine_instrument_ids_stable) {
    Metrics m;
    LobEngine eng(m);
    SymbolWriters sw1, sw2;
    sw1.md = nullptr; sw1.ob = nullptr;
    sw2.md = nullptr; sw2.ob = nullptr;
    eng.add_symbol("BTCUSDT", std::move(sw1));
    eng.add_symbol("ETHUSDT", std::move(sw2));
    REQUIRE_EQ(eng.instrument_id("BTCUSDT"), int32_t{0});
    REQUIRE_EQ(eng.instrument_id("ETHUSDT"), int32_t{1});
    // Repeated lookup returns same id
    REQUIRE_EQ(eng.instrument_id("BTCUSDT"), int32_t{0});
}
