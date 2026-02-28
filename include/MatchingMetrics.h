//
// Created by Eleazar Vega on 01/10/26.
//

#ifndef MATCHINGMETRICS_H
#define MATCHINGMETRICS_H

#include <atomic>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

#include "CpuMetrics.h"

struct MatchingMetrics {

    // ─────────────────────────────────────────────────────────────────────────
    // Order Flow Counters
    // ─────────────────────────────────────────────────────────────────────────
    struct OrderFlow {
        std::atomic<uint64_t> totalMatchCalls{0};
        std::atomic<uint64_t> bidOrders{0};
        std::atomic<uint64_t> askOrders{0};
    } orderFlow;

    // ─────────────────────────────────────────────────────────────────────────
    // Fill Statistics
    // ─────────────────────────────────────────────────────────────────────────
    struct FillStats {
        std::atomic<uint64_t> fullFills{0};
        std::atomic<uint64_t> partialFills{0};
        std::atomic<uint64_t> noFills{0};
        std::atomic<uint64_t> ordersMatched{0};
        std::atomic<uint64_t> qtyMatched{0};
    } fills;

    // ─────────────────────────────────────────────────────────────────────────
    // Match Latency (nanoseconds)
    // ─────────────────────────────────────────────────────────────────────────
    struct MatchLatency {
        std::atomic<uint64_t> totalTimeNs{0};
        std::atomic<uint64_t> bidTimeNs{0};
        std::atomic<uint64_t> askTimeNs{0};
        std::atomic<uint64_t> minNs{UINT64_MAX};
        std::atomic<uint64_t> maxNs{0};
    } latency;

    // ─────────────────────────────────────────────────────────────────────────
    // Symbol Lookup Performance
    // ─────────────────────────────────────────────────────────────────────────
    struct SymbolLookup {
        std::atomic<uint64_t> calls{0};
        std::atomic<uint64_t> totalTimeNs{0};
    } symbolLookup;

    // ─────────────────────────────────────────────────────────────────────────
    // Book State (live counters - updated incrementally)
    // ─────────────────────────────────────────────────────────────────────────
    struct BookState {
        std::atomic<uint64_t> buyOrders{0};
        std::atomic<uint64_t> sellOrders{0};
        std::atomic<uint64_t> buyPriceLevels{0};
        std::atomic<uint64_t> sellPriceLevels{0};
    } book;

    // ─────────────────────────────────────────────────────────────────────────
    // Memory Usage (snapshot - updated on demand)
    // ─────────────────────────────────────────────────────────────────────────
    struct MemoryUsage {
        uint64_t buySymbols{0};
        uint64_t sellSymbols{0};
        uint64_t buyPriceLevels{0};
        uint64_t sellPriceLevels{0};
        uint64_t buyOrders{0};
        uint64_t sellOrders{0};
        uint64_t estimatedBytes{0};

        void reset() {
            buySymbols = 0;
            sellSymbols = 0;
            buyPriceLevels = 0;
            sellPriceLevels = 0;
            buyOrders = 0;
            sellOrders = 0;
            estimatedBytes = 0;
        }
    } memory;

    // ─────────────────────────────────────────────────────────────────────────
    // CPU Usage (cross-platform)
    // ─────────────────────────────────────────────────────────────────────────
    CpuMetrics cpu;

    // ─────────────────────────────────────────────────────────────────────────
    // Methods
    // ─────────────────────────────────────────────────────────────────────────

    void reset() {
        orderFlow.totalMatchCalls = 0;
        orderFlow.bidOrders = 0;
        orderFlow.askOrders = 0;

        fills.fullFills = 0;
        fills.partialFills = 0;
        fills.noFills = 0;
        fills.ordersMatched = 0;
        fills.qtyMatched = 0;

        latency.totalTimeNs = 0;
        latency.bidTimeNs = 0;
        latency.askTimeNs = 0;
        latency.minNs = UINT64_MAX;
        latency.maxNs = 0;

        symbolLookup.calls = 0;
        symbolLookup.totalTimeNs = 0;

        memory.reset();
        cpu.reset();
        cpu.takeSnapshot();  // Start fresh interval
    }

