# binance-lob-capture

C++17 Binance WebSocket market-data capture and local order-book (LOB) engine.

## Compiler & Standard

| Item | Value |
|---|---|
| Compiler | GCC 12+ (`g++-12` on Ubuntu 22.04, `g++` / GCC 13 on Ubuntu 24.04) |
| C++ Standard | C++17 |
| Build system | CMake 3.16+ with Ninja |

## Dependencies

```bash
# Ubuntu 22.04 / Debian
sudo apt-get update
sudo apt-get install -y \
    cmake ninja-build \
    g++-12 \
    libssl-dev \
    libboost-all-dev \
    zlib1g-dev \
    nlohmann-json3-dev
```

## Build

```bash
git clone https://github.com/<your-username>/binance-lob-capture.git
cd binance-lob-capture

# Ubuntu 22.04 (GCC 12):
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=g++-12

# Ubuntu 24.04 (GCC 13 default):
# cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

cmake --build build --parallel
```

Debug build with ASan/UBSan:
```bash
cmake -B build-debug -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER=g++-12
cmake --build build-debug --parallel
```

## CLI

### Live capture (both CSVs)

```bash
# Single symbol, spot, timed 120 s
./build/binance_capture \
    --venue spot \
    --symbols BTCUSDT \
    --output-dir ./output \
    --duration 120

# Multiple symbols
./build/binance_capture \
    --venue spot \
    --symbols BTCUSDT,ETHUSDT \
    --output-dir ./output

# USD-M futures
./build/binance_capture \
    --venue usdm \
    --symbols BTCUSDT \
    --output-dir ./output
```

### Replay mode (regenerates *_orderbook.csv without network)

```bash
./build/binance_capture \
    --replay ./output/market_data_spot_BTCUSDT_2025-01-15.csv \
    --output-dir ./output

# Test with the included sample file (no live connection required)
./build/binance_capture \
    --replay ./sample_output/market_data_spot_BTCUSDT_2025-06-03.csv \
    --output-dir /tmp/replay_out
```

In replay mode **no WebSocket connections are made**. The binary reads the
saved market-data CSV and replays every row through the LOB engine, emitting
`*_orderbook.csv` into `--output-dir`.

## Output files

One file-pair per symbol per UTC calendar day:

```
output/
  market_data_spot_BTCUSDT_2025-01-15.csv
  market_data_spot_BTCUSDT_2025-01-15_orderbook.csv
```

### Market-data CSV header (Deliverable A)

```
recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json
```

### Order-book CSV header (Deliverable B)

```
tsec,tnsec,seqNo,id,type,side,bid0,bid1,bid2,bid3,bid4,bid_size0,bid_size1,bid_size2,bid_size3,bid_size4,ask0,ask1,ask2,ask3,ask4,ask_size0,ask_size1,ask_size2,ask_size3,ask_size4
```

26 data columns per row.

## Symbol list format

Comma-separated uppercase symbols, e.g. `BTCUSDT,ETHUSDT,SOLUSDT`.

## Scaling policy

Binance delivers prices and quantities as decimal strings. This implementation
uses **fixed-point integer scaling at 10^8** for both prices and quantities:

| Value | Stored as |
|---|---|
| `"103456.78"` | `10345678000000` |
| `"0.00001234"` | `1234` |

`parse_scaled(s, 8)` converts without going through IEEE-754 floating point,
ensuring deterministic, lossless integer output in both CSV files.

## Timestamp policy

**Wall-clock time** (`CLOCK_REALTIME` via `std::chrono::system_clock`) is
recorded when the process finishes reading each message off the socket. The
nanosecond epoch value is split into:

- `recv_tsec` / `tsec` - seconds part (`ns / 1e9`)
- `recv_tnsec` / `tnsec` - nanosecond remainder (`ns % 1e9`)

The same `recv_ts` is reused for the corresponding order-book row, so both
CSVs share a consistent timestamp for each event.

## Reconnect / gap handling

