#pragma once
#include <atomic>
#include <functional>

namespace bncap {

extern std::atomic<bool> g_stop;

// Install SIGINT/SIGTERM handlers; on_stop() called (async-signal-safe only)
void install_signal_handlers(std::function<void()> on_stop = {});

} // namespace bncap