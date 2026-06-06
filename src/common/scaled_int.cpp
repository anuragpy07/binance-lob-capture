#include "common/scaled_int.hpp"

namespace bncap {

int64_t parse_scaled(std::string_view s, int scale_exp) {
    if (s.empty()) return 0;

    bool negative = (s[0] == '-');
    if (negative) s = s.substr(1);
    if (s.empty()) return 0;

    const std::size_t dot_pos = s.find('.');

    int64_t integer_part = 0;
    {
        const auto int_str = (dot_pos == std::string_view::npos) ? s : s.substr(0, dot_pos);
        for (char c : int_str) {
            integer_part = integer_part * 10 + static_cast<int64_t>(c - '0');
        }
    }

    int64_t frac_part   = 0;
    int     frac_digits = 0;
    if (dot_pos != std::string_view::npos) {
        for (char c : s.substr(dot_pos + 1)) {
            if (frac_digits >= scale_exp) break;
            frac_part = frac_part * 10 + static_cast<int64_t>(c - '0');
            ++frac_digits;
        }
    }
    // Pad fractional part to scale_exp digits
    while (frac_digits < scale_exp) {
        frac_part *= 10;
        ++frac_digits;
    }

    // Compute integer scale multiplier
    int64_t scale = 1;
    for (int i = 0; i < scale_exp; ++i) scale *= 10;

    const int64_t result = integer_part * scale + frac_part;
    return negative ? -result : result;
}

} // namespace bncap