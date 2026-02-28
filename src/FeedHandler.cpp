//
// Created by Eleazar Vega on 12/14/25.
//

#include "../include/FeedHandler.h"
#include <algorithm>
#include <iomanip>

// Helper method to find or create a symbol entry in the buys market
FeedHandler::symbolPrices* FeedHandler::findSymbolInBuys(const std::basic_string<char> &symbol) {
    auto comp = [&symbol](const auto &pair) { return pair.first < symbol; };
    auto it = std::lower_bound(
        buys.begin(), buys.end(), symbol,
        [](const auto& pair, const auto& value) { return pair.first < value; }
    );
    if (it != buys.end()) {
        return &it->second;
    }

    // Create new symbol entry if not found
    buys.push_back({symbol, symbolPrices()});
    return &buys.back().second;
}

// Helper method to find or create a symbol entry in the sells market
FeedHandler::symbolPrices* FeedHandler::findSymbolInSells(const std::basic_string<char> &symbol) {
    auto it = std::lower_bound(
        sells.begin(), sells.end(), symbol,
        [](const auto& pair, const auto& value) { return pair.first < value; }
    );

    if (it != sells.end()) {
        return &it->second;
    }

    // Create new symbol entry if not found
    sells.push_back({symbol, symbolPrices()});
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

void FeedHandler::processOrder(uint64_t &id,
                                databento::Side &side,
                                std::basic_string<char> &symbol,
                                databento::Action &action,
                                int64_t &price,
                                uint32_t &qty,
                                databento::UnixNanos &ts_recv,
                                databento::TimeDeltaNanos &ts_event)
{

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
            this->cancelOrder(id);
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
            auto pricePoint = sellsForSymbol->getMin();
            // I now have pricePoint, the head of the tree at this price
            if (pricePoint->price <= price && !pricePoint->level->isEmpty()) {  // Match condition: ask price <= bid price
                auto match = pricePoint->level->top();
                if (canceled.find(match->id) != canceled.end()) {
                    pricePoint->level->processCancel();
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
                    pricePoint->level->latestOrderFilled();
                    --metrics_.book.sellOrders;
                    if (pricePoint->level->isEmpty()) {
                        sellsForSymbol->erase(pricePoint);
                        --metrics_.book.sellPriceLevels;
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
            auto pricePoint = buysForSymbol->getMax();

            if (pricePoint->price >= price && !pricePoint->level->isEmpty()) {  // Match condition: bid price >= ask price
                auto match = pricePoint->level->top();
                if (canceled.find(match->id) != canceled.end()) {
                    pricePoint->level->processCancel();
                    continue; //top ordered was canceled earlier
                }
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
                    pricePoint->level->latestOrderFilled();
                    --metrics_.book.buyOrders;
                    if (pricePoint->level->isEmpty()) {
                        buysForSymbol->erase(pricePoint);
                        --metrics_.book.buyPriceLevels;
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
        auto tree = findSymbolInBuys(symbol);
        size_t sizeBefore = tree->size();
        auto pricePoint = tree->find(price, tree->headIndex, nullptr);
        pricePoint->level->push(entry);
        if (tree->size() > sizeBefore) {
            ++metrics_.book.buyPriceLevels;
        }
        ++metrics_.book.buyOrders;
    } else {
        auto tree = findSymbolInSells(symbol);
        size_t sizeBefore = tree->size();
        auto pricePoint = tree->find(price, tree->headIndex, nullptr);
        pricePoint->level->push(entry);
        if (tree->size() > sizeBefore) {
            ++metrics_.book.sellPriceLevels;
        }
        ++metrics_.book.sellOrders;
    }
}

void FeedHandler::cancelOrder(const uint64_t &id) {
    canceled.insert(id);
}

void FeedHandler::updateMemoryMetrics() {
    auto& mem = metrics_.memory;

    // Use live counters - O(1)
    mem.buySymbols = buys.size();
    mem.sellSymbols = sells.size();
    mem.buyPriceLevels = metrics_.book.buyPriceLevels.load();
    mem.sellPriceLevels = metrics_.book.sellPriceLevels.load();
    mem.buyOrders = metrics_.book.buyOrders.load();
    mem.sellOrders = metrics_.book.sellOrders.load();

    // Estimate memory based on counts
    mem.estimatedBytes = calculateMemoryFootprint();
}

size_t FeedHandler::calculateMemoryFootprint() const {
    size_t total = 0;

    // Base container overhead
    total += sizeof(buys) + sizeof(sells);

    // Per-symbol overhead (string + RBTree base)
    size_t perSymbol = sizeof(std::string) + sizeof(symbolPrices);
    total += (buys.size() + sells.size()) * perSymbol;

    // Per-price-level overhead (RBTree node + priceLevel)
    size_t perPriceLevel = sizeof(symbolPrices::Node) + sizeof(priceLevel);
    total += (metrics_.book.buyPriceLevels.load() + metrics_.book.sellPriceLevels.load()) * perPriceLevel;

    // Per-order overhead
    size_t perOrder = sizeof(Order);
    total += (metrics_.book.buyOrders.load() + metrics_.book.sellOrders.load()) * perOrder;

    return total;
}
