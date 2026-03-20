#ifndef MATCHINGMETRICS_H
#define MATCHINGMETRICS_H

#include <atomic>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <vector>

struct MatchingMetrics {
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    // Reservoir sampling for latency percentiles
    struct LatencyHistogram {
        static constexpr size_t RESERVOIR_SIZE = 10000;
        std::vector<uint64_t> samples;
        size_t count = 0;

        LatencyHistogram() { samples.reserve(RESERVOIR_SIZE); }

        void record(uint64_t ns) {
            if (samples.size() < RESERVOIR_SIZE) {
                samples.push_back(ns);
            } else {
                samples[count % RESERVOIR_SIZE] = ns;
            }
            ++count;
        }

        void reset() {
            samples.clear();
            count = 0;
        }

        uint64_t percentile(double p) const {
            if (samples.empty()) return 0;
            std::vector<uint64_t> sorted = samples;
            std::sort(sorted.begin(), sorted.end());
            return sorted[static_cast<size_t>(p * (sorted.size() - 1))];
        }

        uint64_t p50() const { return percentile(0.50); }
        uint64_t p99() const { return percentile(0.99); }
        uint64_t p999() const { return percentile(0.999); }
    };

    std::atomic<uint64_t> ordersProcessed{0};
    std::atomic<uint64_t> fills{0};
    LatencyHistogram latencyHist;

    TimePoint windowStart;
    uint64_t ordersInWindow{0};
    double throughputPerSec{0.0};

    uint64_t memoryUsedBytes{0};

    void startWindow() {
        windowStart = Clock::now();
        ordersInWindow = 0;
    }

    void recordOrder() { ++ordersInWindow; }
    void recordLatency(uint64_t ns) { latencyHist.record(ns); }

    void endWindow() {
        auto durationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - windowStart).count();
        if (durationNs > 0) {
            throughputPerSec = ordersInWindow * 1e9 / durationNs;
        }
    }

    void reset() {
        ordersProcessed = 0;
        fills = 0;
        latencyHist.reset();
        startWindow();
    }

    void printLine(std::ostream &os, uint64_t recordCount) {
        endWindow();

        auto fmtCount = [](uint64_t n) -> std::string {
            if (n >= 1000000) return std::to_string(n / 1000) + "K";
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

        os << "[" << std::setw(6) << fmtCount(recordCount) << "] "
           << std::setw(8) << fmtThroughput(throughputPerSec) << " | "
           << "p50=" << std::setw(6) << fmtLatency(latencyHist.p50()) << " "
           << "p99=" << std::setw(6) << fmtLatency(latencyHist.p99()) << " "
           << "p99.9=" << std::setw(6) << fmtLatency(latencyHist.p999()) << " | "
           << std::setw(6) << fmtMem(memoryUsedBytes) << "\n";
    }

    // CSV header for benchmark output
    static void printCSVHeader(std::ostream &os) {
        os << "records,throughput,p50_ns,p99_ns,p999_ns,memory_bytes\n";
    }

    // CSV line for benchmark output
    void printCSVLine(std::ostream &os, uint64_t recordCount) {
        endWindow();
        os << recordCount << ","
           << std::fixed << std::setprecision(0) << throughputPerSec << ","
           << latencyHist.p50() << ","
           << latencyHist.p99() << ","
           << latencyHist.p999() << ","
           << memoryUsedBytes << "\n";
    }
};

#endif
