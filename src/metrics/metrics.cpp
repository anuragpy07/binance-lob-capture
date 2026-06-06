#include "metrics/metrics.hpp"
#include <iostream>
#include <iomanip>

namespace bncap {

void Metrics::print_summary() const {
    const auto row = [](const char* name, uint64_t val) {
        std::cerr << std::left << std::setw(24) << name
                  << ": " << val << '\n';
    };
    std::cerr << "\n--- Metrics ---\n";
    row("messages_received",      messages_received.load());
    row("parse_errors",           parse_errors.load());
    row("reconnects",             reconnects.load());
    row("rows_written_market",    rows_written_market.load());
    row("rows_written_orderbook", rows_written_orderbook.load());
    row("gaps_detected",          gaps_detected.load());
    row("books_initialized",      books_initialized.load());
    std::cerr << "---------------\n";
}

} // namespace bncap