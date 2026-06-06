#include "test_framework.hpp"
#include "orderbook/lob_engine.hpp"
#include "csv/market_data_writer.hpp"
#include "csv/orderbook_writer.hpp"
#include "replay/csv_reader.hpp"
#include "replay/replay_engine.hpp"
#include "metrics/metrics.hpp"
#include "common/scaled_int.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>

using namespace bncap;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string slurp(const std::string& path) {
    std::ifstream f(path);
    return {std::istreambuf_iterator<char>(f), {}};
}

static std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> v;
    std::istringstream ss(s);
    std::string l;
    while (std::getline(ss, l)) if (!l.empty()) v.push_back(l);
    return v;
}

static std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> fields;
    std::string f;
    bool in_q = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_q) {
            if (c == '"' && i + 1 < line.size() && line[i+1] == '"') { f += '"'; ++i; }
            else if (c == '"') in_q = false;
            else f += c;
        } else {
            if (c == '"') in_q = true;
            else if (c == ',') { fields.push_back(f); f.clear(); }
            else f += c;
        }
    }
    fields.push_back(f);
    return fields;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// Replay the sample market_data CSV and verify key orderbook rows
TEST(replay_integration_sample_csv) {
    const std::string md_path = "/tmp/market_data_spot_BTCUSDT_2025-06-03.csv";
    const std::string out_dir = "/tmp";

    // Write the sample CSV to a tmp location so we can replay it
    const std::string sample = R"(recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json
1748908800,123456789,spot,depth5,0,0,0,BTCUSDT,"{""lastUpdateId"":4523891200,""bids"":[[""67234.56"",""0.12300000""],[""67234.00"",""0.45600000""],[""67233.50"",""1.20000000""],[""67232.00"",""2.34500000""],[""67230.00"",""0.87600000""]],""asks"":[[""67235.00"",""0.09800000""],[""67235.50"",""0.32100000""],[""67236.00"",""0.55400000""],[""67237.00"",""1.10000000""],[""67238.00"",""0.76500000""]]}"
1748908800,234567890,spot,depth_diff,0,0,1,BTCUSDT,"{""e"":""depthUpdate"",""E"":1748908800234,""s"":""BTCUSDT"",""U"":4523891201,""u"":4523891205,""b"":[[""67234.56"",""0.15600000""],[""67231.00"",""0.50000000""]],""a"":[[""67235.00"",""0.07200000""],[""67239.00"",""0.30000000""]]}"
1748908800,345678901,spot,trade,0,0,2,BTCUSDT,"{""e"":""trade"",""E"":1748908800345,""s"":""BTCUSDT"",""t"":3892741023,""p"":""67235.00"",""q"":""0.07200000"",""b"":456789012,""a"":456789013,""T"":1748908800344,""m"":false,""M"":true}"
1748908800,456789012,spot,depth_diff,0,0,3,BTCUSDT,"{""e"":""depthUpdate"",""E"":1748908800456,""s"":""BTCUSDT"",""U"":4523891206,""u"":4523891210,""b"":[[""67234.00"",""0.00000000""],[""67233.00"",""1.80000000""]],""a"":[[""67235.50"",""0.28000000""],[""67240.00"",""0.15000000""]]}"
1748908800,567890123,spot,depth5,0,0,4,BTCUSDT,"{""lastUpdateId"":4523891210,""bids"":[[""67234.56"",""0.15600000""],[""67233.50"",""1.20000000""],[""67233.00"",""1.80000000""],[""67232.00"",""2.34500000""],[""67231.00"",""0.50000000""]],""asks"":[[""67235.00"",""0.07200000""],[""67235.50"",""0.28000000""],[""67236.00"",""0.55400000""],[""67237.00"",""1.10000000""],[""67238.00"",""0.76500000""]]}"
1748908800,678901234,spot,depth_diff,0,0,5,BTCUSDT,"{""e"":""depthUpdate"",""E"":1748908800678,""s"":""BTCUSDT"",""U"":4523891211,""u"":4523891214,""b"":[[""67235.00"",""0.20000000""]],""a"":[[""67235.00"",""0.00000000""],[""67236.50"",""0.40000000""]]}"
1748908800,789012345,spot,trade,0,0,6,BTCUSDT,"{""e"":""trade"",""E"":1748908800789,""s"":""BTCUSDT"",""t"":3892741024,""p"":""67235.50"",""q"":""0.02500000"",""b"":456789014,""a"":456789015,""T"":1748908800788,""m"":true,""M"":true}"
1748908801,12345678,spot,depth_diff,0,0,7,BTCUSDT,"{""e"":""depthUpdate"",""E"":1748908801012,""s"":""BTCUSDT"",""U"":4523891215,""u"":4523891219,""b"":[[""67236.00"",""0.75000000""],[""67230.00"",""0.00000000""]],""a"":[[""67237.50"",""0.90000000""],[""67238.00"",""0.00000000""]]}"
1748908801,123456789,spot,depth5,0,0,8,BTCUSDT,"{""lastUpdateId"":4523891219,""bids"":[[""67236.00"",""0.75000000""],[""67235.00"",""0.20000000""],[""67234.56"",""0.15600000""],[""67233.50"",""1.20000000""],[""67233.00"",""1.80000000""]],""asks"":[[""67235.50"",""0.28000000""],[""67236.00"",""0.55400000""],[""67236.50"",""0.40000000""],[""67237.00"",""1.10000000""],[""67237.50"",""0.90000000""]]}"
1748908801,234567890,spot,depth_diff,0,0,9,BTCUSDT,"{""e"":""depthUpdate"",""E"":1748908801234,""s"":""BTCUSDT"",""U"":4523891220,""u"":4523891223,""b"":[[""67237.00"",""0.10000000""]],""a"":[[""67235.50"",""0.00000000""],[""67238.50"",""0.60000000""]]}"
)";

    {
        std::ofstream f(md_path);
        f << sample;
    }

    Metrics m;
    // Engine is scoped so its destructor flushes+closes all writers before we read back
    {
        LobEngine engine(m);
        ReplayEngine replay(engine, m);
        replay.run(md_path, out_dir);
    }

    const std::string ob_path = "/tmp/market_data_spot_BTCUSDT_2025-06-03_orderbook.csv";
    const auto lines = split_lines(slurp(ob_path));
    // 1 header + 10 data rows
    REQUIRE_EQ(static_cast<int>(lines.size()), 11);

    // ---- seqNo=1 (depth5 F): 10 events processed so 10 rows
    // Row 1: depth5 with full initial top-5
    {
        auto f = split_csv(lines[1]);
        REQUIRE_EQ(static_cast<int>(f.size()), 26);
        REQUIRE_STR_EQ(f[4], "F");  // type
        REQUIRE_STR_EQ(f[5], "N");  // side
        // bid0=67234.56, bid1=67234.00 (both from initial depth5)
        REQUIRE_STR_EQ(f[6],  "6723456000000"); // bid0
        REQUIRE_STR_EQ(f[7],  "6723400000000"); // bid1 = 67234.00
        REQUIRE_STR_EQ(f[16], "6723500000000"); // ask0 = 67235.00
        REQUIRE_STR_EQ(f[20], "6723800000000"); // ask4 = 67238.00
    }

    // ---- seqNo=2 (depth_diff D): after diff that adds 67231.00 and 67239.00
    // 67234.00 should still be present as bid1 (NOT displaced by 67233.50)
    {
        auto f = split_csv(lines[2]);
        REQUIRE_STR_EQ(f[4], "D");
        REQUIRE_STR_EQ(f[6],  "6723456000000"); // bid0 = 67234.56
        REQUIRE_STR_EQ(f[7],  "6723400000000"); // bid1 = 67234.00 (must be present)
        REQUIRE_STR_EQ(f[10], "6723100000000"); // bid4 = 67231.00 (new level)
        // 67238.00 still in asks (not touched by this diff)
        REQUIRE_STR_EQ(f[20], "6723800000000"); // ask4 = 67238.00
    }

    // ---- seqNo=4 (depth_diff D): removes 67234.00 from bids
    {
        auto f = split_csv(lines[4]);
        REQUIRE_STR_EQ(f[4], "D");
        // After removing 67234.00, top bids should be: 67234.56, 67233.50, 67233.00, 67232.00, 67231.00
        REQUIRE_STR_EQ(f[6],  "6723456000000"); // bid0 = 67234.56
        REQUIRE_STR_EQ(f[7],  "6723350000000"); // bid1 = 67233.50 (67234.00 removed)
    }

    // ---- seqNo=5 (depth5 F refresh): depth5 on valid book resets validator
    // This is the bug-fix test: the next diff (seqNo=6) must NOT trigger a gap
    {
        auto f = split_csv(lines[5]);
        REQUIRE_STR_EQ(f[4], "F");
    }

    // ---- seqNo=6 (depth_diff D): first diff after depth5 refresh
    // Without the fix, this would have triggered a false gap and the row would not exist
    {
        auto f = split_csv(lines[6]);
        REQUIRE_STR_EQ(f[4], "D");
        // 67235.00 added to bids (from diff), makes it the new best bid
        REQUIRE_STR_EQ(f[6], "6723500000000"); // bid0 = 67235.00
    }

    // ---- seqNo=10 (last row): verify final book state
    {
        auto f = split_csv(lines[10]);
        REQUIRE_STR_EQ(f[4], "D");
        REQUIRE_STR_EQ(f[6], "6723700000000"); // bid0 = 67237.00
        REQUIRE_STR_EQ(f[16], "6723600000000"); // ask0 = 67236.00 (67235.50 removed)
    }

    // All 10 rows must have exactly 26 columns
    for (int i = 1; i <= 10; ++i) {
        auto f = split_csv(lines[i]);
        REQUIRE_EQ(static_cast<int>(f.size()), 26);
    }

    // Cleanup
    std::remove(md_path.c_str());
    std::remove(ob_path.c_str());
}

