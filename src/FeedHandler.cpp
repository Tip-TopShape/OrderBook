//
// Created by Eleazar Vega on 12/14/25.
//

#include "../include/FeedHandler.h"
#include <algorithm>

void FeedHandler::processOrder(uint64_t &id,
                                databento::Side &side,
                                std::basic_string<char> &symbol,
                                databento::Action &action,
                                int64_t &price,
                                uint32_t &qty,
                                databento::UnixNanos &ts_recv,
                                databento::TimeDeltaNanos &ts_event)
{
    auto newOrder = Order(ts_recv, ts_event, side, qty);
    switch (action) {
        case databento::Action::Add:
            this->match(newOrder, side, price, symbol);
            break;
        case databento::Action::Cancel:
            this->cancelOrder(id);
            break;
        default:
            break;
    }
}

void FeedHandler::match(Order& entry, databento::Side &side, int64_t &price, std::basic_string<char> &symbol) {
    auto matchStart = std::chrono::high_resolution_clock::now();
    bool fulfilled = false;

    SymbolBook* book = findBook(symbol);
    if (!book) return;

    // BUY ORDER - match against asks
    if (side == databento::Side::Bid) {
        while (!fulfilled && !book->asks.empty()) {
            auto level_price = book->asks.minPrice;
            if (level_price == INT64_MAX) break;

            auto &level = book->asks.bestAsk();
            if (level_price <= price && !level.isEmpty()) {
                uint32_t matchIdx = level.top();
                Order& match = order_pool[matchIdx];

                if (canceled.find(match.id) != canceled.end()) {
                    this->processCancel(level);
                    continue;
                }

                if (entry.qty >= match.qty) {
                    entry.qty -= match.qty;
                    match.qty = 0;
                } else {
                    match.qty -= entry.qty;
                    entry.qty = 0;
                }

                if (entry.qty == 0) fulfilled = true;

                if (match.qty == 0) {
                    this->latestOrderFilled(level);
                    ++metrics_.fills;
                    if (level.isEmpty()) {
                        book->asks.decrementSize();
                        book->asks.advanceMin();
                    }
                }
            } else {
                break;
            }
        }
    }
    // SELL ORDER - match against bids
    else if (side == databento::Side::Ask) {
        while (!fulfilled && !book->bids.empty()) {
            auto level_price = book->bids.maxPrice;
            if (level_price == INT64_MIN) break;

            auto &level = book->bids.bestBid();
            if (level_price >= price && !level.isEmpty()) {
                uint32_t matchIdx = level.top();
                Order& match = order_pool[matchIdx];

                if (canceled.find(match.id) != canceled.end()) {
                    this->processCancel(level);
                    continue;
                }

                if (entry.qty >= match.qty) {
                    entry.qty -= match.qty;
                    match.qty = 0;
                } else {
                    match.qty -= entry.qty;
                    entry.qty = 0;
                }

                if (entry.qty == 0) fulfilled = true;

                if (match.qty == 0) {
                    this->latestOrderFilled(level);
                    ++metrics_.fills;
                    if (level.isEmpty()) {
                        book->bids.decrementSize();
                        book->bids.advanceMax();
                    }
                }
            } else {
                break;
            }
        }
    }

    auto matchEnd = std::chrono::high_resolution_clock::now();
    uint64_t matchNs = std::chrono::duration_cast<std::chrono::nanoseconds>(matchEnd - matchStart).count();

    ++metrics_.ordersProcessed;
    metrics_.recordLatency(matchNs);
    metrics_.recordOrder();

    if (!fulfilled) {
        this->addOrder(entry, side, price, symbol);
    }
}

void FeedHandler::addOrder(Order& entry, databento::Side &side, int64_t &price, std::basic_string<char> &symbol) {
    SymbolBook* book = findBook(symbol);
    if (!book) return;

    auto& arr = (side == databento::Side::Bid) ? book->bids : book->asks;
    if (!arr.ticks) return;

    auto& level = arr.find(price);
    bool wasEmpty = level.isEmpty();
    this->push(level, entry);

    if (wasEmpty) {
        arr.incrementSize();
    }
}

void FeedHandler::cancelOrder(const uint64_t &id) {
    canceled.insert(id);
}

size_t FeedHandler::calculateMemoryUsed() const {
    size_t used = sizeof(books) + (MAX_SIZE * sizeof(Order));
    for (const auto& book : books) {
        used += sizeof(SymbolBook);
        used += book.bids.capacity * sizeof(TickArray<int64_t, priceLevel>::Slot);
        used += book.asks.capacity * sizeof(TickArray<int64_t, priceLevel>::Slot);
    }
    return used;
}
