//
// Created by Eleazar Vega on 12/14/25.
//

#include "../include/FeedHandler.h"
#include <algorithm>
#include <iomanip>

// Helper method to find or create a symbol entry in the buys market
FeedHandler::symbolPriceVector* FeedHandler::findSymbolInBuys(const std::basic_string<char> &symbol) {
    auto comp = [&symbol](const auto &pair) { return pair.first < symbol; };
    auto it = std::lower_bound(
        buys.begin(), buys.end(), symbol,
        [](const auto& pair, const auto& value) { return pair.first < value; }
    );
    if (it != buys.end()) {
        return &it->second;
    }

    // Create new symbol entry if not found
    buys.push_back({symbol, symbolPriceVector()});
    return &buys.back().second;
}

// Helper method to find or create a symbol entry in the sells market
FeedHandler::symbolPriceVector* FeedHandler::findSymbolInSells(const std::basic_string<char> &symbol) {
    auto it = std::lower_bound(
        sells.begin(), sells.end(), symbol,
        [](const auto& pair, const auto& value) { return pair.first < value; }
    );

    if (it != sells.end()) {
        return &it->second;
    }

    // Create new symbol entry if not found
    sells.push_back({symbol, symbolPriceVector()});
    return &sells.back().second;
}


void FeedHandler::printCurrentOrderBook(std::ostream &os) const  {

}



std::string stripWhitespace(std::string str) {
    str.erase(std::remove_if(str.begin(), str.end(), [](unsigned char c) { return std::isspace(c); }),
              str.end());
    return str;
}

void FeedHandler::reader(std::string filename) {
    std::ifstream stream(filename);

    std::string line;
    int count = 0;
    while (std::getline(stream, line)) {
        if (count == 100) {
            count = 0;
            printCurrentOrderBook(std::cout);
        } else ++count;
        // processOrder(line);

    }
}

void FeedHandler::processOrder(uint64_t &id, databento::Side &side, std::basic_string<char> &symbol, databento::Action &action, int64_t &price, uint32_t &qty, databento::UnixNanos &ts_recv, databento::TimeDeltaNanos &ts_event) {

    order newOrder = std::make_unique<Order>( Order(ts_recv, ts_event, side, qty) );
    switch (action) {
        case databento::Action::Add:
            this->match(newOrder, side, price, symbol);
            break;
        case databento::Action::Clear:
            break;
        case databento::Action::Modify:
            break;
        case databento::Action::Cancel:
            this->cancelOrder(id, side, price, symbol);
            break;
        default:
            break;
    }
    // this->match(newOrder, side, price, symbol);


}

