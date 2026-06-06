#pragma once
#include "websocket/websocket_client.hpp"
#include "orderbook/lob_engine.hpp"
#include "marketdata/stream_parser.hpp"
#include "metrics/metrics.hpp"
#include <string>
#include <vector>
#include <memory>

namespace bncap {

// Returns the combined-stream URL path for the given venue and symbol list,
// e.g. "/stream?streams=btcusdt@depth@100ms/btcusdt@depth5@100ms/btcusdt@trade".
// Free function so it can be unit-tested independently of network I/O.
std::string build_stream_path(Venue venue, const std::vector<std::string>& symbols);

// Manages one WebSocket session for a set of symbols on one venue.
// Builds the combined-stream URL and routes parsed events to LobEngine.
class ConnectionManager {
public:
    ConnectionManager(net::io_context& ioc,
                      Venue            venue,
                      std::vector<std::string> symbols,
                      LobEngine&       engine,
                      Metrics&         metrics);

    void start();
    void stop();

private:
    net::io_context&         ioc_;
    ssl::context             ssl_ctx_;
    Venue                    venue_;
    std::vector<std::string> symbols_;
    LobEngine&               engine_;
    Metrics&                 metrics_;
    EventHandlers            handlers_;
    std::shared_ptr<WsSession> session_;

    void build_handlers();
    std::string build_url_path() const;
};

} // namespace bncap