#include "marketdata/stream_parser.hpp"
#include "common/scaled_int.hpp"
#include "common/constants.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace bncap {

using json = nlohmann::json;

static std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

static PriceLevel parse_level(const json& arr) {
    return {parse_scaled(arr[0].get<std::string>()),
            parse_scaled(arr[1].get<std::string>())};
}

static void parse_depth_diff(const json& data,
                              const std::string& symbol,
                              RecvTs ts, int32_t shard_id,
                              uint32_t conn_epoch, uint64_t conn_seq,
                              Venue venue, EventHandlers& h,
                              const std::string& payload_json) {
    if (!h.on_depth_diff) return;
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

    if (data.contains("b")) {
        for (const auto& lvl : data["b"])
            ev.bids.push_back(parse_level(lvl));
    }
    if (data.contains("a")) {
        for (const auto& lvl : data["a"])
            ev.asks.push_back(parse_level(lvl));
    }
    h.on_depth_diff(std::move(ev));
}

static void parse_depth5(const json& data,
                          const std::string& symbol,
                          RecvTs ts, int32_t shard_id,
                          uint32_t conn_epoch, uint64_t conn_seq,
                          Venue venue, EventHandlers& h,
                          const std::string& payload_json) {
    if (!h.on_depth5) return;
    Depth5Event ev;
    ev.recv_ts    = ts;
    ev.venue      = venue;
    ev.shard_id   = shard_id;
    ev.conn_epoch = conn_epoch;
    ev.conn_seq   = conn_seq;
    ev.symbol     = symbol;
    ev.payload_json = payload_json;

    // Spot format: {"lastUpdateId":..., "bids":[...], "asks":[...]}
    // USD-M format: also has "e","E","T","U","u","pu" but bids/asks are top-5
    ev.last_update_id = data.value("lastUpdateId",
                        data.value("u", uint64_t{0}));

    const auto fill_levels = [](const json& arr, PriceLevel* out, int& n) {
        n = 0;
        for (const auto& lvl : arr) {
            if (n >= 5) break;
            out[n++] = parse_level(lvl);
        }
    };

    if (data.contains("bids")) fill_levels(data["bids"], ev.bids, ev.n_bids);
    else if (data.contains("b")) fill_levels(data["b"], ev.bids, ev.n_bids);

    if (data.contains("asks")) fill_levels(data["asks"], ev.asks, ev.n_asks);
    else if (data.contains("a")) fill_levels(data["a"], ev.asks, ev.n_asks);

    h.on_depth5(std::move(ev));
}

static void parse_trade(const json& data,
                         const std::string& symbol,
                         RecvTs ts, int32_t shard_id,
                         uint32_t conn_epoch, uint64_t conn_seq,
                         Venue venue, EventHandlers& h,
                         const std::string& payload_json) {
    if (!h.on_trade) return;
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
    h.on_trade(std::move(ev));
}

bool StreamParser::parse(std::string_view raw_msg,
                         RecvTs         recv_ts,
                         int32_t        shard_id,
                         uint32_t       conn_epoch,
                         uint64_t       conn_seq,
                         Venue          venue,
                         EventHandlers& handlers) {
    json doc;
    try {
        doc = json::parse(raw_msg.begin(), raw_msg.end());
    } catch (...) {
        return false;
    }

    // Combined stream envelope: {"stream":"...", "data":{...}}
    auto sit = doc.find("stream");
    auto dit = doc.find("data");
    if (sit == doc.end() || dit == doc.end()) return false;

    const std::string& stream_name = sit->get_ref<const std::string&>();
    const json&        data        = *dit;
    std::string        payload_json = data.dump(); // minified inner object

    // Extract symbol: part before first '@'
    const auto at_pos = stream_name.find('@');
    if (at_pos == std::string::npos) return false;
    std::string symbol  = to_upper(stream_name.substr(0, at_pos));
    std::string suffix  = stream_name.substr(at_pos + 1);

    if (suffix == "trade") {
        parse_trade(data, symbol, recv_ts, shard_id, conn_epoch, conn_seq,
                    venue, handlers, payload_json);
    } else if (suffix.rfind("depth5", 0) == 0) {
        parse_depth5(data, symbol, recv_ts, shard_id, conn_epoch, conn_seq,
                     venue, handlers, payload_json);
    } else if (suffix.rfind("depth@", 0) == 0) {
        parse_depth_diff(data, symbol, recv_ts, shard_id, conn_epoch, conn_seq,
                         venue, handlers, payload_json);
    } else {
        return false; // unrecognised stream suffix
    }
    return true;
}

} // namespace bncap