#pragma once
#include "common/constants.hpp"
#include "marketdata/event.hpp"
#include <functional>
#include <map>
#include <string>

namespace bncap {

enum class BookState { uninitialized, valid };

class OrderBook {
public:
    explicit OrderBook(std::string symbol);

    // Replace top-5 levels from a depth5 snapshot; sets state to valid.
    void apply_depth5(const Depth5Event& ev);

    // Apply a differential update. Sequence gap detection is done by the caller.
    // Returns false only if the book is in an invalid state (should not happen).
    bool apply_depth_diff(const DepthDiffEvent& ev);

    BookState   state()          const { return state_; }
    uint64_t    last_update_id() const { return last_update_id_; }
    const std::string& symbol()  const { return symbol_; }

    // Fill out[0..n-1] with the top-n bid levels; returns actual count filled.
    int top_bids(PriceLevel* out, int n) const;
    int top_asks(PriceLevel* out, int n) const;

    // Reset book to uninitialized (used on reconnect / gap)
    void reset();

private:
    std::string symbol_;
    BookState   state_{BookState::uninitialized};
    uint64_t    last_update_id_{0};

    // bids: best (highest price) first -> descending key order
    std::map<int64_t, int64_t, std::greater<int64_t>> bids_;
    // asks: best (lowest price) first -> ascending key order
    std::map<int64_t, int64_t> asks_;

    void apply_levels(const std::vector<PriceLevel>& levels, bool is_bid);
};

} // namespace bncap