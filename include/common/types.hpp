#pragma once
#include <cstdint>
#include <string>

namespace bncap {

enum class Venue : uint8_t { spot = 0, usdm = 1 };

enum class StreamKind : uint8_t {
    depth_diff = 0,
    depth5     = 1,
    trade      = 2
};

struct RecvTs {
    int64_t tsec{0};
    int32_t tnsec{0};
};

inline const char* venue_str(Venue v) {
    return v == Venue::spot ? "spot" : "usdm";
}

inline const char* stream_kind_str(StreamKind k) {
    switch (k) {
        case StreamKind::depth_diff: return "depth_diff";
        case StreamKind::depth5:     return "depth5";
        case StreamKind::trade:      return "trade";
    }
    return "unknown";
}

} // namespace bncap