// Verify that no gaps are detected when replaying correct sequential data
TEST(replay_integration_no_gaps) {
    const std::string md_path = "/tmp/market_data_spot_BTCUSDT_2023-11-14.csv";
    const std::string out_dir = "/tmp";

    {
        std::ofstream f(md_path);
        f << "recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json\n";
        f << R"(1700000000,0,spot,depth5,0,0,0,BTCUSDT,"{""lastUpdateId"":100,""bids"":[[""100.00"",""1.00""]],""asks"":[[""101.00"",""1.00""]]})" << "\n";
        f << R"(1700000001,0,spot,depth_diff,0,0,1,BTCUSDT,"{""e"":""depthUpdate"",""U"":101,""u"":105,""pu"":0,""b"":[[""100.50"",""0.50""]],""a"":[]}")" << "\n";
        f << R"(1700000002,0,spot,depth_diff,0,0,2,BTCUSDT,"{""e"":""depthUpdate"",""U"":106,""u"":110,""pu"":0,""b"":[],""a"":[[""101.50"",""2.00""]]})" << "\n";
    }

    Metrics m;
    LobEngine engine(m);
    ReplayEngine replay(engine, m);
    replay.run(md_path, out_dir);

    REQUIRE_EQ(m.gaps_detected.load(), uint64_t{0});
    REQUIRE_EQ(m.books_initialized.load(), uint64_t{1});

    std::remove(md_path.c_str());
    std::remove("/tmp/market_data_spot_BTCUSDT_2023-11-14_orderbook.csv");
}

