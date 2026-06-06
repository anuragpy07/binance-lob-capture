#include "test_framework.hpp"
#include "csv/csv_escape.hpp"

TEST(csv_escape_plain) {
    REQUIRE_STR_EQ(bncap::csv_escape("hello"), "hello");
}

TEST(csv_escape_with_comma) {
    REQUIRE_STR_EQ(bncap::csv_escape("a,b"), "\"a,b\"");
}

TEST(csv_escape_with_quote) {
    // hello"world -> "hello""world"
    REQUIRE_STR_EQ(bncap::csv_escape("hello\"world"), "\"hello\"\"world\"");
}

TEST(csv_escape_with_newline) {
    REQUIRE_STR_EQ(bncap::csv_escape("a\nb"), "\"a\nb\"");
}

TEST(csv_escape_json_payload) {
    // A typical Binance JSON payload contains commas and should be quoted
    const std::string payload = R"({"e":"depthUpdate","s":"BTCUSDT","U":1,"u":2})";
    const std::string escaped = bncap::csv_escape(payload);
    REQUIRE(escaped.front() == '"');
    REQUIRE(escaped.back()  == '"');
}

TEST(csv_escape_to_appends) {
    std::string buf = "prefix,";
    bncap::csv_escape_to(buf, "no special");
    REQUIRE_STR_EQ(buf, "prefix,no special");
}

TEST(csv_escape_to_wraps_commas) {
    std::string buf;
    bncap::csv_escape_to(buf, "a,b,c");
    REQUIRE_STR_EQ(buf, "\"a,b,c\"");
}