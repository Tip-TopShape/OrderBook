# Order Book Reconstruction — NASDAQ ITCH

Reconstructs a full price-level order book from real NASDAQ MBO data from Databento. The goal was to handle all five action types (Add, Modify, Cancel, Trade, Fill) correctly and keep per-order latency in the sub-microsecond range throughout the 8 hour dataset.
NOTE: Match() is dead code. Kept just in case...

---

## How it works

**Price levels — TickArray**
I choose to avoid maps, each side of the book is a direct-mapped array indexed by price tick. Lookup O(1). Best bid/ask are tracked as running min/max. Ref prices is computed during a pre-pass. To avoid API usage cost, I pre-pass the same dataset instead of previous day. I acknowledge that this is not ideal and plan on improving this with more datasets/sources and handle the scenario where the tick size goes above/below the bounds.

**Memory — pool allocator**
Orders and price levels are pre-allocated with `mmap`+`mlock` into pools. `MAP_POPULATE`. The free-list is embedded directly in the pool storage, so no heap allocator in the hot path at all.

**Order queues — linked list via pool indices**
Each price level holds a doubly-linked FIFO queue of orders using pool indices instead of pointers. Cancel is O(1) via an `order_id → pool index` hashmap (`unordered_dense`).

**Instrument lookup**
`book_by_id[instrument_id]` — direct array index. Symbol → instrument id is a flat array, no hash maps.

**Performance**
Latency is measured with `rdtscp` calibrated against `CLOCK_MONOTONIC_RAW` at startup, sampled every 16 orders to keep the latency check off the hot path. Tracked per action in csv, but the live console output shows an aggregated p50/p99/p99.9 across all actions.


## Build

C++20, CMake 3.14+. Databento SDK is pulled with CMake.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/OrderBook --file=path/to/feed.dbn.zst
./build/OrderBook --file=path/to/feed.dbn.zst --csv   # saves run to runs/
./build/OrderBook --file=path/to/feed.dbn.zst --10    # cap at 10M records
```

Or just use the script:
```bash
./benchmark.sh --csv --10
```


## Dependencies
- [databento-cpp](https://github.com/databento/databento-cpp) v0.51.0
- [unordered_dense](https://github.com/martinus/unordered_dense) v4.4.0
- [HdrHistogram_c](https://github.com/HdrHistogram/HdrHistogram_c) 0.9.9
