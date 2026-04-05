//
// Created by Eleazar Vega on 2/28/26.
//

#ifndef TICKARRAY_H
#define TICKARRAY_H

#include <cstdint>
#include <sys/mman.h>
#include <iostream>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif


template <typename T>
class TickArray {
public:
    TickArray() = default;

    struct Slot {
        T        price;
        uint32_t level_idx;
        bool     active;

        Slot() : price(0), level_idx(0), active(false) {}
    };

    Slot* ticks = nullptr;

    size_t _size = 0;

    int64_t minPrice = INT64_MAX;
    int64_t maxPrice = INT64_MIN;

    int64_t base_price;
    int64_t tick_size;
    size_t capacity;
    // Prices are now normalized to cents ($1.00 = 100)

    static constexpr size_t MAX_SLOTS = 5000;

    void initialize(int64_t low, int64_t high, int64_t tick) {
        tick_size = tick;
        base_price = low;
        capacity = (high - low) / tick;

        if (capacity == 0) {
            std::cerr << "TickArray: zero capacity! low=" << low << " high=" << high << " tick=" << tick << "\n";
            return;
        }

        ticks = static_cast<Slot*>(mmap(
            nullptr,
            capacity * sizeof(Slot),
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1, 0
        ));

        if (ticks == MAP_FAILED) {
            ticks = nullptr;
            return;
        }

        memset(ticks, 0xFF, capacity * sizeof(Slot));

        mlock(ticks, capacity * sizeof(Slot));
    }

    TickArray(const TickArray&) = delete;
    TickArray& operator=(const TickArray&) = delete;

    TickArray(TickArray&& other) noexcept
        : ticks(other.ticks), _size(other._size),
          minPrice(other.minPrice), maxPrice(other.maxPrice),
          base_price(other.base_price), tick_size(other.tick_size),
          capacity(other.capacity) {
        other.ticks = nullptr;
        other.capacity = 0;
    }

    TickArray& operator=(TickArray&& other) noexcept {
        if (this != &other) {
            if (ticks) munmap(ticks, capacity * sizeof(Slot));
            ticks = other.ticks;
            _size = other._size;
            minPrice = other.minPrice;
            maxPrice = other.maxPrice;
            base_price = other.base_price;
            tick_size = other.tick_size;
            capacity = other.capacity;
            other.ticks = nullptr;
            other.capacity = 0;
        }
        return *this;
    }

    ~TickArray() {
        if (ticks) munmap(ticks, capacity * sizeof(Slot));
    }

    bool empty() const {
        return _size == 0;
    }

    size_t size() const {
        return _size;
    }

    void incrementSize() { ++_size; }
    void decrementSize() { if (_size > 0) --_size; }

    void advanceMin() {
        if (!ticks) return;
        do {
            minPrice += tick_size;
            size_t idx = toIndex(minPrice);
            if (idx == SIZE_MAX) {
                minPrice = INT64_MAX;
                return;
            }
            if (ticks[idx].active) return;
        } while (true);
    }

    void advanceMax() {
        if (!ticks) return;
        do {
            maxPrice -= tick_size;
            size_t idx = toIndex(maxPrice);
            if (idx == SIZE_MAX) {
                maxPrice = INT64_MIN;
                return;
            }
            if (ticks[idx].active) return;
        } while (true);
    }

    size_t toIndex(int64_t price) {
        if (price < base_price) return SIZE_MAX;
        size_t idx = (price - base_price) / tick_size;
        if (idx >= capacity) return SIZE_MAX;
        return idx;
    }

    uint32_t find(const T& price) {
        if (!ticks) {
            return UINT32_MAX;
        }

        size_t idx = toIndex(price);
        if (idx == SIZE_MAX) {
            return UINT32_MAX;
        }

        if (price < minPrice) minPrice = price;
        if (price > maxPrice) maxPrice = price;

        return ticks[idx].active ? ticks[idx].level_idx : UINT32_MAX;
    }

    void activate(const T& price, uint32_t pool_idx) {
        size_t idx = toIndex(price);
        if (idx == SIZE_MAX) return;
        ticks[idx].level_idx = pool_idx;
        ticks[idx].active = true;
    }

    void deactivate(const T& price) {
        size_t idx = toIndex(price);
        if (idx == SIZE_MAX) return;
        ticks[idx].active = false;
    }

    uint32_t bestAsk() {
        size_t idx = toIndex(minPrice);
        if (idx == SIZE_MAX) return UINT32_MAX;
        return ticks[idx].level_idx;
    }

    uint32_t bestBid() {
        size_t idx = toIndex(maxPrice);
        if (idx == SIZE_MAX) return UINT32_MAX;
        return ticks[idx].level_idx;
    }

    int64_t getMin() {
        return minPrice;
    }

    int64_t getMax() {
        return maxPrice;
    }

};

#endif //TICKARRAY_H
