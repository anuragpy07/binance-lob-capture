#pragma once
#include "common/types.hpp"
#include <cstdint>
#include <string>

namespace bncap {

int64_t now_ns();
RecvTs  split_ts(int64_t ns_epoch);
std::string utc_date_str(int64_t tsec);

} // namespace bncap