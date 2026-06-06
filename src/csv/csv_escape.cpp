#include "csv/csv_escape.hpp"

namespace bncap {

static bool needs_quoting(std::string_view s) {
    for (char c : s) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') return true;
    }
    return false;
}

std::string csv_escape(std::string_view field) {
    if (!needs_quoting(field)) return std::string(field);
    std::string out;
    out.reserve(field.size() + 8);
    csv_escape_to(out, field);
    return out;
}

void csv_escape_to(std::string& buf, std::string_view field) {
    if (!needs_quoting(field)) {
        buf.append(field);
        return;
    }
    buf.push_back('"');
    for (char c : field) {
        if (c == '"') buf.push_back('"'); // double the quote
        buf.push_back(c);
    }
    buf.push_back('"');
}

} // namespace bncap