    double old = 0.0;
    void print(std::ostream &os) {
        // Update CPU metrics before printing
        cpu.calculateCpuPercentage();

        auto pct = [&](uint64_t val) {
            return orderFlow.totalMatchCalls > 0
                ? (100.0 * val / orderFlow.totalMatchCalls) : 0.0;
        };

        if (pct(fills.fullFills) > 2*old){
            os << "\n*** ALERT: Full fills percentage increased from "
               << std::fixed << std::setprecision(2) << old << "% to "
               << pct(fills.fullFills) << "% ***\n";
        } else {
            old = pct(fills.fullFills);
        }

        os << "\n============ Matching Engine Metrics ============\n";
        os << "Platform: " << CpuMetrics::getPlatformInfo() << "\n\n";

        os << "ORDER FLOW\n";
        os << "  Total match() calls:   " << orderFlow.totalMatchCalls << "\n";
        os << "  Bid orders:            " << orderFlow.bidOrders << "\n";
        os << "  Ask orders:            " << orderFlow.askOrders << "\n\n";

        os << "FILL STATISTICS\n";
        os << "  Full fills:            " << fills.fullFills
           << " (" << pct(fills.fullFills) << "%)\n";
        os << "  Partial fills:         " << fills.partialFills
           << " (" << pct(fills.partialFills) << "%)\n";
        os << "  No fills (added):      " << fills.noFills
           << " (" << pct(fills.noFills) << "%)\n";
        os << "  Orders matched:        " << fills.ordersMatched << "\n";
        os << "  Quantity matched:      " << fills.qtyMatched << "\n\n";

        os << "MATCH LATENCY (nanoseconds)\n";
        os << "  Total time:            " << latency.totalTimeNs << "\n";
        os << "  Avg per match:         "
           << (orderFlow.totalMatchCalls > 0 ? latency.totalTimeNs / orderFlow.totalMatchCalls : 0) << "\n";
        os << "  Min:                   "
           << (latency.minNs == UINT64_MAX ? 0 : latency.minNs.load()) << "\n";
        os << "  Max:                   " << latency.maxNs << "\n";
        os << "  Avg bid match:         "
           << (orderFlow.bidOrders > 0 ? latency.bidTimeNs / orderFlow.bidOrders : 0) << "\n";
        os << "  Avg ask match:         "
           << (orderFlow.askOrders > 0 ? latency.askTimeNs / orderFlow.askOrders : 0) << "\n\n";

        os << "SYMBOL LOOKUP\n";
        os << "  Lookup calls:          " << symbolLookup.calls << "\n";
        os << "  Total time:            " << symbolLookup.totalTimeNs << " ns\n";
        os << "  Avg per lookup:        "
           << (symbolLookup.calls > 0 ? symbolLookup.totalTimeNs / symbolLookup.calls : 0) << " ns\n\n";

        os << "MEMORY USAGE\n";
        os << "  Buy symbols:           " << memory.buySymbols << "\n";
        os << "  Sell symbols:          " << memory.sellSymbols << "\n";
        os << "  Buy price levels:      " << memory.buyPriceLevels << "\n";
        os << "  Sell price levels:     " << memory.sellPriceLevels << "\n";
        os << "  Buy orders:            " << memory.buyOrders << "\n";
        os << "  Sell orders:           " << memory.sellOrders << "\n";
        os << "  Total orders:          " << (memory.buyOrders + memory.sellOrders) << "\n";
        os << "  Estimated memory:      " << formatBytes(memory.estimatedBytes) << "\n\n";

        cpu.print(os);

        os << "\n=================================================\n";
    }

    static std::string formatBytes(uint64_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB"};
        int unitIdx = 0;
        double size = static_cast<double>(bytes);
        while (size >= 1024.0 && unitIdx < 3) {
            size /= 1024.0;
            ++unitIdx;
        }
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << size << " " << units[unitIdx];
        return oss.str();
    }

    // Helper to update min atomically
    void updateMin(uint64_t val) {
        uint64_t current = latency.minNs.load();
        while (val < current && !latency.minNs.compare_exchange_weak(current, val));
    }

    // Helper to update max atomically
    void updateMax(uint64_t val) {
        uint64_t current = latency.maxNs.load();
        while (val > current && !latency.maxNs.compare_exchange_weak(current, val));
    }
};

// RAII timer for scoped measurements
class ScopedTimer {
public:
    using Clock = std::chrono::high_resolution_clock;

    explicit ScopedTimer(std::atomic<uint64_t> &target)
        : target_(target), start_(Clock::now()) {}

    ~ScopedTimer() {
        auto end = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
        target_.fetch_add(duration);
    }

    uint64_t elapsed() const {
        auto now = Clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_).count();
    }

private:
    std::atomic<uint64_t> &target_;
    Clock::time_point start_;
};

#endif //MATCHINGMETRICS_H
