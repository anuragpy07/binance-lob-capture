# Requirement Traceability Matrix

Maps each acceptance criterion and reviewer check from the assignment to the
implementing source file(s).

---

## Functional Acceptance Criteria

| # | Criterion | Implementation |
|---|-----------|----------------|
| 1 | Live capture from Binance combined streams (`depth@100ms`, `depth5@100ms`, `trade`) | `src/websocket/connection_manager.cpp` builds combined-stream URL; `src/websocket/websocket_client.cpp` TLS + WS async read loop |
| 2 | Market dump CSV matches column contract, row-per-event ordering | `src/csv/market_data_writer.cpp` — `write_row()` emits one row per event in processing order |
| 3 | Order-book CSV header exact; 26 data columns per row | `src/csv/orderbook_writer.cpp` — `OB_HEADER` + `write()`; verified by `tests/test_csv_schema.cpp` |
| 4 | Top-5 bid/ask reflect correct diff semantics (qty 0 removes level) | `src/orderbook/order_book.cpp` — `apply_levels()` erases on qty==0 |
| 5 | Integer scaling and timestamps documented; reproducible from fixed input | `src/common/scaled_int.cpp` — pure integer arithmetic; `src/replay/csv_reader.cpp` — replay path |
| 6 | Reconnect / sequence / epoch behaviour stated | `src/websocket/websocket_client.cpp` — `schedule_reconnect()`; `src/orderbook/lob_engine.cpp` — gap detection |

---

## Correctness & Market-Data Literacy

| Check | File / Function |
|-------|-----------------|
| Differential depth vs depth5 replace semantics | `OrderBook::apply_depth5()` clears maps; `apply_depth_diff()` modifies affected levels only |
| Trade stream does not update book | `LobEngine::on_trade()` annotates only; book maps unchanged |
| `U`/`u`/`pu` sequence validation | `src/marketdata/sequence_validator.cpp` — spot: `U==last_u+1`; USD-M: `pu==last_u` |
| Gap behaviour on new `conn_epoch` | `LobEngine::on_reconnect()` resets validator + all books |
| No double in hot path to CSV integers | `parse_scaled()` — integer-only, no float; scale 10^8 |
| Stable level ordering (best first) | `bids_` uses `std::greater<>` comparator; `asks_` uses default ascending |
| RFC 4180 escaping of `payload_json` | `src/csv/csv_escape.cpp` — `csv_escape_to()` |
| Exact header rows | Hard-coded `const char*` literals in both writers |

---

## C++ Quality

| Check | File |
|-------|------|
| RAII ownership of file handles | `std::ofstream` RAII destructors in both CSV writers |
| No dangling refs across async callbacks | `WsSession` uses `shared_from_this()` throughout |
| Clean shutdown on SIGINT/SIGTERM | `boost::asio::signal_set` in `main.cpp`; `ConnectionManager::stop()` closes WS |
| Error handling on all async paths | `schedule_reconnect()` handles all Beast error codes |
| `enum class` scoped types | `Venue`, `StreamKind`, `BookState` |
| `constexpr` for constants | All URL/scale values in `include/common/constants.hpp` |
| `-Wall -Wextra -Wpedantic` | `CMakeLists.txt` `add_compile_options` |

---

## Performance & Scalability

| Check | Notes |
|-------|-------|
| JSON parsed once per message | `StreamParser::parse()` calls `json::parse()` once; inner object re-serialised via `.dump()` for storage |
| Allocation reduction | `csv_escape_to()` appends to caller's buffer; `write_row()` pre-reserves 256 bytes |
| Order-book update O(log N) per level | `std::map` insert/erase; top-5 extraction O(5) by iterator |
| I/O: non-blocking network thread | All CSV writes on IO thread with `std::ofstream` default 8 KB buffer; documented in README |
| No locks on hot path | Single IO thread eliminates data-race risk without synchronisation overhead |

---

## Observability

| Check | File |
|-------|------|
| Counters (messages, errors, reconnects, rows) | `include/metrics/metrics.hpp`; printed by `Metrics::print_summary()` |
| Replay mode | `src/replay/replay_engine.cpp` + `src/replay/csv_reader.cpp` |

---

## Security

| Check | File |
|-------|------|
| No credentials in repo | Public streams only; no authentication required |
| TLS verify_peer enabled | `connection_manager.cpp` — `ssl_ctx_.set_verify_mode(ssl::verify_peer)` |
| Symbol count limit | `MAX_SYMBOLS=64` in `include/common/constants.hpp` |

---

## Test Coverage Summary

| Test file | Covers |
|-----------|--------|
| `test_csv_escape.cpp` | RFC 4180 quoting: plain, comma, quote, newline, JSON |
| `test_scaled_int.cpp` | Integer, decimal, small, zero, precision, truncation |
| `test_order_book.cpp` | apply_depth5, apply_depth_diff add/remove, reset, snapshot replacement |
| `test_sequence_validator.cpp` | Spot in-seq, spot gap, USD-M in-seq, USD-M gap, reset, multi-symbol |
| `test_csv_schema.cpp` | Exact header strings; column counts (9 market-data, 26 orderbook) |
| `test_market_data_writer.cpp` | Depth-diff/trade rows, payload_json quoting |
| `test_orderbook_writer.cpp` | 26-column data rows, field values |
| `test_replay.cpp` | Round-trip depth_diff/depth5; unknown-kind skip |