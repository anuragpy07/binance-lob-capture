#pragma once
#include "marketdata/event.hpp"
#include <functional>
#include <string>

namespace bncap {

// Callbacks for each row type found in the market-data CSV
struct ReplayHandlers {
    std::function<void(DepthDiffEvent)> on_depth_diff;
    std::function<void(Depth5Event)>    on_depth5;
    std::function<void(TradeEvent)>     on_trade;
};

// Reads a market_data_*.csv file and fires handlers row by row.
// Returns number of rows processed; throws std::runtime_error on file errors.
uint64_t read_market_csv(const std::string& path, ReplayHandlers& handlers);

} // namespace bncap