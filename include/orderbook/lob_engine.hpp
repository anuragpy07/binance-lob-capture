#pragma once
#include "orderbook/order_book.hpp"
#include "orderbook/snapshot.hpp"
#include "csv/market_data_writer.hpp"
#include "csv/orderbook_writer.hpp"
#include "metrics/metrics.hpp"
#include "marketdata/sequence_validator.hpp"
#include <string>
#include <unordered_map>
#include <memory>

namespace bncap {

// Per-symbol writer pair
struct SymbolWriters {
    std::unique_ptr<MarketDataWriter> md;
    std::unique_ptr<OrderBookWriter>  ob;
};

// Integrates parsing results -> order book -> CSV output.
// Called exclusively from the IO thread; no locking needed.
class LobEngine {
public:
    explicit LobEngine(Metrics& metrics);

    // Register per-symbol writers (call before processing events)
    void add_symbol(const std::string& symbol, SymbolWriters writers);

    void on_depth_diff(const DepthDiffEvent& ev);
    void on_depth5(const Depth5Event& ev);
    void on_trade(const TradeEvent& ev);

    // Reset sequence tracking (e.g. on reconnect)
    void on_reconnect();

    // Stable numeric instrument id for the session (0-based insertion order)
    int32_t instrument_id(const std::string& symbol);

private:
    Metrics&          metrics_;
    SequenceValidator seq_validator_;
    uint64_t          ob_seq_{0};

    std::unordered_map<std::string, OrderBook>     books_;
    std::unordered_map<std::string, SymbolWriters> writers_;
    std::unordered_map<std::string, int32_t>       instr_ids_;
    int32_t next_id_{0};

    OrderBook& get_or_create_book(const std::string& symbol);
    void emit_ob_snapshot(const std::string& symbol,
                          const OrderBook& book,
                          RecvTs ts, char type, char side);
};

} // namespace bncap