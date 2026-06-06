#pragma once
#include <atomic>
#include <cstdint>

namespace bncap {

struct Metrics {
    std::atomic<uint64_t> messages_received{0};
    std::atomic<uint64_t> parse_errors{0};
    std::atomic<uint64_t> reconnects{0};
    std::atomic<uint64_t> rows_written_market{0};
    std::atomic<uint64_t> rows_written_orderbook{0};
    std::atomic<uint64_t> gaps_detected{0};
    std::atomic<uint64_t> books_initialized{0};

    // Print a summary to stderr
    void print_summary() const;
};

} // namespace bncap