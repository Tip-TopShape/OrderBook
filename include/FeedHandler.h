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
// #include <ctime>
#include <atomic>
#include <iostream>

#include <databento/constants.hpp>
#include <databento/dbn.hpp>
#include <databento/symbology.hpp>
#include <databento/historical.hpp>

#include "../include/priceLevel.h"
#include "../include/MatchingMetrics.h"

#include <iostream>           // Basic I/O
#include <vector>             // Dynamic arrays (fine for non-hot-path)
#include <array>              // Fixed-size arrays
#include <cstdint>            // Fixed-width integers
#include <algorithm>          // std::sort, std::lower_bound

#include "RBTree.h"
// #include <memory>             // Smart pointers
//
// // CSV Parsing
// #include <rapidcsv.h>         // Or write simple custom parser
//
// // Testing
// #include <gtest/gtest.h>
//
// // Benchmarking
// #include <benchmark/benchmark.h>
//
// // Optional: If doing complex stats
// #include <Eigen/Dense>        // Linear algebra
// #include <boost/math/distributions.hpp>  // Statistical distributions

static std::unordered_map<uint32_t, std::string> id_to_symbol;

class FeedHandler {
private:

    // ts_recv,ts_event,rtype,publisher_id,instrument_id,action,side,
    // depth,price,size,flags,ts_in_delta,sequence,symb ol

    // a symbol's container
    using pricePoint = std::unique_ptr<priceLevel>;
    // using symbolPrices = std::vector<pricePoint> ;  // pre-allocated vector of price levels
    using symbolPrices = RBTree<int64_t, priceLevel>;

    // buy and sell side
    using listing = std::pair<std::basic_string<char>, symbolPrices>;
    using buyMarket = std::vector<listing>;  // symbol -> sorted prices (ascending for buys)
    using sellMarket = std::vector<listing>;  // symbol -> sorted prices (descending for sells)

    buyMarket buys; // best bid -> highest priced order
    sellMarket sells; // best ask -> lowest priced order
    std::unordered_set<uint64_t> canceled;

    MatchingMetrics metrics_;

    // Helper methods for vector-based market access
    symbolPrices* findSymbolInBuys(const std::basic_string<char> &symbol);
    symbolPrices* findSymbolInSells(const std::basic_string<char> &symbol);

    symbolPrices* getSymbolPriceLevel(const std::basic_string<char> &symbol, databento::Side side) {
        if (side == databento::Side::Ask) {
            auto sellsForSymbol = findSymbolInSells(symbol);
            return sellsForSymbol;
        }

        auto buysForSymbol = findSymbolInBuys(symbol);
        return buysForSymbol;
    }


    // order by id
    order HEAD;


    // std::map<int64_t, std::queue<order>> BUY_OVERFLOW; // best bid -> highest priced order
    // std::map<int64_t, level, std::queue<order>> SELL_OVERFLOW; // best ask -> lowest priced order

public:
    FeedHandler() {
        metrics_.cpu.takeSnapshot();  // Initialize CPU baseline
    };
    FeedHandler(const FeedHandler &fh){};
    ~FeedHandler(){};

    void increment() {
        ++counter;
    }
/*-----------------------Metrics------------------------------ BEGIN*/
    MatchingMetrics& getMetrics() { return metrics_; }
    const MatchingMetrics& getMetrics() const { return metrics_; }
    void updateMemoryMetrics();
    size_t calculateMemoryFootprint() const;
    void printMetrics(std::ostream &os = std::cout) {
        updateMemoryMetrics();
        metrics_.print(os);
    }
    void resetMetrics() { metrics_.reset(); }

    void printCurrentOrderBook(std::ostream &os) const;
    void reader(std::string filename);
    /*-----------------------Metrics------------------------------ END*/

    void processOrder(
        uint64_t &id,
        databento::Side &side,
        std::basic_string<char> &symbol,
        databento::Action &action,
        int64_t &price, uint32_t &qty,
        databento::UnixNanos &ts_recv,
        databento::TimeDeltaNanos &ts_event);


    void match(order &entry, databento::Side &side, int64_t &price, std::basic_string<char> &symbol);

    // rebuild methods
    void addOrder(order &entry, databento::Side &side, int64_t &price, std::basic_string<char> &symbol);
    void cancelOrder(const uint64_t &id);

    void prep(std::vector<std::basic_string<char>> &symbols) {
        for (std::basic_string<char> symbol : symbols) {
            auto treeOfPriceLevels = std::make_unique<symbolPrices>();
            buys.push_back({symbol, symbolPrices()});
            sells.push_back({symbol, symbolPrices()});
        }
    }
};


#endif //FEEDHANDLER_H
