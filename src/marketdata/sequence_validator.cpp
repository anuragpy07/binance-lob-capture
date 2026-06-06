#include "marketdata/sequence_validator.hpp"

namespace bncap {

bool SequenceValidator::check(const std::string& symbol,
                               uint64_t U, uint64_t u, uint64_t pu) {
    auto& st = states_[symbol];
    if (!st.initialized) {
        st.last_u       = u;
        st.initialized  = true;
        return true; // first message is always accepted
    }

    bool ok;
    if (pu != 0) {
        // USD-M: pu must equal last processed u
        ok = (pu == st.last_u);
    } else {
        // Spot: U must be exactly last_u + 1
        ok = (U == st.last_u + 1);
    }

    st.last_u = u;
    return ok;
}

void SequenceValidator::reset(const std::string& symbol) {
    states_.erase(symbol);
}

void SequenceValidator::reset_all() {
    states_.clear();
}

} // namespace bncap