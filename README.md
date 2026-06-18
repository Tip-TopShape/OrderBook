# Order Book Reconstruction — NASDAQ ITCH

Reconstructs a full price-level order book from real NASDAQ MBO data from Databento. The goal was to handle all five action types (Add, Modify, Cancel, Trade, Fill) correctly and keep per-order latency in the sub-microsecond range throughout the 8 hour dataset.
NOTE: Match() is dead code. Kept just in case...

---

## How it works

**Price levels — TickArray**
No maps, each side of the book is a direct-mapped array indexed by price tick. Lookup O(1). Best bid/ask are tracked as running min/max so there's no scan. Ref prices is computed during a pre-pass. To avoid API usage cost, I pre-pass the same dataset instead of previous day. I acknowledge that this is not ideal and plan on improving this with more datasets/sources and handle the scenario where the tick size goes above/below the bounds.

**Memory — pool allocator**
Orders and price levels are pre-allocated with `mmap`+`mlock` into pools. `MAP_POPULATE` is used to avoid page faults mid-processing. The free-list is embedded directly in the pool storage, so no heap allocator in the hot path at all.

**Order queues — linked list via pool indices**
Each price level holds a doubly-linked FIFO queue of orders using 32-bit pool indices instead of pointers. Head fills first. Cancel is O(1) via an `order_id → pool index` hashmap. Using indices instead of pointers reduces node size and helps cache density.

**Instrument lookup**
`book_by_id[instrument_id]` — direct array index, no hash, no branch.

**Performance**
Latency is measured per-order with `rdtscp` (serialized reads) calibrated against `CLOCK_MONOTONIC_RAW` at startup. Bucketed into a power-of-2 histogram and reported as p50/p99/p99.9


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
