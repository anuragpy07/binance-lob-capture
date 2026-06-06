#pragma once
#include "common/types.hpp"
#include "marketdata/event.hpp"
#include <fstream>
#include <string>

namespace bncap {

// Writes market_data_<venue>_<SYMBOL>_<date>.csv (Deliverable A).
// All writes happen on the caller's thread; no internal locking.
class MarketDataWriter {
public:
    MarketDataWriter() = default;
    ~MarketDataWriter();

    void open(const std::string& path);
    void close();
    bool is_open() const { return file_.is_open(); }

    void write_depth_diff(const DepthDiffEvent& ev);
    void write_depth5(const Depth5Event& ev);
    void write_trade(const TradeEvent& ev);

    uint64_t rows_written() const { return rows_; }

private:
    std::ofstream file_;
    uint64_t      rows_{0};

    void write_row(RecvTs             ts,
                   Venue              venue,
                   StreamKind         kind,
                   int32_t            shard_id,
                   uint32_t           conn_epoch,
                   uint64_t           conn_seq,
                   const std::string& symbol,
                   const std::string& payload_json);
};

} // namespace bncap