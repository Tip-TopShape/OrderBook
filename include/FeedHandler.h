//
// Created by Eleazar Vega on 12/14/25.
//

#ifndef FEEDHANDLER_H
#define FEEDHANDLER_H


#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <atomic>
#include <iostream>
#include <vector>
#include <array>
#include <cstdint>
#include <algorithm>
#include <utility>
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#include "../include/priceLevel.h"
#include "../include/MatchingMetrics.h"
#include "TickArray.h"


struct SymbolBook {
    std::basic_string<char> symbol;
    TickArray<int64_t, priceLevel> bids;
    TickArray<int64_t, priceLevel> asks;

    bool operator<(const SymbolBook& other) const {
        return symbol < other.symbol;
    }
};

template <typename T, size_t CAPACITY>
struct Pool {
    T*   storage; // pool of orders
    uint32_t freelist[CAPACITY];
    uint32_t freelist_top;

    Pool() : freelist_top(CAPACITY) {
        for (uint32_t i = 0; i < CAPACITY; ++i) {
            freelist[i] = i;
        }

        storage = static_cast<T*>(mmap(
            nullptr,
            CAPACITY * sizeof(T),
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1, 0
        ));

        if (storage == MAP_FAILED) {
            storage = nullptr;
        }

        mlock(storage, CAPACITY * sizeof(T));
    }

    ~Pool() {
        if (storage && storage != MAP_FAILED) {
            munmap(storage, CAPACITY * sizeof(T));
        }
    }

    uint32_t allocate() {
        if (freelist_top == 0) {
            return UINT32_MAX; // Pool exhausted
        }
        return freelist[--freelist_top];
    }

    void free(uint32_t idx) {
        if (freelist_top < CAPACITY) {
            freelist[freelist_top++] = idx;
        }
    }

    T& operator[](uint32_t idx) {
        return storage[idx];
    }

}; // end of pool

class FeedHandler {
private:
    std::vector<SymbolBook> books;
    std::unordered_set<uint64_t> canceled;
    std::unordered_map<uint32_t, std::string> id_to_symbol_;  // instrument_id -> symbol
    Pool<Order, MAX_SIZE> order_pool;

    MatchingMetrics metrics_;

    SymbolBook* findBook(const std::basic_string<char>& symbol) {
        SymbolBook target;
        target.symbol = symbol;
        auto it = std::lower_bound(books.begin(), books.end(), target);
        if (it != books.end() && it->symbol == symbol) {
            return &(*it);
        }
        return nullptr;
    }

public:
    FeedHandler() = default;
    FeedHandler(const FeedHandler &fh){};
    ~FeedHandler(){};

    MatchingMetrics& getMetrics() { return metrics_; }
    size_t calculateMemoryUsed() const;

    void printMetrics(std::ostream &os, uint64_t recordCount) {
        metrics_.memoryUsedBytes = calculateMemoryUsed();
        metrics_.printLine(os, recordCount);
    }

    void printMetricsCSV(std::ostream &os, uint64_t recordCount) {
        metrics_.memoryUsedBytes = calculateMemoryUsed();
        metrics_.printCSVLine(os, recordCount);
    }

    void resetMetrics() { metrics_.reset(); }

    void processOrder(
        uint64_t &id,
        databento::Side &side,
        std::basic_string<char> &symbol,
        databento::Action &action,
        int64_t &price, uint32_t &qty,
        databento::UnixNanos &ts_recv,
        databento::TimeDeltaNanos &ts_event);


    void match(Order& entry, databento::Side &side, int64_t &price, std::basic_string<char> &symbol);

    void  addOrder(Order& entry, databento::Side &side, int64_t &price, std::basic_string<char> &symbol);
    void cancelOrder(const uint64_t &id);

    void registerSymbol(uint32_t id, const std::string& symbol) {
        id_to_symbol_[id] = symbol;
    }

    const std::string& getSymbol(uint32_t id) const {
        return id_to_symbol_.at(id);
    }

    void prep(const std::basic_string<char>& symbol,
              int64_t low,
              int64_t high,
              int64_t tick_size) {
        SymbolBook book;
        book.bids.initialize(low, high, tick_size);
        book.asks.initialize(low, high, tick_size);
        book.symbol = symbol;
        books.push_back(std::move(book));
    }

    void reserveBooks(size_t count) {
        books.reserve(count);
    }

    void finalizePrep() {
        std::sort(books.begin(), books.end());
    }

    /* POOL MANAGEMENT */
    void latestOrderFilled(priceLevel& level) {
        if (level.isEmpty()) return;

        uint32_t headIdx = level.head;
        Order& toRemove = order_pool[headIdx];
        level.head = toRemove._next;

        if (level.head == NULL_INDEX) {
            level.tail = NULL_INDEX;
        }

        order_pool.free(headIdx);
        --level._size;
    }

    void processCancel(priceLevel& level) {
        // same as latestOrderFilled, created for clarity
        latestOrderFilled(level);
    }

    uint32_t push(priceLevel& level, Order& entry) {
        uint32_t idx = order_pool.allocate();
        if (idx == NULL_INDEX) {
            return NULL_INDEX; // pool exhausted
        }

        order_pool[idx] = entry;
        order_pool[idx].idx = idx;
        order_pool[idx]._next = NULL_INDEX;

        if (level.isEmpty()) {
            level.head = idx;
            level.tail = idx;
        } else {
            order_pool[level.tail]._next = idx;
            level.tail = idx;
        }

        ++level._size;
        return idx;
    }
};



#endif //FEEDHANDLER_H
