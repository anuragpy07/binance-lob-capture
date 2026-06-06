#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

namespace bncap {

// Tracks per-symbol depth-diff sequence numbers to detect gaps.
// Spot:   gap when U[n] != u[n-1] + 1
// USD-M:  gap when pu[n] != u[n-1]
class SequenceValidator {
public:
    // Returns true if the message is in sequence (or is the first message).
    // `pu` is the USD-M `pu` field; pass 0 for spot.
    bool check(const std::string& symbol, uint64_t U, uint64_t u, uint64_t pu = 0);

    // Reset tracking for a symbol (e.g. on reconnect)
    void reset(const std::string& symbol);

    // Reset all symbols
    void reset_all();

private:
    struct State {
        uint64_t last_u{0};
        bool     initialized{false};
    };
    std::unordered_map<std::string, State> states_;
};

} // namespace bncap