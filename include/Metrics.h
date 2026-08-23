#ifndef MATCHINGMETRICS_H
#define MATCHINGMETRICS_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <hdr_histogram.h>
#include <iomanip>
#include <iostream>
#include <sys/types.h>
#include <time.h>

#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
static inline uint64_t rdtsc() {
  unsigned int aux;
  return __rdtscp(&aux);
}
#elif defined(__aarch64__)
static inline uint64_t rdtsc() {
  uint64_t val;
  asm volatile("mrs %0, cntvct_el0" : "=r"(val));
  return val;
}
#else
#error "rdtsc: unsupported arch"
#endif

static const uint64_t tsc_mul = [] {
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
  uint64_t c0 = rdtsc();
  struct timespec deadline = {t0.tv_sec, t0.tv_nsec + 5'000'000};
  if (deadline.tv_nsec >= 1'000'000'000) {
    ++deadline.tv_sec;
    deadline.tv_nsec -= 1'000'000'000;
  }
  do {
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
  } while (t1.tv_sec < deadline.tv_sec ||
           (t1.tv_sec == deadline.tv_sec && t1.tv_nsec < deadline.tv_nsec));
  uint64_t c1 = rdtsc();
  uint64_t ns = (uint64_t)(t1.tv_sec - t0.tv_sec) * 1'000'000'000ULL +
                (uint64_t)(t1.tv_nsec - t0.tv_nsec);
  return ((__uint128_t)ns << 32) / (c1 - c0);
}();

static inline uint64_t now_ns() {
  return ((__uint128_t)rdtsc() * tsc_mul) >> 32;
}

struct Metrics {

  struct LatencyHistogram {
    static constexpr int64_t LOWEST = 1;
    static constexpr int64_t HIGHEST = 10'000'000'000LL;
    static constexpr int SIGFIG = 3;

    struct hdr_histogram *histogram{nullptr};

    LatencyHistogram() { hdr_init(LOWEST, HIGHEST, SIGFIG, &histogram); }

    ~LatencyHistogram() {
      if (histogram)
        hdr_close(histogram);
    }

    LatencyHistogram(const LatencyHistogram &) = delete;
    LatencyHistogram &operator=(const LatencyHistogram &) = delete;

    void record(uint64_t ns) {
      hdr_record_value(histogram,
                       ns < LOWEST ? LOWEST : static_cast<int64_t>(ns));
    }

    void reset() {
      hdr_reset(histogram);
      p50 = p99 = p999 = 0;
    }

    uint64_t p50{0};
    uint64_t p99{0};
    uint64_t p999{0};
    void percentile() {
      p50 = static_cast<uint64_t>(hdr_value_at_percentile(histogram, 50.0));
      p99 = static_cast<uint64_t>(hdr_value_at_percentile(histogram, 99.0));
      p999 = static_cast<uint64_t>(hdr_value_at_percentile(histogram, 99.9));
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
      if (n >= 1000000)
        return std::to_string(n / 1000000) + "M";
      if (n >= 1000)
        return std::to_string(n / 1000) + "K";
      return std::to_string(n);
    };

    auto fmtThroughput = [](double t) -> std::string {
      std::ostringstream ss;
      if (t >= 1e6)
        ss << std::fixed << std::setprecision(2) << (t / 1e6) << "M/s";
      else if (t >= 1e3)
        ss << std::fixed << std::setprecision(0) << (t / 1e3) << "K/s";
      else
        ss << std::fixed << std::setprecision(0) << t << "/s";
      return ss.str();
    };

    auto fmtLatency = [](uint64_t ns) -> std::string {
      std::ostringstream ss;
      if (ns >= 1000000)
        ss << std::fixed << std::setprecision(1) << (ns / 1e6) << "ms";
      else if (ns >= 1000)
        ss << std::fixed << std::setprecision(1) << (ns / 1e3) << "us";
      else
        ss << ns << "ns";
      return ss.str();
    };

    auto fmtMem = [](uint64_t b) -> std::string {
      std::ostringstream ss;
      if (b >= 1024 * 1024 * 1024)
        ss << std::fixed << std::setprecision(1) << (b / (1024.0 * 1024 * 1024))
           << "GB";
      else if (b >= 1024 * 1024)
        ss << std::fixed << std::setprecision(0) << (b / (1024.0 * 1024))
           << "MB";
      else if (b >= 1024)
        ss << std::fixed << std::setprecision(0) << (b / 1024.0) << "KB";
      else
        ss << b << "B";
      return ss.str();
    };

    latencyHist.percentile();
    os << "[" << std::setw(6) << fmtCount(recordCount) << "] " << std::setw(8)
       << fmtThroughput(throughputPerSec) << " | "
       << "p50=" << std::setw(6) << fmtLatency(latencyHist.p50) << " "
       << "p99=" << std::setw(6) << fmtLatency(latencyHist.p99) << " "
       << "p99.9=" << std::setw(6) << fmtLatency(latencyHist.p999) << " | "
       << std::setw(6) << fmtMem(memoryUsedBytes) << "\n";
  }

  static void printCSVHeader(std::ostream &os) {
    os << "records,throughput,p50_ns,p99_ns,p999_ns,memory_bytes,adds,modifies,"
          "cancels,trades,fills\n";
  }

  void printCSVLine(std::ostream &os, uint64_t recordCount) {
    endWindow();
    latencyHist.percentile();
    os << recordCount << "," << std::fixed << std::setprecision(0)
       << throughputPerSec << "," << latencyHist.p50 << "," << latencyHist.p99
       << "," << latencyHist.p999 << "," << memoryUsedBytes << "," << adds
       << "," << modifies << "," << cancels << "," << trades << "," << fills
       << "\n";
  }
};

#endif
