#pragma once
#include "common/types.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace bncap {

using PriceLevel = std::pair<int64_t, int64_t>; // price (scaled), qty (scaled)

struct DepthDiffEvent {
    RecvTs      recv_ts;
    Venue       venue{Venue::spot};
    int32_t     shard_id{0};
    uint32_t    conn_epoch{0};
    uint64_t    conn_seq{0};
    std::string symbol;
    uint64_t    first_update_id{0}; // U
    uint64_t    last_update_id{0};  // u
    uint64_t    prev_update_id{0};  // pu (USD-M only; 0 for spot)
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    std::string payload_json;       // inner Binance data object, minified
};

struct Depth5Event {
    RecvTs      recv_ts;
    Venue       venue{Venue::spot};
    int32_t     shard_id{0};
    uint32_t    conn_epoch{0};
    uint64_t    conn_seq{0};
    std::string symbol;
    uint64_t    last_update_id{0};
    PriceLevel  bids[5]{};
    PriceLevel  asks[5]{};
    int         n_bids{0};
    int         n_asks{0};
    std::string payload_json;
};

struct TradeEvent {
    RecvTs      recv_ts;
    Venue       venue{Venue::spot};
    int32_t     shard_id{0};
    uint32_t    conn_epoch{0};
    uint64_t    conn_seq{0};
    std::string symbol;
    int64_t     price{0};
    int64_t     qty{0};
    uint64_t    trade_id{0};
    bool        is_buyer_market_maker{false};
    std::string payload_json;
};

} // namespace bncap