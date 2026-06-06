# Requirements Summary

Condensed from the assignment specification.

---

## Functional Requirements

### Streams (per symbol)

| Stream | Purpose |
|--------|---------|
| `<sym>@depth@100ms` | Differential depth update (`depthUpdate`) |
| `<sym>@depth5@100ms` | Partial top-of-book snapshot (sanity / refresh) |
| `<sym>@trade` | Individual trade |

### Venues

- **Spot**: `wss://stream.binance.com:9443/stream?streams=`
- **USD-M**: `wss://fstream.binance.com/public/stream?streams=`

---

## Deliverable A — Market-data CSV

**Filename**: `market_data_<venue>_<SYMBOL>_<UTC-date>.csv`

**Header** (9 columns):
```
recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json
```

| Column | Type | Description |
|--------|------|-------------|
| recv_tsec | int64 | Wall-clock seconds |
| recv_tnsec | int32 | Nanosecond remainder [0, 999_999_999] |
| venue | string | `spot` or `usdm` |
| stream_kind | string | `depth_diff`, `depth5`, or `trade` |
| shard_id | int | Connection shard index (0 for single) |
| conn_epoch | uint | Increments on each reconnect |
| conn_seq | uint64 | Monotonic within (shard_id, conn_epoch) |
| symbol | string | Uppercase, e.g. `BTCUSDT` |
| payload_json | string | Inner Binance data object, minified, RFC 4180 escaped |

---

## Deliverable B — Order-book CSV

**Filename**: `market_data_<venue>_<SYMBOL>_<UTC-date>_orderbook.csv`

**Header** (26 columns):
```
tsec,tnsec,seqNo,id,type,side,
bid0,bid1,bid2,bid3,bid4,
bid_size0,bid_size1,bid_size2,bid_size3,bid_size4,
ask0,ask1,ask2,ask3,ask4,
ask_size0,ask_size1,ask_size2,ask_size3,ask_size4
```

| Column | Type | Description |
|--------|------|-------------|
| tsec | int64 | Row timestamp seconds |
| tnsec | int32 | Nanosecond remainder |
| seqNo | uint64 | Monotonic application sequence |
| id | int32 | Stable numeric instrument id (0-based insertion order) |
| type | char | `D`=depth_diff, `F`=depth5, `T`=trade |
| side | char | `B`=bid, `S`=ask, `N`=symmetric/N/A |
| bid0–bid4 | int64 | Top 5 bid prices (scaled) |
| bid_size0–bid_size4 | int64 | Top 5 bid quantities (scaled) |
| ask0–ask4 | int64 | Top 5 ask prices (scaled) |
| ask_size0–ask_size4 | int64 | Top 5 ask quantities (scaled) |

---

## Timestamp Policy

Wall-clock time recorded when the process finishes reading each message from
the socket (`std::chrono::system_clock::now()`), split:

- `recv_tsec` / `tsec` = `ns_epoch / 1_000_000_000`
- `recv_tnsec` / `tnsec` = `ns_epoch % 1_000_000_000`

---

## Integer Scaling Policy

All prices and quantities stored as `int64_t` with scale **10^8**:

```
"103456.78" → 10345678000000
"0.00001"   → 1000
"1.0"       → 100000000
```

No floating-point in the hot path. `parse_scaled()` uses integer arithmetic only.

---

## Reconnect Policy

| Event | Action |
|-------|--------|
| WebSocket disconnect | `conn_epoch++`; exponential back-off 1 s → 30 s; reconnect indefinitely |
| Sequence gap (spot: `U[n] != u[n-1]+1`; USD-M: `pu[n] != u[n-1]`) | Book reset to UNINITIALIZED; validator cleared; wait for next depth5 |
| depth5 received after gap | Book re-initialised from top-5 snapshot |
| `conn_epoch` changes | `conn_seq` resets to 0; `LobEngine::on_reconnect()` called |

---

## Optional Stretch (not implemented)

- REST depth snapshot + diff buffer resync per full Binance LOB construction algorithm
- Multi-shard connections (one WS per symbol group)
- Performance benchmarks (throughput / latency)