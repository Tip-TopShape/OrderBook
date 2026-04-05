#ifndef MATCHINGMETRICS_H
#define MATCHINGMETRICS_H

#include <cstdint>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <time.h>

static inline uint64_t now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

struct MatchingMetrics {

    struct LatencyHistogram {
        static constexpr size_t BUCKETS = 64;
        uint64_t counts[BUCKETS]{};
        uint64_t total{0};

        void record(uint64_t ns) {
            size_t b = (ns < 2) ? 0 : (63 - __builtin_clzll(ns));
            ++counts[b];
            ++total;
        }

        void reset() {
            memset(counts, 0, sizeof(counts));
            total = p50 = p99 = p999 = 0;
        }

        uint64_t p50{0};
        uint64_t p99{0};
        uint64_t p999{0};
        void percentile() {
            if (total == 0) return;
            const uint64_t t50  = static_cast<uint64_t>(0.500 * total);
            const uint64_t t99  = static_cast<uint64_t>(0.990 * total);
            const uint64_t t999 = static_cast<uint64_t>(0.999 * total);
            uint64_t cmltv = 0;
            for (size_t i = 0; i < BUCKETS; ++i) {
                cmltv += counts[i];
                if (!p50  && cmltv > t50)  p50  = (1ULL << i);
                if (!p99  && cmltv > t99)  p99  = (1ULL << i);
                if (!p999 && cmltv > t999) p999 = (1ULL << i);
            }
        }
    };

    uint64_t ordersProcessed{0};
    uint64_t adds{0};
    uint64_t modifies{0};
    uint64_t cancels{0};
    uint64_t trades{0};
    uint64_t fills{0};
    LatencyHistogram latencyHist;

    uint64_t windowStart{0};
    uint64_t ordersInWindow{0};
    double throughputPerSec{0.0};

    uint64_t memoryUsedBytes{0};

    void startWindow() {
        windowStart = now_ns();
        ordersInWindow = 0;
    }

    void recordOrder() { ++ordersInWindow; }
    void recordLatency(uint64_t ns) { latencyHist.record(ns); }

    void endWindow() {
        uint64_t durationNs = now_ns() - windowStart;
        if (durationNs > 0) {
            throughputPerSec = ordersInWindow * 1e9 / durationNs;
        }
    }

    void reset() {
        ordersProcessed = 0;
        adds = 0;
        modifies = 0;
        cancels = 0;
        trades = 0;
        fills = 0;
        latencyHist.reset();
        startWindow();
    }

    void printLine(std::ostream &os, uint64_t recordCount) {
        endWindow();

        auto fmtCount = [](uint64_t n) -> std::string {
            if (n >= 1000000) return std::to_string(n / 1000000) + "M";
            if (n >= 1000) return std::to_string(n / 1000) + "K";
            return std::to_string(n);
        };

        auto fmtThroughput = [](double t) -> std::string {
            std::ostringstream ss;
            if (t >= 1e6) ss << std::fixed << std::setprecision(2) << (t / 1e6) << "M/s";
            else if (t >= 1e3) ss << std::fixed << std::setprecision(0) << (t / 1e3) << "K/s";
            else ss << std::fixed << std::setprecision(0) << t << "/s";
            return ss.str();
        };

        auto fmtLatency = [](uint64_t ns) -> std::string {
            std::ostringstream ss;
            if (ns >= 1000000) ss << std::fixed << std::setprecision(1) << (ns / 1e6) << "ms";
            else if (ns >= 1000) ss << std::fixed << std::setprecision(1) << (ns / 1e3) << "us";
            else ss << ns << "ns";
            return ss.str();
        };

        auto fmtMem = [](uint64_t b) -> std::string {
            std::ostringstream ss;
            if (b >= 1024*1024*1024) ss << std::fixed << std::setprecision(1) << (b / (1024.0*1024*1024)) << "GB";
            else if (b >= 1024*1024) ss << std::fixed << std::setprecision(0) << (b / (1024.0*1024)) << "MB";
            else if (b >= 1024) ss << std::fixed << std::setprecision(0) << (b / 1024.0) << "KB";
            else ss << b << "B";
            return ss.str();
        };

        latencyHist.percentile();
        os << "[" << std::setw(6) << fmtCount(recordCount) << "] "
           << std::setw(8) << fmtThroughput(throughputPerSec) << " | "
           << "p50=" << std::setw(6) << fmtLatency(latencyHist.p50) << " "
           << "p99=" << std::setw(6) << fmtLatency(latencyHist.p99) << " "
           << "p99.9=" << std::setw(6) << fmtLatency(latencyHist.p999) << " | "
           << std::setw(6) << fmtMem(memoryUsedBytes) << "\n";
    }

    static void printCSVHeader(std::ostream &os) {
        os << "records,throughput,p50_ns,p99_ns,p999_ns,memory_bytes,adds,modifies,cancels,trades,fills\n";
    }

    void printCSVLine(std::ostream &os, uint64_t recordCount) {
        endWindow();
        os << recordCount << ","
           << std::fixed << std::setprecision(0) << throughputPerSec << ","
           << latencyHist.p50 << ","
           << latencyHist.p99 << ","
           << latencyHist.p999 << ","
           << memoryUsedBytes << ","
           << adds << ","
           << modifies << ","
           << cancels << ","
           << trades << ","
           << fills << "\n";
    }
};

#endif