// Verify depth5-on-valid-book does not create false gaps (the key bug fix).
// Uses timestamp 1700086400 = 2023-11-15 to avoid file collision with replay_integration_no_gaps.
TEST(replay_integration_depth5_refresh_no_false_gap) {
    const std::string md_path = "/tmp/market_data_spot_BTCUSDT_2023-11-15.csv";
    const std::string out_dir = "/tmp";

    // Sequence: depth5(u=100), diff(U=101,u=200), depth5_refresh(u=250), diff(U=251,u=255)
    // Old code: after diff(u=200), validator.last_u=200; depth5_refresh ignored by validator;
    //           next diff U=251 != 200+1 → FALSE GAP!
    // New code: depth5_refresh resets validator to u=250; next diff U=251=250+1 → OK.
    {
        std::ofstream f(md_path);
        f << "recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json\n";
        f << R"(1700086400,0,spot,depth5,0,0,0,BTCUSDT,"{""lastUpdateId"":100,""bids"":[[""100.00"",""1.00""]],""asks"":[[""101.00"",""1.00""]]})" << "\n";
        f << R"(1700086401,0,spot,depth_diff,0,0,1,BTCUSDT,"{""e"":""depthUpdate"",""U"":101,""u"":200,""pu"":0,""b"":[],""a"":[]}")" << "\n";
        f << R"(1700086402,0,spot,depth5,0,0,2,BTCUSDT,"{""lastUpdateId"":250,""bids"":[[""100.00"",""2.00""]],""asks"":[[""101.00"",""2.00""]]})" << "\n";
        f << R"(1700086403,0,spot,depth_diff,0,0,3,BTCUSDT,"{""e"":""depthUpdate"",""U"":251,""u"":255,""pu"":0,""b"":[],""a"":[]}")" << "\n";
    }

    Metrics m;
    LobEngine engine(m);
    ReplayEngine replay(engine, m);
    replay.run(md_path, out_dir);

    REQUIRE_EQ(m.gaps_detected.load(), uint64_t{0});
    REQUIRE_EQ(m.rows_written_orderbook.load(), uint64_t{4});

    std::remove(md_path.c_str());
    std::remove("/tmp/market_data_spot_BTCUSDT_2023-11-15_orderbook.csv");
}
