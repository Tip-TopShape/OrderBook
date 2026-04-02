//
// Created by Eleazar Vega on 12/31/25.
//

#ifndef PRICELEVEL_H
#define PRICELEVEL_H

#include <atomic>
#include <cstdint>

#include <databento/constants.hpp>
#include <databento/dbn.hpp>
#include <databento/symbology.hpp>
#include <databento/historical.hpp>

inline static std::atomic<int64_t> counter = 0;
constexpr uint32_t NULL_INDEX = UINT32_MAX;
constexpr static size_t MAX_SIZE = 2000000;
static constexpr uint32_t MAX_LEVELS = 1 << 20;

struct Order {
    Order(databento::UnixNanos &ts1, databento::TimeDeltaNanos &ts2, databento::Side &_side, int _qty, int64_t _price)
        :  side(_side), qty(_qty), price(_price), ts(ts1), ts_event(ts2) {

        id = counter;
        ++counter;
    };

    int64_t id;
    uint32_t qty;
    uint32_t idx;
    int64_t price;
    char side;
    uint32_t _next;
    uint32_t _prev;

    databento::UnixNanos ts;
    databento::UnixNanos ts_event;

    size_t memoryFootprint() const {
        return sizeof(*this);
    }
}__attribute__((aligned(64)));

class priceLevel {
private:


public:
    size_t _size = 0;
    int64_t price;
    uint32_t head;
    uint32_t tail;
    priceLevel() : head(NULL_INDEX), tail(NULL_INDEX) {}

    bool isEmpty() const {
        return _size == 0;
    }

    size_t size() {
        return _size;
    }

    uint32_t top() const {
        return head;
    }

    size_t memoryFootprint() const {
        size_t total = sizeof(*this);
        total += MAX_SIZE * sizeof(Order);
        return total;
    }
};
#endif //PRICELEVEL_H
