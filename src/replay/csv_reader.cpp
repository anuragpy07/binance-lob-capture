#include "replay/csv_reader.hpp"
#include "common/scaled_int.hpp"
#include "common/types.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace bncap {

using json = nlohmann::json;

// Split a CSV line respecting RFC-4180 quoting (unquotes in-place).
// Does NOT handle embedded newlines in quoted fields.
static std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field.push_back('"');
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                field.push_back(c);
            }
        } else {
            if (c == '"') {
                in_quotes = true;
            } else if (c == ',') {
                fields.push_back(std::move(field));
                field.clear();
            } else {
                field.push_back(c);
            }
        }
    }
    fields.push_back(std::move(field));
    return fields;
}

static Venue parse_venue(const std::string& s) {
    return (s == "usdm") ? Venue::usdm : Venue::spot;
}

uint64_t read_market_csv(const std::string& path, ReplayHandlers& handlers) {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("read_market_csv: cannot open " + path);

    std::string line;
    // Skip header
    if (!std::getline(file, line))
        return 0;

    uint64_t count = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        auto fields = split_csv_line(line);
        if (fields.size() < 9) continue; // malformed

        // Column order: recv_tsec,recv_tnsec,venue,stream_kind,shard_id,
        //               conn_epoch,conn_seq,symbol,payload_json
        RecvTs ts;
        try {
            ts.tsec  = std::stoll(fields[0]);
            ts.tnsec = std::stoi(fields[1]);
        } catch (...) { continue; }

        const Venue       venue       = parse_venue(fields[2]);
        const std::string kind_str    = fields[3];
        int32_t           shard_id    = 0;
        uint32_t          conn_epoch  = 0;
        uint64_t          conn_seq    = 0;
        try {
            shard_id   = std::stoi(fields[4]);
            conn_epoch = static_cast<uint32_t>(std::stoul(fields[5]));
            conn_seq   = std::stoull(fields[6]);
        } catch (...) {}

        const std::string symbol       = fields[7];
        const std::string payload_json = fields[8];

        if (kind_str == "depth_diff" && handlers.on_depth_diff) {
            // Re-parse the stored payload JSON to reconstruct the event
            json data;
            try { data = json::parse(payload_json); } catch (...) { continue; }

            DepthDiffEvent ev;
            ev.recv_ts    = ts;
            ev.venue      = venue;
            ev.shard_id   = shard_id;
            ev.conn_epoch = conn_epoch;
            ev.conn_seq   = conn_seq;
            ev.symbol     = symbol;
            ev.payload_json = payload_json;
            ev.first_update_id = data.value("U", uint64_t{0});
            ev.last_update_id  = data.value("u", uint64_t{0});
            ev.prev_update_id  = data.value("pu", uint64_t{0});
            for (const auto& lvl : data.value("b", json::array()))
                ev.bids.push_back({parse_scaled(lvl[0].get<std::string>()),
                                   parse_scaled(lvl[1].get<std::string>())});
            for (const auto& lvl : data.value("a", json::array()))
                ev.asks.push_back({parse_scaled(lvl[0].get<std::string>()),
                                   parse_scaled(lvl[1].get<std::string>())});
            handlers.on_depth_diff(std::move(ev));

        } else if (kind_str == "depth5" && handlers.on_depth5) {
            json data;
            try { data = json::parse(payload_json); } catch (...) { continue; }

            Depth5Event ev;
            ev.recv_ts    = ts;
            ev.venue      = venue;
            ev.shard_id   = shard_id;
            ev.conn_epoch = conn_epoch;
            ev.conn_seq   = conn_seq;
            ev.symbol     = symbol;
            ev.payload_json = payload_json;
            ev.last_update_id = data.value("lastUpdateId", data.value("u", uint64_t{0}));

            int i = 0;
            for (const auto& lvl : data.value("bids", data.value("b", json::array()))) {
                if (i >= 5) break;
                ev.bids[i++] = {parse_scaled(lvl[0].get<std::string>()),
                                parse_scaled(lvl[1].get<std::string>())};
            }
            ev.n_bids = i;
            i = 0;
            for (const auto& lvl : data.value("asks", data.value("a", json::array()))) {
                if (i >= 5) break;
                ev.asks[i++] = {parse_scaled(lvl[0].get<std::string>()),
                                parse_scaled(lvl[1].get<std::string>())};
            }
            ev.n_asks = i;
            handlers.on_depth5(std::move(ev));

        } else if (kind_str == "trade" && handlers.on_trade) {
            json data;
            try { data = json::parse(payload_json); } catch (...) { continue; }

            TradeEvent ev;
            ev.recv_ts    = ts;
            ev.venue      = venue;
            ev.shard_id   = shard_id;
            ev.conn_epoch = conn_epoch;
            ev.conn_seq   = conn_seq;
            ev.symbol     = symbol;
            ev.payload_json = payload_json;
            ev.trade_id  = data.value("t", uint64_t{0});
            ev.price     = parse_scaled(data.value("p", std::string{"0"}));
            ev.qty       = parse_scaled(data.value("q", std::string{"0"}));
            ev.is_buyer_market_maker = data.value("m", false);
            handlers.on_trade(std::move(ev));
        }
        ++count;
    }
    return count;
}

} // namespace bncap