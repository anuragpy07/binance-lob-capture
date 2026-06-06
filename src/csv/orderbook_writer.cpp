#include "csv/orderbook_writer.hpp"
#include "common/constants.hpp"
#include <stdexcept>
#include <string>

namespace bncap {

static const char* OB_HEADER =
    "tsec,tnsec,seqNo,id,type,side,"
    "bid0,bid1,bid2,bid3,bid4,"
    "bid_size0,bid_size1,bid_size2,bid_size3,bid_size4,"
    "ask0,ask1,ask2,ask3,ask4,"
    "ask_size0,ask_size1,ask_size2,ask_size3,ask_size4\n";

OrderBookWriter::~OrderBookWriter() { close(); }

void OrderBookWriter::open(const std::string& path) {
    file_.open(path, std::ios::out | std::ios::trunc);
    if (!file_.is_open())
        throw std::runtime_error("OrderBookWriter: cannot open " + path);
    file_ << OB_HEADER;
}

void OrderBookWriter::close() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

void OrderBookWriter::write(const OBSnapshot& s) {
    std::string row;
    row.reserve(256);
    row += std::to_string(s.tsec);  row += ',';
    row += std::to_string(s.tnsec); row += ',';
    row += std::to_string(s.seqno); row += ',';
    row += std::to_string(s.id);    row += ',';
    row.push_back(s.type);          row += ',';
    row.push_back(s.side);

    for (std::size_t i = 0; i < TOP_LEVELS; ++i) {
        row += ',';
        row += std::to_string(s.bid[i]);
    }
    for (std::size_t i = 0; i < TOP_LEVELS; ++i) {
        row += ',';
        row += std::to_string(s.bid_size[i]);
    }
    for (std::size_t i = 0; i < TOP_LEVELS; ++i) {
        row += ',';
        row += std::to_string(s.ask[i]);
    }
    for (std::size_t i = 0; i < TOP_LEVELS; ++i) {
        row += ',';
        row += std::to_string(s.ask_size[i]);
    }
    row += '\n';

    file_.write(row.data(), static_cast<std::streamsize>(row.size()));
    ++rows_;
}

} // namespace bncap