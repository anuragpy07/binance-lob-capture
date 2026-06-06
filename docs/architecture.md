# Architecture Overview

## Component Diagram

```
   main.cpp
    parse CLI → create writers → start ConnectionManager
    → io_context.run() → signal/timer → metrics.print()

   ConnectionManager
    Builds combined-stream URL
    Creates ssl::context (verify_peer)
    Owns WsSession
    Routes parsed events to LobEngine via EventHandlers

   WsSession  (Boost.Beast TLS WebSocket)
    Async chain: resolve -> TCP connect -> SSL handshake
                -> WS handshake -> read loop
    Reconnect with exponential back-off (1s..30s)
    Tracks conn_epoch (increments on reconnect) + conn_seq

   StreamParser  (stateless, static method)
    Parses combined-stream JSON envelope
    Routes by stream suffix:
      @depth@100ms  ->  DepthDiffEvent
      @depth5@100ms ->  Depth5Event
      @trade        ->  TradeEvent
    Stores payload_json via nlohmann::json .dump()

   LobEngine  (per-symbol dispatch hub)
    SequenceValidator   spot: U==last_u+1 | USD-M: pu==last_u
    OrderBook           bids: map<price,qty,greater> | asks: map<price,qty>
                        apply_depth5 (full replace) | apply_depth_diff (incr)
    MarketDataWriter    Deliverable A CSV
    OrderBookWriter     Deliverable B CSV

   ReplayEngine
    Reads saved market_data_*.csv via CsvReader (no network)
    Fires same LobEngine handlers -> reproduces orderbook CSV
```

## Threading Model

**Single IO thread** — `io_context::run()` runs on the main thread.

All components (WS reads, JSON parsing, order-book updates, CSV writes)
execute on this one thread with no locking required.

Trade-off: CSV writes block the read loop if the filesystem stalls.
`std::ofstream` default 8 KB kernel buffer absorbs typical micro-stalls.
A dedicated writer thread with a lock-free SPSC queue would be the natural
next step for sub-millisecond requirements; documented in `README.md`.

## Order Book State Machine

```
UNINITIALIZED  ---depth5 arrives-->  VALID
     ^                                 |
     |                                 | depth_diff gap detected
     +----  book.reset() called  ------+
            (next depth5 re-inits)
```

- **UNINITIALIZED**: Book has no snapshot. Depth diffs are recorded in the
  market-data CSV but not applied; no OB row emitted.
- **VALID**: Normal. Each processed event emits one OB snapshot row.
- On gap: book reset to UNINITIALIZED; validator cleared for the symbol;
  next depth5 re-establishes VALID state.

## File Naming Convention

```
output/
  market_data_<venue>_<SYMBOL>_<UTC-date>.csv           (Deliverable A)
  market_data_<venue>_<SYMBOL>_<UTC-date>_orderbook.csv (Deliverable B)
```

One pair per symbol. UTC date is fixed at process start time.

## Scaling

Both price and quantity are stored as `int64_t` with scale 10^8.
`parse_scaled()` performs integer arithmetic only (no `double` in hot path).

## Key Dependency Chain

```
main.cpp
  ConnectionManager -> WsSession (Boost.Beast + OpenSSL)
  LobEngine -> SequenceValidator
            -> OrderBook
            -> MarketDataWriter -> csv_escape
            -> OrderBookWriter
  ReplayEngine -> CsvReader (nlohmann/json)
  Metrics (atomic counters, printed on exit)
```