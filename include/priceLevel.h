//
// Created by Eleazar Vega on 12/31/25.
//

// #define "memory_order.h"

#ifndef PRICELEVEL_H
#define PRICELEVEL_H

inline static std::atomic_int counter = 0;

#include <atomic>

struct Order {
    Order();

    Order(databento::UnixNanos &ts1, databento::TimeDeltaNanos &ts2, databento::Side &_side, int _qty)
        :  side(_side), qty(_qty), ts(ts1), ts_event(ts2) {

        id = counter;
        ++counter;
    };

    int64_t id;
    uint32_t qty;
    char side;
    std::unique_ptr<Order> _next;

    // std::basic_string<char> symbol;
    databento::UnixNanos ts;
    databento::UnixNanos ts_event;

    size_t memoryFootprint() const {
        return sizeof(*this);
    }
}__attribute__((aligned(64)));

using order = std::unique_ptr<Order>;
constexpr static int MAX_SIZE = 100;
class priceLevel {
private:
    std::vector<order> levels = std::vector<order>(MAX_SIZE);
    std::atomic<int> start = 0; // start of the buffer
    std::atomic<int> next = 0;
    size_t _size = 0;

public:
    priceLevel() {}
    bool isEmpty() {
        if (_size == 0) {
            return true;
        }


        return false;
    }

    void incrementStart() {
        // // Single producer, single consumer - NO CAS needed!
        // write_index = atomic_load_relaxed(&producer_index);
        // if (write_index - atomic_load_acquire(&consumer_index) < BUFFER_SIZE) {
        //     buffer[write_index % BUFFER_SIZE] = data;
        //     atomic_store_release(&producer_index, write_index + 1);
        // }
        // auto i = start.load(std::memory_order::relaxed);
        // if (i - std::atomic_load)
    }

    void latestOrderFilled() {
        levels[start].reset();
        postReset();
    }

    void processCancel() {
        // same as latestOrderFilled, created for clarity
        latestOrderFilled();
    }

    size_t size() {
        return _size;
    }

    void push(order &entry) {
        if (this->size() == MAX_SIZE) {
            return;
        }

        levels[next] = std::move(entry);
        ++next;
        next = next % MAX_SIZE; // loop back using modulo
        ++_size;
    }

    Order* top() {
        return levels[start].get();
    }

    void postReset() {
        --_size; // removed an order
        ++start;
        start = start % MAX_SIZE; // loop back using modulo
    }

    size_t memoryFootprint() const {
        size_t total = sizeof(*this);
        total += levels.capacity() * sizeof(order);  // vector storage

        // Add memory for each actual Order object
        for (const auto& ord : levels) {
            if (ord) {
                total += ord->memoryFootprint();
            }
        }
        return total;
    }
};
#endif //PRICELEVEL_H
