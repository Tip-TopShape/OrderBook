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
#ifndef MAP_POPULATE
#define MAP_POPULATE 0
#endif

#include "../include/priceLevel.h"
#include "../include/MatchingMetrics.h"
#include "TickArray.h"


struct SymbolBook {
    std::basic_string<char> symbol;
    TickArray<int64_t> bids;
    TickArray<int64_t> asks;

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
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE,
            -1, 0
        ));

        if (storage == MAP_FAILED) {
            storage = nullptr;
            return;
        }

        mlock(storage, CAPACITY * sizeof(T));
    }

    ~Pool() {
        if (storage && storage != MAP_FAILED) {
            munmap(storage, CAPACITY * sizeof(T));
        }
    }

    uint32_t allocate() {
        if (head == UINT32_MAX) return UINT32_MAX;
        uint32_t idx = head;
        head = *reinterpret_cast<uint32_t*>(&storage[idx]);
        return idx;
    }

    void free(uint32_t idx) {
        *reinterpret_cast<uint32_t*>(&storage[idx]) = head;
        head = idx;
    }

    T& operator[](uint32_t idx) {
        return storage[idx];
    }

}; // end of pool

class FeedHandler {
private:
    static constexpr uint32_t MAX_INSTRUMENT_ID = 100000;
    std::vector<SymbolBook> books;
    std::unordered_map<uint32_t, std::string> id_to_symbol_;
    std::unordered_map<uint64_t, uint32_t> id_to_index;
    Pool<Order, MAX_SIZE> order_pool;
    Pool<priceLevel, MAX_LEVELS> price_pool;
    SymbolBook* book_by_id[MAX_INSTRUMENT_ID] = {};

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

    SymbolBook* findBook(uint32_t instrument_id) {
        if (instrument_id >= MAX_INSTRUMENT_ID) return nullptr;
        return book_by_id[instrument_id];
    }

public:
    FeedHandler() = default;
    FeedHandler(const FeedHandler&) = delete;
    FeedHandler& operator=(const FeedHandler&) = delete;
    ~FeedHandler() = default;

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
        uint32_t instrument_id,
        databento::Action &action,
        int64_t &price, uint32_t &qty,
        databento::UnixNanos &ts_recv,
        databento::TimeDeltaNanos &ts_event);


    void match(Order& entry, databento::Side &side, int64_t &price, uint32_t instrument_id);
    bool modifyOrder(uint64_t order_id, int64_t new_price, uint32_t new_qty, uint32_t instrument_id);

    void trade(uint64_t id, databento::Side &side, int64_t &price, uint32_t instrument_id);
    void addOrder(Order& entry, databento::Side &side, int64_t &price, uint32_t instrument_id);
    void cancelOrder(const uint64_t &id, uint32_t instrument_id);

    void registerSymbol(uint32_t id, const std::string& symbol) {
        id_to_symbol_[id] = symbol;
    }

    void prep(const std::basic_string<char>& symbol,
              int64_t low,
              int64_t high,
              int64_t tick_size) {
        SymbolBook book;
        book.bids.initialize(low, high, tick_size);
        book.asks.initialize(low, high, tick_size);
        book.symbol = symbol;
        books.emplace_back(std::move(book));
    }

    void reserveBooks(size_t count) {
        books.reserve(count);
    }

    void finalizePrep() {
        std::sort(books.begin(), books.end());
        for (auto& [id, sym] : id_to_symbol_) {
            if (id < MAX_INSTRUMENT_ID)
                book_by_id[id] = findBook(sym);
        }
    }

    /* POOL MANAGEMENT */
    void latestOrderFilled(priceLevel& level) {
        if (level.isEmpty()) return;

        uint32_t headIdx = level.head;
        Order& toRemove = order_pool[headIdx];
        level.head = toRemove._next;

        if (level.head == NULL_INDEX) {
            // if (level.tail == NULL_INDEX && level.head != NULL_INDEX) {// 6095691568
            //     std::cout << "here";
            // }
            level.tail = NULL_INDEX;
        } else {
            order_pool[level.head]._prev = NULL_INDEX;
        }

        id_to_index.erase(toRemove.id);
        order_pool.free(headIdx);
        --level._size;
    }

    void popOrder(priceLevel& level, uint32_t idx) {
        if (idx == level.head) {
            latestOrderFilled(level);
            return;
        }

        Order& order = order_pool[idx];

        if (order._prev != NULL_INDEX)
            order_pool[order._prev]._next = order._next;

        if (order._next != NULL_INDEX)
            order_pool[order._next]._prev = order._prev;
        else
            level.tail = order._prev;

        id_to_index.erase(order.id);
        order_pool.free(idx);
        --level._size;
    }

    uint32_t push(priceLevel& level, Order& entry) {
        uint32_t idx = order_pool.allocate();
        if (idx == NULL_INDEX) {
            return NULL_INDEX; // pool exhausted
        }

        order_pool[idx] = entry;
        order_pool[idx].idx = idx;
        order_pool[idx]._next = NULL_INDEX;
        id_to_index[entry.id] = idx;
        order_pool[idx]._prev = level.isEmpty() ? NULL_INDEX : level.tail;

        if (level.isEmpty()) {
            level.head = idx;
            //     if (level.tail == NULL_INDEX && level.head != NULL_INDEX) {// 6095691568
            //     std::cout << "here";
            // }
            level.tail = idx;
        } else {
            // if (level.tail == NULL_INDEX && level.head != NULL_INDEX) {// 6095691568
            //     std::cout << "here";
            // }
            order_pool[level.tail]._next = idx;
            level.tail = idx;
        }

        ++level._size;
        return idx;
    }
};



#endif //FEEDHANDLER_H
