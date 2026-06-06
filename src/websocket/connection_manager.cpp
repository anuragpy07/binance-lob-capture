#include "websocket/connection_manager.hpp"
#include "common/constants.hpp"
#include "common/time_utils.hpp"
#include "marketdata/stream_parser.hpp"
#include <boost/asio/ssl/context.hpp>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace bncap {

ConnectionManager::ConnectionManager(net::io_context&         ioc,
                                      Venue                    venue,
                                      std::vector<std::string> symbols,
                                      LobEngine&               engine,
                                      Metrics&                 metrics)
    : ioc_(ioc)
    , ssl_ctx_(ssl::context::tlsv12_client)
    , venue_(venue)
    , symbols_(std::move(symbols))
    , engine_(engine)
    , metrics_(metrics)
{
    ssl_ctx_.set_default_verify_paths();
    ssl_ctx_.set_verify_mode(ssl::verify_peer);
    build_handlers();
}

void ConnectionManager::build_handlers() {
    handlers_.on_depth_diff = [this](DepthDiffEvent ev){
        engine_.on_depth_diff(ev);
    };
    handlers_.on_depth5 = [this](Depth5Event ev){
        engine_.on_depth5(ev);
    };
    handlers_.on_trade = [this](TradeEvent ev){
        engine_.on_trade(ev);
    };
}

std::string build_stream_path(Venue venue, const std::vector<std::string>& symbols) {
    const std::string_view base_path =
        (venue == Venue::spot) ? SPOT_WS_PATH : USDM_WS_PATH;

    std::string path(base_path);
    bool first = true;
    for (const auto& sym : symbols) {
        std::string low = sym;
        std::transform(low.begin(), low.end(), low.begin(),
                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (!first) path += '/';
        path += low + "@depth@100ms/" + low + "@depth5@100ms/" + low + "@trade";
        first = false;
    }
    return path;
}

std::string ConnectionManager::build_url_path() const {
    return build_stream_path(venue_, symbols_);
}

void ConnectionManager::start() {
    const std::string host =
        (venue_ == Venue::spot) ? std::string(SPOT_WS_HOST)
                                : std::string(USDM_WS_HOST);
    const uint16_t port =
        (venue_ == Venue::spot) ? SPOT_WS_PORT : USDM_WS_PORT;
    const std::string path = build_url_path();

    std::cerr << "[ConnectionManager] connecting to " << host
              << ":" << port << path << '\n';

    MessageHandler msg_handler =
        [this, last_epoch = uint32_t{0}](std::string_view msg,
                                         uint32_t epoch, uint64_t seq) mutable {
            if (epoch != last_epoch) {
                // Reconnect detected: reset all books and sequence validators
                // immediately rather than waiting for the next gap to be detected.
                last_epoch = epoch;
                engine_.on_reconnect();
                std::cerr << "[ConnectionManager] epoch change to " << epoch
                          << ", books reset\n";
            }
            const RecvTs ts = split_ts(now_ns());
            if (!StreamParser::parse(msg, ts, 0, epoch, seq, venue_, handlers_))
                ++metrics_.parse_errors;
        };

    session_ = std::make_shared<WsSession>(
        ioc_, ssl_ctx_, host, port, path,
        std::move(msg_handler), metrics_);
    session_->connect();
}

void ConnectionManager::stop() {
    if (session_) session_->close();
}

} // namespace bncap