| Trigger | Behaviour |
|---|---|
| WebSocket disconnect | `conn_epoch` incremented; `on_reconnect()` called on first message of new conn_epoch → all books reset to **UNINITIALIZED**; exponential backoff 1 s→30 s cap; reconnects indefinitely |
| Sequence gap (spot: `U[n] != u[n-1]+1`; USD-M: `pu[n] != u[n-1]`) | Book reset to **UNINITIALIZED**; `gaps_detected` counter incremented; all depth-diff rows dropped until next `depth5` refresh |
| `depth5` received | Book set to **VALID**; sequence validator reset to uninitialized (first following diff is accepted unconditionally as anchor); top-5 levels replaced in full; deeper levels accumulate from subsequent diffs |

`conn_seq` resets to 0 on each new `conn_epoch`. `shard_id` is 0 for
single-connection operation (all symbols share one combined stream URL).

The sequence validator is **reset to uninitialized on every `depth5` event**. The
first depth-diff after a depth5 is always accepted as the new sequence anchor (its
`u` field becomes `last_u`). This avoids false gaps caused by the Binance combined
stream delivering a depth5 snapshot whose `lastUpdateId` is slightly behind the
already-in-flight diff events.

## type / side mapping in order-book CSV

| Event | `type` | `side` |
|---|---|---|
| `depthUpdate` (`@depth@100ms`) | `D` | `N` |
| depth5 snapshot (`@depth5@100ms`) | `F` | `N` |
| trade (`@trade`) | `T` | `N` |

Trades annotate the current book state without modifying it; a row is emitted
with the book snapshot at that moment.

## Instrument ID derivation

`id` is a **session-stable sequential integer** assigned in the order symbols
are first seen (starting at 0). E.g. if the program is started with
`--symbols BTCUSDT,ETHUSDT` then BTCUSDT=0, ETHUSDT=1. This mapping is
printed to stderr at startup and re-derived deterministically in replay mode
from the order of first occurrence in the market-data CSV.

## Threading & I/O design

- **Single IO thread**: Boost.Asio `io_context` runs one thread. All WebSocket
  reads, JSON parsing, order-book updates, and CSV writes happen on this
  thread.
- **Buffered CSV writes**: `std::ofstream` uses its default 8 KB kernel buffer.
  No separate writer thread is used; the trade-off is simplicity and
  correctness (no data-race risk) at the cost of occasional OS write-back
  latency. For microsecond-latency requirements a lock-free SPSC queue plus
  dedicated writer thread would be appropriate - noted as a future improvement.
- **Signal handling**: `SIGINT`/`SIGTERM` set an `std::atomic<bool>` stop flag;
  the Asio `io_context` is stopped on the next iteration, files are flushed and
  closed, metrics are printed, and the process exits cleanly.

## Metrics

Printed to stderr on shutdown:

```
messages_received     : 18432
parse_errors          : 0
reconnects            : 0
rows_written_market   : 18432
rows_written_orderbook: 12288
gaps_detected         : 0
books_initialized     : 1
```

## Run tests

```bash
cmake --build build --target tests
./build/tests/unit_tests
```

Or via CTest:
```bash
cd build && ctest --output-on-failure
```

## Verify CSV schema

```bash
# Market-data header
head -1 output/market_data_spot_BTCUSDT_*.csv

# Order-book column count (must be 26)
awk -F',' 'NR==2{print NF}' output/market_data_spot_BTCUSDT_*_orderbook.csv
# Expected: 26
```

## Security

- No API keys or credentials required (public streams only).
- TLS verification is **enabled** (default OpenSSL certificate store).
- No credentials or API keys are committed to this repository.

## How to Submit on GitHub

```bash
git init
git add .
git commit -m "Initial submission: Binance WebSocket capture + LOB"
git remote add origin https://github.com/<your-username>/binance-lob-capture.git
git branch -M main
git push -u origin main
git tag -a v1.0.0 -m "Submission v1.0.0"
git push origin v1.0.0
```