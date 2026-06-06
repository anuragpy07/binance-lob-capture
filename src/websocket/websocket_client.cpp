#include "websocket/websocket_client.hpp"
#include <boost/asio/steady_timer.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>

namespace bncap {

WsSession::WsSession(net::io_context& ioc,
                     ssl::context&    ssl_ctx,
                     std::string      host,
                     uint16_t         port,
                     std::string      path,
                     MessageHandler   handler,
                     Metrics&         metrics)
    : ioc_(ioc)
    , ssl_ctx_(ssl_ctx)
    , resolver_(ioc)
    , host_(std::move(host))
    , port_(port)
    , path_(std::move(path))
    , handler_(std::move(handler))
    , metrics_(metrics)
{}

void WsSession::connect() {
    if (closing_) return;
    conn_seq_    = 0;
    connected_   = false;
    do_resolve();
}

void WsSession::close() {
    closing_ = true;
    if (ws_ && ws_->is_open()) {
        beast::error_code ec;
        ws_->close(websocket::close_code::normal, ec);
    }
}

void WsSession::do_resolve() {
    resolver_.async_resolve(
        host_,
        std::to_string(port_),
        beast::bind_front_handler(&WsSession::on_resolve, shared_from_this()));
}

void WsSession::on_resolve(beast::error_code ec,
                            tcp::resolver::results_type res) {
    if (ec) {
        std::cerr << "[WsSession] resolve error: " << ec.message() << '\n';
        schedule_reconnect();
        return;
    }
    // Create a fresh websocket stream
    ws_ = std::make_unique<ws_t>(ioc_, ssl_ctx_);

    // Set SNI hostname for TLS
    if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(),
                                   host_.c_str())) {
        std::cerr << "[WsSession] SSL_set_tlsext_host_name failed\n";
        schedule_reconnect();
        return;
    }

    beast::get_lowest_layer(*ws_).async_connect(
        res,
        beast::bind_front_handler(&WsSession::on_connect, shared_from_this()));
}

void WsSession::on_connect(beast::error_code ec,
                             tcp::resolver::results_type::endpoint_type) {
    if (ec) {
        std::cerr << "[WsSession] connect error: " << ec.message() << '\n';
        schedule_reconnect();
        return;
    }
    // Set a TCP no-delay option for lower latency
    beast::get_lowest_layer(*ws_).socket().set_option(
        net::ip::tcp::no_delay(true));

    ws_->next_layer().async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(&WsSession::on_ssl_handshake, shared_from_this()));
}

void WsSession::on_ssl_handshake(beast::error_code ec) {
    if (ec) {
        std::cerr << "[WsSession] SSL handshake error: " << ec.message() << '\n';
        schedule_reconnect();
        return;
    }
    // Increase receive buffer for large depth snapshots
    ws_->read_message_max(16 * 1024 * 1024);

    ws_->async_handshake(
        host_, path_,
        beast::bind_front_handler(&WsSession::on_ws_handshake, shared_from_this()));
}

void WsSession::on_ws_handshake(beast::error_code ec) {
    if (ec) {
        std::cerr << "[WsSession] WebSocket handshake error: " << ec.message() << '\n';
        schedule_reconnect();
        return;
    }
    connected_           = true;
    reconnect_delay_ms_  = 1000; // reset backoff on successful connect
    std::cerr << "[WsSession] connected (epoch=" << conn_epoch_ << ")\n";
    do_read();
}

void WsSession::do_read() {
    buf_.clear();
    ws_->async_read(
        buf_,
        beast::bind_front_handler(&WsSession::on_read, shared_from_this()));
}

void WsSession::on_read(beast::error_code ec, std::size_t /*bytes*/) {
    if (ec) {
        connected_ = false;
        if (!closing_) {
            std::cerr << "[WsSession] read error: " << ec.message() << '\n';
            schedule_reconnect();
        }
        return;
    }

    const auto msg = beast::buffers_to_string(buf_.data());
    buf_.consume(buf_.size());

    handler_(msg, conn_epoch_, conn_seq_++);
    do_read();
}

void WsSession::schedule_reconnect() {
    if (closing_) return;
    ++conn_epoch_;
    ++metrics_.reconnects;
    connected_ = false;

    const int delay = reconnect_delay_ms_;
    // Exponential backoff capped at RECONNECT_MAX_MS (use integer constants)
    reconnect_delay_ms_ = std::min(reconnect_delay_ms_ * 2, 30000);

    std::cerr << "[WsSession] reconnecting in " << delay
              << " ms (epoch=" << conn_epoch_ << ")\n";

    auto timer = std::make_shared<net::steady_timer>(ioc_);
    timer->expires_after(std::chrono::milliseconds(delay));
    timer->async_wait([self = shared_from_this(), timer](beast::error_code ec2) {
        if (!ec2 && !self->closing_)
            self->connect();
    });
}

} // namespace bncap