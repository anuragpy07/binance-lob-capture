#include "test_framework.hpp"
#include "csv/market_data_writer.hpp"
#include "csv/orderbook_writer.hpp"
#include "orderbook/snapshot.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
}

static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::istringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, delim)) parts.push_back(tok);
    return parts;
}

static int count_fields(const std::string& line) {
    // Simple comma count + 1, ignoring quoted fields for header check
    int commas = 0;
    for (char c : line) if (c == ',') ++commas;
    return commas + 1;
}

TEST(market_data_header) {
    const std::string path = "/tmp/test_md.csv";
    {
        bncap::MarketDataWriter w;
        w.open(path);
    }
    const std::string content = read_file(path);
    const auto lines = split(content, '\n');
    REQUIRE(!lines.empty());
    REQUIRE_STR_EQ(lines[0],
        "recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json");
    std::remove(path.c_str());
}

TEST(market_data_header_column_count) {
    const std::string path = "/tmp/test_md2.csv";
    {
        bncap::MarketDataWriter w;
        w.open(path);
    }
    const std::string content = read_file(path);
    const auto lines = split(content, '\n');
    REQUIRE_EQ(count_fields(lines[0]), 9);
    std::remove(path.c_str());
}

TEST(orderbook_header) {
    const std::string path = "/tmp/test_ob.csv";
    {
        bncap::OrderBookWriter w;
        w.open(path);
    }
    const std::string content = read_file(path);
    const auto lines = split(content, '\n');
    REQUIRE(!lines.empty());
    REQUIRE_STR_EQ(lines[0],
        "tsec,tnsec,seqNo,id,type,side,"
        "bid0,bid1,bid2,bid3,bid4,"
        "bid_size0,bid_size1,bid_size2,bid_size3,bid_size4,"
        "ask0,ask1,ask2,ask3,ask4,"
        "ask_size0,ask_size1,ask_size2,ask_size3,ask_size4");
    std::remove(path.c_str());
}

TEST(orderbook_row_has_26_columns) {
    const std::string path = "/tmp/test_ob2.csv";
    {
        bncap::OrderBookWriter w;
        w.open(path);
        bncap::OBSnapshot snap{};
        snap.tsec = 1000; snap.tnsec = 500; snap.seqno = 1;
        snap.id = 0; snap.type = 'D'; snap.side = 'N';
        w.write(snap);
    }
    const std::string content = read_file(path);
    const auto lines = split(content, '\n');
    // lines[0] = header, lines[1] = data row, lines[2] = empty (trailing \n)
    REQUIRE(lines.size() >= 2);
    REQUIRE_EQ(count_fields(lines[1]), 26);
    std::remove(path.c_str());
}