void FeedHandler::match(order &entry, databento::Side &side, int64_t &price, std::basic_string<char> &symbol) {
    auto matchStart = std::chrono::high_resolution_clock::now();
    uint32_t initialQty = entry->qty;
    uint64_t matchedQty = 0;
    uint64_t ordersMatchedThisCall = 0;

    bool fufilled = false;

    // BUY ORDER
    if (side == databento::Side::Bid) {
        ++metrics_.orderFlow.bidOrders;

        auto lookupStart = std::chrono::high_resolution_clock::now();
        auto sellsForSymbol = findSymbolInSells(symbol);
        auto lookupEnd = std::chrono::high_resolution_clock::now();
        metrics_.symbolLookup.totalTimeNs += std::chrono::duration_cast<std::chrono::nanoseconds>(lookupEnd - lookupStart).count();
        ++metrics_.symbolLookup.calls;

        while (!fufilled && !sellsForSymbol->empty()) {
            // For sell side, prices are in descending order, so first element is lowest ask
            auto &pricePoint = sellsForSymbol->front();
            int64_t top_price = pricePoint.first;

            if (top_price <= price && !pricePoint.second.first->isEmpty()) {  // Match condition: ask price <= bid price
                auto match = pricePoint.second.first->top();
                if (nullptr == match) {
                    continue; //top ordered was canceled earlier
                }
                uint32_t fillQty = std::min(entry->qty, match->qty);
                matchedQty += fillQty;
                ++ordersMatchedThisCall;

                if (entry->qty >= match->qty) {
                    entry->qty = entry->qty - match->qty;
                    match->qty = 0;
                } else {
                    match->qty = match->qty - entry->qty;
                    entry->qty = 0;
                }

                // Check if new order is fulfilled
                if (entry->qty == 0) {
                    fufilled = true;
                    entry.reset();
                }

                // If matched order is filled
                if (match->qty == 0) {
                    pricePoint.second.first->latestOrderFilled();
                    if (pricePoint.second.first->isEmpty()) {
                        sellsForSymbol->erase(sellsForSymbol->begin());
                    }
                }
            } else {
                break;
            }
        }
    }
    // SELL ORDER
    else if (side == databento::Side::Ask) {
        ++metrics_.orderFlow.askOrders;

        auto lookupStart = std::chrono::high_resolution_clock::now();
        auto buysForSymbol = findSymbolInBuys(symbol);
        auto lookupEnd = std::chrono::high_resolution_clock::now();
        metrics_.symbolLookup.totalTimeNs += std::chrono::duration_cast<std::chrono::nanoseconds>(lookupEnd - lookupStart).count();
        ++metrics_.symbolLookup.calls;

        while (!fufilled && !buysForSymbol->empty()) {
            // For buy side, prices are in ascending order, so last element is highest bid
            auto &pricePoint = buysForSymbol->back();
            int64_t top_price = pricePoint.first;

            if (top_price >= price && !pricePoint.second.first->isEmpty()) {  // Match condition: bid price >= ask price
                auto match = pricePoint.second.first->top();
                if (nullptr == match) {
                    continue; //top ordered was canceled earlier
                }
                uint32_t fillQty = std::min(entry->qty, match->qty);
                matchedQty += fillQty;
                ++ordersMatchedThisCall;

                if (entry->qty >= match->qty) {
                    entry->qty = entry->qty - match->qty;
                    match->qty = 0;
                } else {
                    match->qty = match->qty - entry->qty;
                    entry->qty = 0;
                }

                // Check if new order is fulfilled
                if (entry->qty == 0) {
                    fufilled = true;
                    entry.reset();
                }

                // If matched order is filled
                if (match->qty == 0) {
                    pricePoint.second.first->latestOrderFilled();
                    if (pricePoint.second.first->isEmpty()) {
                        buysForSymbol->pop_back();
                    }
                }
            } else {
                break;
            }
        }
    }

    // Record metrics at end of match
    auto matchEnd = std::chrono::high_resolution_clock::now();
    uint64_t matchDurationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(matchEnd - matchStart).count();

    ++metrics_.orderFlow.totalMatchCalls;
    metrics_.latency.totalTimeNs += matchDurationNs;
    metrics_.updateMin(matchDurationNs);
    metrics_.updateMax(matchDurationNs);

    if (side == databento::Side::Bid) {
        metrics_.latency.bidTimeNs += matchDurationNs;
    } else {
        metrics_.latency.askTimeNs += matchDurationNs;
    }

    metrics_.fills.ordersMatched += ordersMatchedThisCall;
    metrics_.fills.qtyMatched += matchedQty;

    if (fufilled) {
        ++metrics_.fills.fullFills;
    } else if (matchedQty > 0) {
        ++metrics_.fills.partialFills;
    } else {
        ++metrics_.fills.noFills;
    }

    if (!fufilled) {
        this->addOrder(entry, side, price, symbol);
    }
}
void FeedHandler::addOrder(order &entry, databento::Side &side, int64_t &price, std::basic_string<char> &symbol) {
    if (side == databento::Side::Bid) {
        auto buysForSymbol = findSymbolInBuys(symbol);

        // Find the correct position to insert (maintain ascending order)
        auto it = std::lower_bound(buysForSymbol->begin(), buysForSymbol->end(), price,
            [](const auto &pair, int64_t p) { return pair.first < p; });

        if (it != buysForSymbol->end() && it->first == price) {
            // Price level exists
            if (it->second.second == priceLevel::STATUS::BELOW) {
                it->second.second = it->second.first->push(entry);
            } else {
                // use overflow buffer
            }
        } else {
            // Create new price level
            auto newLevel = std::make_unique<priceLevel>();
            auto newPoint = std::make_pair(price, std::make_pair(std::move(newLevel), priceLevel::STATUS::BELOW));
            it = buysForSymbol->insert(it, std::move(newPoint));
            // Push the order to the newly created level
            it->second.second = it->second.first->push(entry);
        }
    }
    else if (side == databento::Side::Ask) {
        auto sellsForSymbol = findSymbolInSells(symbol);

        // Find the correct position to insert (maintain descending order)
        auto it = std::lower_bound(sellsForSymbol->begin(), sellsForSymbol->end(), price,
            [](const auto &pair, int64_t p) { return pair.first > p; });

        if (it != sellsForSymbol->end() && it->first == price) {
            // Price level exists
            if (it->second.second == priceLevel::STATUS::BELOW) {
                it->second.second = it->second.first->push(entry);
            } else {
                // use overflow buffer
            }
        } else {
            // Create new price level
            auto newLevel = std::make_unique<priceLevel>();
            auto newPoint = std::make_pair(price, std::make_pair(std::move(newLevel), priceLevel::STATUS::BELOW));
            it = sellsForSymbol->insert(it, std::move(newPoint));
            // Push the order to the newly created level
            it->second.second = it->second.first->push(entry);
        }
    }
}

