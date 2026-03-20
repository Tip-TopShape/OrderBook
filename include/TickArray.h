//
// Created by Eleazar Vega on 2/28/26.
//

#ifndef TICKARRAY_H
#define TICKARRAY_H

#include <cstdint>
#include <sys/mman.h>
#include <iostream>
#include <new>  // for placement new

// macOS compatibility
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

template <typename T, size_t CAPACITY>
struct Arena {
    T* storage;
    size_t top = 0;

    Arena() {
        storage = static_cast<T*>(mmap(
            nullptr,
            CAPACITY*sizeof(T),
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1, 0
        ));
        mlock(storage, CAPACITY * sizeof(T));
    }

    T* allocate(size_t n) {
        if (top + n > CAPACITY) return nullptr; // exhausted
        T* start = &storage[top];
        top += n;
        return start;
    }
};

template <typename T, typename CT>
class TickArray {
public:
    TickArray() = default;

    struct Slot {
        T price;
        CT level; // slot owns
        bool active = false;

        Slot() : price(0), level(), active(false) {}
    };

    Slot* ticks = nullptr;

    size_t _size = 0;

    int64_t minPrice = INT64_MAX;
    int64_t maxPrice = INT64_MIN;

    int64_t base_price;
    int64_t tick_size;
    size_t capacity;
    // Prices are now normalized to cents ($1.00 = 100)

    static constexpr size_t MAX_SLOTS = 5000; // reduced for M1 16GB

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
            std::cerr << "TickArray: mmap failed for " << capacity << " slots\n";
            ticks = nullptr;
            return;
        }

        for (size_t i = 0; i < capacity; ++i) {
            new (&ticks[i]) Slot();
        }

        // mlock can fail due to resource limits - ignore
        mlock(ticks, capacity * sizeof(Slot));
    }

    TickArray(const TickArray&) = delete;
    TickArray& operator=(const TickArray&) = delete;

    TickArray(TickArray&& other) noexcept
        : ticks(other.ticks), _size(other._size),
          minPrice(other.minPrice), maxPrice(other.maxPrice),
          base_price(other.base_price), tick_size(other.tick_size),
          capacity(other.capacity), out_of_bounds_level(std::move(other.out_of_bounds_level)) {
        other.ticks = nullptr;  // prevent double-free
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
            out_of_bounds_level = std::move(other.out_of_bounds_level);
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
            if (!ticks[idx].level.isEmpty()) return;
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
            if (!ticks[idx].level.isEmpty()) return;
        } while (true);
    }

    CT out_of_bounds_level;

    size_t toIndex(int64_t price) {
        if (price < base_price) return SIZE_MAX;
        size_t idx = (price - base_price) / tick_size;
        if (idx >= capacity) return SIZE_MAX;
        return idx;
    }

    CT& find(const T& price) {
        if (!ticks) {
            return out_of_bounds_level;
        }

        size_t idx = toIndex(price);
        if (idx == SIZE_MAX) {
            return out_of_bounds_level;
        }

        if (price < minPrice) minPrice = price;
        if (price > maxPrice) maxPrice = price;

        return ticks[idx].level;
    }

    CT& bestAsk() {
        size_t idx = toIndex(minPrice);
        if (idx == SIZE_MAX) return out_of_bounds_level;
        return ticks[idx].level;
    }

    CT& bestBid() {
        size_t idx = toIndex(maxPrice);
        if (idx == SIZE_MAX) return out_of_bounds_level;
        return ticks[idx].level;
    }

    int64_t getMin() {
        return minPrice;
    }

    int64_t getMax() {
        return maxPrice;
    }

};

#endif //TICKARRAY_H
