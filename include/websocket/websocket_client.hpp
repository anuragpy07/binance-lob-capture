#pragma once
#include "common/types.hpp"
#include "metrics/metrics.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace bncap {

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
namespace ssl       = net::ssl;
using     tcp       = net::ip::tcp;

// Callback invoked for each received WebSocket text frame.
// msg is valid only for the duration of the call.
using MessageHandler =
    std::function<void(std::string_view msg, uint32_t conn_epoch, uint64_t conn_seq)>;

// Async Binance WebSocket client (TLS).
// One instance = one connection to one combined-stream URL.
// Reconnects automatically with exponential back-off.
class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(net::io_context&     ioc,
              ssl::context&        ssl_ctx,
              std::string          host,
              uint16_t             port,
              std::string          path,
              MessageHandler       handler,
              Metrics&             metrics);

    void connect();
    void close();

    uint32_t conn_epoch()   const { return conn_epoch_; }
    bool     is_connected() const { return connected_; }

private:
    using ws_t = websocket::stream<beast::ssl_stream<beast::tcp_stream>>;

    net::io_context&      ioc_;
    ssl::context&         ssl_ctx_;
    tcp::resolver         resolver_;
    std::unique_ptr<ws_t> ws_;
    beast::flat_buffer    buf_;

    std::string    host_;
    uint16_t       port_;
    std::string    path_;
    MessageHandler handler_;
    Metrics&       metrics_;

    uint32_t conn_epoch_{0};
    uint64_t conn_seq_{0};
    bool     connected_{false};
    bool     closing_{false};
    int      reconnect_delay_ms_{1000};

    void do_resolve();
    void on_resolve(beast::error_code ec, tcp::resolver::results_type res);
    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type);
    void on_ssl_handshake(beast::error_code ec);
    void on_ws_handshake(beast::error_code ec);
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes);
    void schedule_reconnect();
};

} // namespace bncap