void FeedHandler::cancelOrder(const uint64_t &id, databento::Side &side, int64_t &price, std::basic_string<char> &symbol) {
    auto vec  = getSymbolPriceLevel(symbol, side); // get prices for symbol
    bool isAscending = side == databento::Side::Bid ? false : true; // greater or less comparator
    auto idx = getPriceLevelIdx(*vec, price, isAscending); // pull price level for given price
    if (idx != -1)
        (*vec)[idx].second.first->cancel(id); // mark it as cancel
}

void FeedHandler::updateMemoryMetrics() {
    auto& mem = metrics_.memory;
    mem.reset();

    // Count buy side
    mem.buySymbols = buys.size();
    for (const auto& [symbol, priceLevels] : buys) {
        mem.buyPriceLevels += priceLevels.size();
        for (const auto& [price, level] : priceLevels) {
            if (level.first) {
                mem.buyOrders += level.first->size();
            }
        }
    }

    // Count sell side
    mem.sellSymbols = sells.size();
    for (const auto& [symbol, priceLevels] : sells) {
        mem.sellPriceLevels += priceLevels.size();
        for (const auto& [price, level] : priceLevels) {
            if (level.first) {
                mem.sellOrders += level.first->size();
            }
        }
    }

    // Estimate memory usage using sizeof() for durability against refactoring
    mem.estimatedBytes = calculateMemoryFootprint();
}

size_t FeedHandler::calculateMemoryFootprint() const {
    size_t total = 0;

    // Base container overhead
    total += sizeof(buys) + sizeof(sells);

    // Buy side
    for (const auto& [symbol, priceLevels] : buys) {
        total += sizeof(symbol);  // string storage
        total += sizeof(priceLevels);                 // vector overhead
        total += priceLevels.capacity() * sizeof(pricePoint);  // vector storage

        for (const auto& [price, level] : priceLevels) {
            if (level.first) {
                total += level.first->memoryFootprint();
            }
        }
    }

    // Sell side
    for (const auto& [symbol, priceLevels] : sells) {
        total += sizeof(symbol);
        total += sizeof(priceLevels);
        total += priceLevels.capacity() * sizeof(pricePoint);

        for (const auto& [price, level] : priceLevels) {
            if (level.first) {
                total += level.first->memoryFootprint();
            }
        }
    }

    return total;
}
