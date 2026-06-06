#include "csv/market_data_writer.hpp"
#include "csv/csv_escape.hpp"
#include "common/types.hpp"
#include <stdexcept>
#include <string>

namespace bncap {

static const char* MARKET_DATA_HEADER =
    "recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json\n";

MarketDataWriter::~MarketDataWriter() { close(); }

void MarketDataWriter::open(const std::string& path) {
    file_.open(path, std::ios::out | std::ios::trunc);
    if (!file_.is_open())
        throw std::runtime_error("MarketDataWriter: cannot open " + path);
    file_ << MARKET_DATA_HEADER;
}

void MarketDataWriter::close() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

void MarketDataWriter::write_row(RecvTs             ts,
                                  Venue              venue,
                                  StreamKind         kind,
                                  int32_t            shard_id,
                                  uint32_t           conn_epoch,
                                  uint64_t           conn_seq,
                                  const std::string& symbol,
                                  const std::string& payload_json) {
    // Build row in a local string to minimise syscall count
    std::string row;
    row.reserve(256);
    row += std::to_string(ts.tsec);
    row += ',';
    row += std::to_string(ts.tnsec);
    row += ',';
    row += venue_str(venue);
    row += ',';
    row += stream_kind_str(kind);
    row += ',';
    row += std::to_string(shard_id);
    row += ',';
    row += std::to_string(conn_epoch);
    row += ',';
    row += std::to_string(conn_seq);
    row += ',';
    row += symbol;
    row += ',';
    csv_escape_to(row, payload_json);
    row += '\n';

    file_.write(row.data(), static_cast<std::streamsize>(row.size()));
    ++rows_;
}

void MarketDataWriter::write_depth_diff(const DepthDiffEvent& ev) {
    write_row(ev.recv_ts, ev.venue, StreamKind::depth_diff,
              ev.shard_id, ev.conn_epoch, ev.conn_seq, ev.symbol, ev.payload_json);
}

void MarketDataWriter::write_depth5(const Depth5Event& ev) {
    write_row(ev.recv_ts, ev.venue, StreamKind::depth5,
              ev.shard_id, ev.conn_epoch, ev.conn_seq, ev.symbol, ev.payload_json);
}

void MarketDataWriter::write_trade(const TradeEvent& ev) {
    write_row(ev.recv_ts, ev.venue, StreamKind::trade,
              ev.shard_id, ev.conn_epoch, ev.conn_seq, ev.symbol, ev.payload_json);
}

} // namespace bncap