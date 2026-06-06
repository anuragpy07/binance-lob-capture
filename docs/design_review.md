# Design Review Notes

Self-assessment against the reviewer checklist from the assignment.

---

## Correctness

### Stream semantics

`depth@100ms` messages are treated as incremental diffs: `apply_depth_diff()`
calls `apply_levels()` which inserts/updates or erases (on qty==0) individual
price levels. `depth5@100ms` triggers a full replacement of the book's
bid/ask maps (`apply_depth5()` calls `clear()` first). Trades are recorded
in the market-data CSV and produce an annotating OB row but do not modify
the book — consistent with the spec.

### Sequence / gap handling

`SequenceValidator` tracks one state per symbol:
- **Spot**: expects `U[n] == last_u[n-1] + 1`
- **USD-M**: expects `pu[n] == last_u[n-1]`

On first message (or after reset): always accepted and state initialised.
On gap: `books_[sym].reset()` + `seq_validator_.reset(sym)`. The book enters
UNINITIALIZED and no OB rows are emitted until the next `depth5` snapshot.

### `conn_epoch` lifecycle

`WsSession::schedule_reconnect()` increments `conn_epoch_` and resets
`conn_seq_` to 0. `ConnectionManager`'s message handler compares the incoming
epoch against the last-seen epoch; on change it calls `engine_.on_reconnect()`
before routing the message, then updates `last_epoch`. This means all books
are reset on the **first message** of a new conn_epoch rather than
synchronously at the moment of TCP disconnect.

---

## C++ Quality

### RAII

`MarketDataWriter` and `OrderBookWriter` flush and close in their destructors.
`WsSession` is managed via `shared_ptr` so it outlives all pending async
callbacks. `ssl::context` is owned by `ConnectionManager`.

### Async lifetimes

`WsSession` inherits `enable_shared_from_this<WsSession>`. Every async handler
captures `shared_from_this()`, preventing premature destruction while I/O is
pending.

### Signal handling

`boost::asio::signal_set` is used in `main.cpp` for async-signal-safe
SIGINT/SIGTERM handling. This avoids calling non-signal-safe functions
(like `std::function` or `io_context::stop()`) from a POSIX signal handler.
`signal_handler.cpp` also sets `g_stop` (atomic) for replay mode, which runs
synchronously outside Asio.

### Modern C++

- C++17 structured bindings (`auto& [price, qty]`) in `order_book.cpp`
- `std::filesystem::create_directories` in main and replay engine
- `std::string_view` parameters throughout hot-path functions
- `enum class` for `Venue`, `StreamKind`, `BookState`
- `inline constexpr` for all compile-time constants

---

## Performance

### JSON parsing

`nlohmann::json` is used for correctness and ease of development. Each message
is parsed once via `json::parse()`; the inner data object is re-serialised
via `.dump()` for the `payload_json` column. This re-serialisation is the
main allocation cost per message.

Optimisation path: replace with `simdjson` (zero-copy, SIMD-accelerated) and
extract `payload_json` as a raw substring of the original buffer to eliminate
the re-serialisation allocation.

### Order book

`std::map` gives O(log N) per level insert/erase. For BTC the typical book
depth is a few hundred levels, so log N ≈ 8. Top-5 extraction is O(5) by
iterating from `begin()`.

A flat sorted array (e.g. `std::vector` kept sorted with `lower_bound`)
would improve cache locality but complicate insert/erase. The current map
approach is simple and correctness-first.

### CSV I/O

One `write_row()` builds a local `std::string` (pre-reserved to 256 bytes)
and calls `file_.write()` once. `std::ofstream` buffers writes in the kernel.
No per-field `<<` operator chaining avoids multiple small writes.

---

## Known Limitations / Future Work

1. **`on_reconnect()` wired lazily, not eagerly**: `LobEngine::on_reconnect()`
   resets all books and is called by `ConnectionManager` when the first
   message of a new conn_epoch is received, not at the instant of TCP
   disconnect. In the single-IO-thread model this is race-free and
   functionally equivalent to an eager call, but adds one message of latency
   before the reset is visible. A future improvement would be to add an
   explicit reconnect callback to `WsSession`.

2. **No REST snapshot resync**: The book is initialised only from
   `depth5@100ms` (top 5 levels). The Binance-recommended approach uses a
   REST `GET /api/v3/depth?limit=1000` call to seed the full book. This is
   marked as optional stretch in the assignment.

3. **Embedded newlines in `payload_json`**: The CSV reader does not support
   RFC 4180 quoted fields that span multiple lines. In practice Binance JSON
   payloads never contain literal newlines, so this is not an issue.

4. **Multi-shard scaling**: A single WebSocket connection handles all symbols.
   Binance supports up to 1024 streams per connection; for larger deployments,
   multiple shards (one WS per symbol group) with distinct `shard_id` values
   would be used.