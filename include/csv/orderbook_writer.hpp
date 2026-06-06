#pragma once
#include "orderbook/snapshot.hpp"
#include <fstream>
#include <string>

namespace bncap {

// Writes market_data_<venue>_<SYMBOL>_<date>_orderbook.csv (Deliverable B).
// All writes happen on the caller's thread; no internal locking.
class OrderBookWriter {
public:
    OrderBookWriter() = default;
    ~OrderBookWriter();

    void open(const std::string& path);
    void close();
    bool is_open() const { return file_.is_open(); }

    void write(const OBSnapshot& snap);

    uint64_t rows_written() const { return rows_; }

private:
    std::ofstream file_;
    uint64_t      rows_{0};
};

} // namespace bncap