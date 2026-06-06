#pragma once
#include <cstdint>
#include <string_view>

namespace bncap {

inline constexpr int64_t PRICE_SCALE = 100'000'000LL;
inline constexpr int64_t QTY_SCALE   = 100'000'000LL;
inline constexpr int     SCALE_EXP   = 8;

inline constexpr std::string_view SPOT_WS_HOST = "stream.binance.com";
inline constexpr uint16_t         SPOT_WS_PORT = 9443;
inline constexpr std::string_view SPOT_WS_PATH = "/stream?streams=";

inline constexpr std::string_view USDM_WS_HOST = "fstream.binance.com";
inline constexpr uint16_t         USDM_WS_PORT = 443;
inline constexpr std::string_view USDM_WS_PATH = "/public/stream?streams=";

inline constexpr int RECONNECT_BASE_MS = 1000;
inline constexpr int RECONNECT_MAX_MS  = 30000;

inline constexpr std::size_t MAX_SYMBOLS = 64;
inline constexpr std::size_t TOP_LEVELS  = 5;

} // namespace bncap