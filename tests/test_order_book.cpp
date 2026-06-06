#include "test_framework.hpp"
#include "orderbook/order_book.hpp"
#include "common/scaled_int.hpp"

using namespace bncap;

// Helper: build a Depth5Event with given bid/ask strings
static Depth5Event make_depth5(const std::string& sym,
                                std::vector<std::pair<std::string,std::string>> bids,
                                std::vector<std::pair<std::string,std::string>> asks) {
    Depth5Event ev;
    ev.symbol = sym;
    ev.last_update_id = 100;
    ev.n_bids = static_cast<int>(bids.size());
    ev.n_asks = static_cast<int>(asks.size());
    for (int i = 0; i < ev.n_bids; ++i)
        ev.bids[i] = {parse_scaled(bids[i].first), parse_scaled(bids[i].second)};
    for (int i = 0; i < ev.n_asks; ++i)
        ev.asks[i] = {parse_scaled(asks[i].first), parse_scaled(asks[i].second)};
    return ev;
}

TEST(order_book_initial_state) {
    OrderBook book("BTCUSDT");
    REQUIRE(book.state() == BookState::uninitialized);
}

TEST(order_book_apply_depth5) {
    OrderBook book("BTCUSDT");
    auto ev = make_depth5("BTCUSDT",
        {{"100.0","1.5"},{"99.0","2.0"},{"98.0","3.0"}},
        {{"101.0","1.0"},{"102.0","2.0"}});
    book.apply_depth5(ev);
    REQUIRE(book.state() == BookState::valid);

    PriceLevel bids[5]{}, asks[5]{};
    const int nb = book.top_bids(bids, 5);
    const int na = book.top_asks(asks, 5);
    REQUIRE_EQ(nb, 3);
    REQUIRE_EQ(na, 2);
    // Best bid = 100.0 scaled
    REQUIRE_EQ(bids[0].first, parse_scaled("100.0"));
    // Best ask = 101.0 scaled
    REQUIRE_EQ(asks[0].first, parse_scaled("101.0"));
}

TEST(order_book_depth_diff_add) {
    OrderBook book("BTCUSDT");
    book.apply_depth5(make_depth5("BTCUSDT",
        {{"100.0","1.5"}},
        {{"101.0","1.0"}}));

    DepthDiffEvent diff;
    diff.symbol          = "BTCUSDT";
    diff.first_update_id = 101;
    diff.last_update_id  = 101;
    diff.bids.push_back({parse_scaled("99.5"), parse_scaled("5.0")});
    book.apply_depth_diff(diff);

    PriceLevel bids[5]{};
    const int nb = book.top_bids(bids, 5);
    REQUIRE_EQ(nb, 2);
    REQUIRE_EQ(bids[0].first, parse_scaled("100.0")); // best bid unchanged
    REQUIRE_EQ(bids[1].first, parse_scaled("99.5"));  // new level
}

TEST(order_book_depth_diff_remove) {
    OrderBook book("BTCUSDT");
    book.apply_depth5(make_depth5("BTCUSDT",
        {{"100.0","1.5"},{"99.0","2.0"}},
        {{"101.0","1.0"}}));

    // Remove 100.0 bid (qty=0)
    DepthDiffEvent diff;
    diff.symbol          = "BTCUSDT";
    diff.first_update_id = 101;
    diff.last_update_id  = 101;
    diff.bids.push_back({parse_scaled("100.0"), 0}); // remove
    book.apply_depth_diff(diff);

    PriceLevel bids[5]{};
    const int nb = book.top_bids(bids, 5);
    REQUIRE_EQ(nb, 1);
    REQUIRE_EQ(bids[0].first, parse_scaled("99.0")); // 99.0 is now best
}

TEST(order_book_reset) {
    OrderBook book("BTCUSDT");
    book.apply_depth5(make_depth5("BTCUSDT",
        {{"100.0","1.5"}}, {{"101.0","1.0"}}));
    REQUIRE(book.state() == BookState::valid);
    book.reset();
    REQUIRE(book.state() == BookState::uninitialized);
    PriceLevel bids[5]{};
    REQUIRE_EQ(book.top_bids(bids, 5), 0);
}

TEST(order_book_depth5_clears_old_levels) {
    OrderBook book("BTCUSDT");
    // Initial book with 3 bids
    book.apply_depth5(make_depth5("BTCUSDT",
        {{"100.0","1.0"},{"99.0","2.0"},{"98.0","3.0"}},
        {{"101.0","1.0"}}));
    // New depth5 with only 1 bid — old bids should be cleared
    book.apply_depth5(make_depth5("BTCUSDT",
        {{"105.0","1.0"}},
        {{"106.0","1.0"}}));

    PriceLevel bids[5]{};
    const int nb = book.top_bids(bids, 5);
    REQUIRE_EQ(nb, 1);
    REQUIRE_EQ(bids[0].first, parse_scaled("105.0"));
}