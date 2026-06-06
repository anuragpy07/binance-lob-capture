#include "common/signal_handler.hpp"
#include <csignal>

namespace bncap {

std::atomic<bool> g_stop{false};

namespace {
    std::function<void()> g_on_stop;

    void handle_signal(int /*sig*/) {
        g_stop.store(true, std::memory_order_relaxed);
        if (g_on_stop) g_on_stop();
    }
}

void install_signal_handlers(std::function<void()> on_stop) {
    g_on_stop = std::move(on_stop);
    std::signal(SIGINT,  handle_signal);
    std::signal(SIGTERM, handle_signal);
}

} // namespace bncap