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
    /*
        Add	A	Insert a new order into the book.
        Modify	M	Change an order's price and/or size.
        Cancel	C	Fully or partially cancel an order from the book.
        Clear	R	Remove all resting orders for the instrument.
        Trade	T	An aggressing order traded. Does not affect the book.
        Fill	F	A resting order was filled. Does not affect the book.
        None	N	No action: does not affect the book, but may carry flags or other information.
     */
    auto newOrder = Order(ts_recv, ts_event, side, qty, price);
    newOrder.id = id;
    bool priceChange = false;
    auto t0 = now_ns();
    switch (action) {
        case databento::Action::Add:
            this->addOrder(newOrder, side, price, symbol);
            ++metrics_.adds;
            break;
        case databento::Action::Modify:
            priceChange = this->modifyOrder(id, price, qty, symbol);
            if (priceChange)
                this->addOrder(newOrder, side, price, symbol);
            ++metrics_.modifies;
            break;
        case databento::Action::Cancel:
            this->cancelOrder(id, symbol);
            ++metrics_.cancels;
            break;
        case databento::Action::Trade:
            this->trade(id, side, price, symbol);
            ++metrics_.trades;
            break;
        case databento::Action::Fill:
            this->trade(id, side, price, symbol);
            ++metrics_.fills;
            break;
        default:
            break;
    }
    auto t1 = now_ns();
    metrics_.recordLatency(t1 - t0);
    metrics_.recordOrder();
}

void FeedHandler::match(Order& entry, databento::Side &side, int64_t &price, std::basic_string<char> &symbol) {
    auto matchStart = now_ns();
    bool fulfilled = false;

    SymbolBook* book = findBook(symbol);
    if (!book) return;

    // BUY ORDER
    if (side == databento::Side::Bid) {
        while (!fulfilled && !book->asks.empty()) {
            auto level_price = book->asks.minPrice;
            if (level_price == INT64_MAX) break;

            auto lvl_idx = book->asks.bestAsk();
            if (lvl_idx == UINT32_MAX) break;
            auto &level = price_pool[lvl_idx];
            if (level_price <= price && !level.isEmpty()) {
                uint32_t matchIdx = level.top();
                Order& match = order_pool[matchIdx];

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
                        book->asks.deactivate(level_price);
                        price_pool.free(lvl_idx);
                        book->asks.decrementSize();
                        book->asks.advanceMin();
                    }
                }
            } else {
                break;
            }
        }
    }
    // SELL ORDER
    else if (side == databento::Side::Ask) {
        while (!fulfilled && !book->bids.empty()) {
            auto level_price = book->bids.maxPrice;
            if (level_price == INT64_MIN) break;

            auto lvl_idx = book->bids.bestBid();
            if (lvl_idx == UINT32_MAX) break;
            auto &level = price_pool[lvl_idx];
            if (level_price >= price && !level.isEmpty()) {
                uint32_t matchIdx = level.top();
                Order& match = order_pool[matchIdx];

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
                        book->bids.deactivate(level_price);
                        price_pool.free(lvl_idx);
                        book->bids.decrementSize();
                        book->bids.advanceMax();
                    }
                }
            } else {
                break;
            }
        }
    }

    uint64_t matchNs = now_ns() - matchStart;

    ++metrics_.ordersProcessed;
    metrics_.recordLatency(matchNs);
    metrics_.recordOrder();

    if (!fulfilled) {
        this->addOrder(entry, side, price, symbol);
    }
}

bool FeedHandler::modifyOrder(uint64_t order_id, int64_t new_price, uint32_t new_qty, std::basic_string<char> &symbol) {
    if (!id_to_index.contains(order_id)) return false;
    auto idx = id_to_index[order_id];
    auto &order = order_pool[idx];
    SymbolBook* book = findBook(symbol);
    if (!book) return false;
    auto &arr = (order.side == databento::Side::Bid) ? book->bids : book->asks;

    if (new_price == order.price) {
        order.qty = new_qty;
        return false;
    } else {
        uint32_t lvl_idx = arr.find(order.price);
        if (lvl_idx == UINT32_MAX) return false;
        priceLevel &level = price_pool[lvl_idx];
        popOrder(level, idx);
        if (level.isEmpty()) {
            arr.deactivate(order.price);
            price_pool.free(lvl_idx);
            arr.decrementSize();
        }
        // order.price = new_price;
        // order.qty = new_qty;
        // auto newLevel = book.find(new_price);
        return true;
    }
}

void FeedHandler::trade(uint64_t id, databento::Side &side, int64_t &price, std::basic_string<char> &symbol) {
    if (!id_to_index.contains(id)) return;
    auto idx = id_to_index[id];
    auto &order = order_pool[idx];
    SymbolBook* book = findBook(symbol);
    if (!book) return;
    auto &arr = (order.side == databento::Side::Bid) ? book->bids : book->asks;

    uint32_t lvl_idx = arr.find(order.price);
    if (lvl_idx == UINT32_MAX) return;
    priceLevel &level = price_pool[lvl_idx];
    order.qty = std::min(0u, (order.qty - order.qty));
    if (order.qty == 0) {
        popOrder(level, idx);
        if (level.isEmpty()) {
            arr.deactivate(order.price);
            price_pool.free(lvl_idx);
            arr.decrementSize();
        }
    }
}

void FeedHandler::addOrder(Order& entry, databento::Side &side, int64_t &price, std::basic_string<char> &symbol) {
    SymbolBook* book = findBook(symbol);
    if (!book) return;
    auto& arr = (side == databento::Side::Bid) ? book->bids : book->asks;
    if (!arr.ticks) return;

    uint32_t lvl_idx = arr.find(price);
    if (lvl_idx == UINT32_MAX) {
        lvl_idx = price_pool.allocate();
        if (lvl_idx == UINT32_MAX) return;
        price_pool[lvl_idx] = priceLevel{};
        arr.activate(price, lvl_idx);
        arr.incrementSize();
    }
    this->push(price_pool[lvl_idx], entry);
}

void FeedHandler::cancelOrder(const uint64_t &id, std::basic_string<char> &symbol) {
    if (!id_to_index.contains(id)) return;
    uint32_t idx = id_to_index[id];
    auto &order = order_pool[idx];

    SymbolBook* book = findBook(symbol);
    if (!book) return;
    auto &arr = (order.side == databento::Side::Bid) ? book->bids : book->asks;

    uint32_t lvl_idx = arr.find(order.price);
    if (lvl_idx == UINT32_MAX) return;
    priceLevel &level = price_pool[lvl_idx];

    popOrder(level, idx);
    id_to_index.erase(id);

    if (level.isEmpty()) {
        arr.deactivate(order.price);
        price_pool.free(lvl_idx);
        arr.decrementSize();
    }
}

size_t FeedHandler::calculateMemoryUsed() const {
    size_t used = sizeof(books) + (MAX_SIZE * sizeof(Order));
    for (const auto& book : books) {
        used += sizeof(SymbolBook);
        used += book.bids.capacity * sizeof(TickArray<int64_t>::Slot);
        used += book.asks.capacity * sizeof(TickArray<int64_t>::Slot);
    }
    return used;
}
