#pragma once
#include "orderbook/lob_engine.hpp"
#include "metrics/metrics.hpp"
#include <string>

namespace bncap {

// Replay a saved market_data_*.csv through the LOB engine.
// Outputs only orderbook CSV rows; no network connections made.
class ReplayEngine {
public:
    ReplayEngine(LobEngine& engine, Metrics& metrics);

    // Process the given market-data CSV file.
    // Returns number of rows processed.
    uint64_t run(const std::string& csv_path, const std::string& output_dir);

private:
    LobEngine& engine_;
    Metrics&   metrics_;
};

} // namespace bncap