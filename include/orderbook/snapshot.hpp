#pragma once
#include <cstdint>

namespace bncap {

// One row of the order-book CSV (Deliverable B)
struct OBSnapshot {
    int64_t  tsec{0};
    int32_t  tnsec{0};
    uint64_t seqno{0};
    int32_t  id{0};    // stable numeric instrument id
    char     type{'D'}; // D=depth_diff, F=depth5, T=trade
    char     side{'N'}; // B=bid, S=ask, N=symmetric/N/A
    int64_t  bid[5]{};
    int64_t  bid_size[5]{};
    int64_t  ask[5]{};
    int64_t  ask_size[5]{};
};

} // namespace bncap