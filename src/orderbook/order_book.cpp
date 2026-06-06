#include "orderbook/order_book.hpp"

namespace bncap {

OrderBook::OrderBook(std::string symbol) : symbol_(std::move(symbol)) {}

void OrderBook::apply_levels(const std::vector<PriceLevel>& levels, bool is_bid) {
    if (is_bid) {
        for (const auto& [price, qty] : levels) {
            if (qty == 0) bids_.erase(price);
            else          bids_[price] = qty;
        }
    } else {
        for (const auto& [price, qty] : levels) {
            if (qty == 0) asks_.erase(price);
            else          asks_[price] = qty;
        }
    }
}

void OrderBook::apply_depth5(const Depth5Event& ev) {
    bids_.clear();
    asks_.clear();

    for (int i = 0; i < ev.n_bids; ++i) {
        if (ev.bids[i].second != 0)
            bids_[ev.bids[i].first] = ev.bids[i].second;
    }
    for (int i = 0; i < ev.n_asks; ++i) {
        if (ev.asks[i].second != 0)
            asks_[ev.asks[i].first] = ev.asks[i].second;
    }

    last_update_id_ = ev.last_update_id;
    state_ = BookState::valid;
}

bool OrderBook::apply_depth_diff(const DepthDiffEvent& ev) {
    if (state_ != BookState::valid) return true; // stale/uninit: skip silently
    apply_levels(ev.bids, true);
    apply_levels(ev.asks, false);
    last_update_id_ = ev.last_update_id;
    return true;
}

void OrderBook::reset() {
    bids_.clear();
    asks_.clear();
    last_update_id_ = 0;
    state_ = BookState::uninitialized;
}

int OrderBook::top_bids(PriceLevel* out, int n) const {
    int count = 0;
    for (auto it = bids_.begin(); it != bids_.end() && count < n; ++it, ++count)
        out[count] = {it->first, it->second};
    return count;
}

int OrderBook::top_asks(PriceLevel* out, int n) const {
    int count = 0;
    for (auto it = asks_.begin(); it != asks_.end() && count < n; ++it, ++count)
        out[count] = {it->first, it->second};
    return count;
}

} // namespace bncap