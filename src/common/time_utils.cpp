#include "common/time_utils.hpp"
#include <chrono>
#include <ctime>

namespace bncap {

int64_t now_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
}

RecvTs split_ts(int64_t ns) {
    return {static_cast<int64_t>(ns / 1'000'000'000LL),
            static_cast<int32_t>(ns % 1'000'000'000LL)};
}

std::string utc_date_str(int64_t tsec) {
    std::time_t t = static_cast<std::time_t>(tsec);
    struct tm tm_buf{};
    gmtime_r(&t, &tm_buf);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm_buf);
    return buf;
}

} // namespace bncap