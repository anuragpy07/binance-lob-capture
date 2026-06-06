#pragma once
#include <string>
#include <string_view>

namespace bncap {

// RFC 4180: wrap in double-quotes if the field contains comma, quote, or newline.
// Internal double-quotes are doubled: " -> "".
std::string csv_escape(std::string_view field);

// Append RFC 4180-escaped field to buf without a heap allocation for clean fields.
void csv_escape_to(std::string& buf, std::string_view field);

} // namespace bncap