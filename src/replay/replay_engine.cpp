#include "replay/replay_engine.hpp"
#include "replay/csv_reader.hpp"
#include "common/time_utils.hpp"
#include "csv/orderbook_writer.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace bncap {

namespace fs = std::filesystem;

ReplayEngine::ReplayEngine(LobEngine& engine, Metrics& metrics)
    : engine_(engine), metrics_(metrics) {}

uint64_t ReplayEngine::run(const std::string& csv_path,
                            const std::string& output_dir) {
    // Derive symbol, venue, date from filename
    // Expected pattern: market_data_<venue>_<SYMBOL>_<date>.csv
    const fs::path p(csv_path);
    const std::string stem = p.stem().string(); // strip .csv
    // stem = "market_data_<venue>_<SYMBOL>_<date>"
    // Split on '_'
    std::vector<std::string> parts;
    std::string tok;
    for (char c : stem) {
        if (c == '_') { parts.push_back(tok); tok.clear(); }
        else          tok.push_back(c);
    }
    parts.push_back(tok);

    // parts[0]=market, parts[1]=data, parts[2]=venue, parts[3]=SYMBOL, parts[4]=date
    if (parts.size() < 5)
        throw std::runtime_error("Cannot parse filename: " + csv_path);

    const std::string venue_s = parts[2];
    const std::string symbol  = parts[3];
    const std::string date_s  = parts[4];

    // Ensure output directory exists
    fs::create_directories(output_dir);

    // Create output writers
    SymbolWriters sw;
    const std::string ob_path = output_dir + "/market_data_" + venue_s
                               + "_" + symbol + "_" + date_s + "_orderbook.csv";
    sw.ob = std::make_unique<OrderBookWriter>();
    sw.ob->open(ob_path);
    // No market-data writer in replay mode
    sw.md = nullptr;

    engine_.add_symbol(symbol, std::move(sw));

    ReplayHandlers h;
    h.on_depth_diff = [this](DepthDiffEvent ev){ engine_.on_depth_diff(ev); };
    h.on_depth5     = [this](Depth5Event    ev){ engine_.on_depth5(ev); };
    h.on_trade      = [this](TradeEvent     ev){ engine_.on_trade(ev); };

    const uint64_t rows = read_market_csv(csv_path, h);
    std::cerr << "[ReplayEngine] processed " << rows << " rows from "
              << csv_path << '\n';
    return rows;
}

} // namespace bncap