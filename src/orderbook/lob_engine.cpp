#include "orderbook/lob_engine.hpp"
#include "common/constants.hpp"
#include <iostream>

namespace bncap {

LobEngine::LobEngine(Metrics& metrics) : metrics_(metrics) {}

void LobEngine::add_symbol(const std::string& symbol, SymbolWriters writers) {
    writers_[symbol] = std::move(writers);
    instrument_id(symbol); // pre-assign id in registration order
}

int32_t LobEngine::instrument_id(const std::string& symbol) {
    auto [it, inserted] = instr_ids_.emplace(symbol, next_id_);
    if (inserted) {
        std::cerr << "[LobEngine] instrument_id=" << next_id_
                  << " symbol=" << symbol << '\n';
        ++next_id_;
    }
    return it->second;
}

OrderBook& LobEngine::get_or_create_book(const std::string& symbol) {
    auto it = books_.find(symbol);
    if (it == books_.end())
        it = books_.emplace(symbol, OrderBook{symbol}).first;
    return it->second;
}

void LobEngine::emit_ob_snapshot(const std::string& symbol,
                                  const OrderBook&   book,
                                  RecvTs ts, char type, char side) {
    auto wit = writers_.find(symbol);
    if (wit == writers_.end() || !wit->second.ob || !wit->second.ob->is_open())
        return;

    OBSnapshot snap;
    snap.tsec  = ts.tsec;
    snap.tnsec = ts.tnsec;
    snap.seqno = ++ob_seq_;
    snap.id    = instrument_id(symbol);
    snap.type  = type;
    snap.side  = side;

    PriceLevel bids[5]{}, asks[5]{};
    const int nb = book.top_bids(bids, 5);
    const int na = book.top_asks(asks, 5);
    for (int i = 0; i < 5; ++i) {
        snap.bid[i]      = (i < nb) ? bids[i].first  : 0;
        snap.bid_size[i] = (i < nb) ? bids[i].second : 0;
        snap.ask[i]      = (i < na) ? asks[i].first  : 0;
        snap.ask_size[i] = (i < na) ? asks[i].second : 0;
    }

    wit->second.ob->write(snap);
    ++metrics_.rows_written_orderbook;
}

void LobEngine::on_depth_diff(const DepthDiffEvent& ev) {
    ++metrics_.messages_received;

    // Write to market-data CSV regardless of book state
    {
        auto wit = writers_.find(ev.symbol);
        if (wit != writers_.end() && wit->second.md && wit->second.md->is_open()) {
            wit->second.md->write_depth_diff(ev);
            ++metrics_.rows_written_market;
        }
    }

    OrderBook& book = get_or_create_book(ev.symbol);
    if (book.state() == BookState::uninitialized) return; // wait for depth5

    const bool in_seq = seq_validator_.check(
        ev.symbol, ev.first_update_id, ev.last_update_id, ev.prev_update_id);
    if (!in_seq) {
        ++metrics_.gaps_detected;
        seq_validator_.reset(ev.symbol);
        book.reset(); // revert to uninitialized; depth5 will resync
        return;
    }

    book.apply_depth_diff(ev);
    emit_ob_snapshot(ev.symbol, book, ev.recv_ts, 'D', 'N');
}

void LobEngine::on_depth5(const Depth5Event& ev) {
    ++metrics_.messages_received;

    {
        auto wit = writers_.find(ev.symbol);
        if (wit != writers_.end() && wit->second.md && wit->second.md->is_open()) {
            wit->second.md->write_depth5(ev);
            ++metrics_.rows_written_market;
        }
    }

    OrderBook& book = get_or_create_book(ev.symbol);
    const bool was_uninit = (book.state() == BookState::uninitialized);
    book.apply_depth5(ev);

    // Reset sequence tracking: leave validator uninitialized so the first
    // depth_diff after this depth5 is accepted unconditionally (first-message
    // logic). In live Binance combined streams the depth5 lastUpdateId is
    // often slightly behind the diff stream, so anchoring to it causes
    // spurious gap detections on every depth5 cycle. The real anchor comes
    // from the first diff's u field, after which normal U==last_u+1 checking
    // resumes.
    seq_validator_.reset(ev.symbol);

    if (was_uninit) {
        ++metrics_.books_initialized;
    }

    emit_ob_snapshot(ev.symbol, book, ev.recv_ts, 'F', 'N');
}

void LobEngine::on_trade(const TradeEvent& ev) {
    ++metrics_.messages_received;

    {
        auto wit = writers_.find(ev.symbol);
        if (wit != writers_.end() && wit->second.md && wit->second.md->is_open()) {
            wit->second.md->write_trade(ev);
            ++metrics_.rows_written_market;
        }
    }

    // Annotate: emit current book state at trade time (no book modification)
    OrderBook& book = get_or_create_book(ev.symbol);
    if (book.state() == BookState::valid)
        emit_ob_snapshot(ev.symbol, book, ev.recv_ts, 'T', 'N');
}

void LobEngine::on_reconnect() {
    seq_validator_.reset_all();
    for (auto& [sym, book] : books_)
        book.reset();
}

} // namespace bncap
