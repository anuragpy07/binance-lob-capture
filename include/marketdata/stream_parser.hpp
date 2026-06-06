#pragma once
#include "marketdata/event.hpp"
#include "common/types.hpp"
#include <functional>
#include <string_view>

namespace bncap {

// Callbacks for each parsed event type
struct EventHandlers {
    std::function<void(DepthDiffEvent)> on_depth_diff;
    std::function<void(Depth5Event)>    on_depth5;
    std::function<void(TradeEvent)>     on_trade;
};

class StreamParser {
public:
    // Parse a raw combined-stream WebSocket message.
    // Calls the appropriate handler in `handlers` for each recognized event.
    // Returns false on JSON parse error.
    static bool parse(std::string_view raw_msg,
                      RecvTs         recv_ts,
                      int32_t        shard_id,
                      uint32_t       conn_epoch,
                      uint64_t       conn_seq,
                      Venue          venue,
                      EventHandlers& handlers);
};

} // namespace bncap