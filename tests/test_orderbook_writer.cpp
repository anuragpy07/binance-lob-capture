#include "test_framework.hpp"
#include "csv/orderbook_writer.hpp"
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdio>

static std::string slurp2(const std::string& path) {
    std::ifstream f(path);
    return {std::istreambuf_iterator<char>(f), {}};
}

static std::vector<std::string> lines_of(const std::string& s) {
    std::vector<std::string> v;
    std::istringstream ss(s);
    std::string l;
    while (std::getline(ss, l)) v.push_back(l);
    return v;
}

TEST(orderbook_writer_single_row_values) {
    const std::string path = "/tmp/test_obw.csv";
    {
        bncap::OrderBookWriter w;
        w.open(path);

        bncap::OBSnapshot snap{};
        snap.tsec  = 1700000000LL;
        snap.tnsec = 500000000;
        snap.seqno = 42;
        snap.id    = 3;
        snap.type  = 'D';
        snap.side  = 'N';
        for (int i = 0; i < 5; ++i) {
            snap.bid[i]      = static_cast<int64_t>((100 - i) * 100000000LL);
            snap.bid_size[i] = static_cast<int64_t>((i + 1) * 100000000LL);
            snap.ask[i]      = static_cast<int64_t>((101 + i) * 100000000LL);
            snap.ask_size[i] = static_cast<int64_t>((i + 1) * 100000000LL);
        }
        w.write(snap);
        REQUIRE_EQ(w.rows_written(), uint64_t{1});
    }

    const auto lines = lines_of(slurp2(path));
    // lines[0] = header, lines[1] = data
    REQUIRE(lines.size() >= 2);

    // Count columns in data row: must be 26
    int commas = 0;
    for (char c : lines[1]) if (c == ',') ++commas;
    REQUIRE_EQ(commas + 1, 26);

    // Check that tsec value appears
    REQUIRE(lines[1].find("1700000000") != std::string::npos);
    // Check type and side chars
    REQUIRE(lines[1].find(",D,") != std::string::npos);
    REQUIRE(lines[1].find(",N,") != std::string::npos);

    std::remove(path.c_str());
}
