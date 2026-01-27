//
// Created by Eleazar Vega on 01/11/26.
//

#ifndef CPUMETRICS_H
#define CPUMETRICS_H

#include <cstdint>
#include <iostream>
#include <iomanip>
#include <chrono>

// Platform-specific includes
#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS
    #include <windows.h>
    #include <psapi.h>
#elif defined(__APPLE__) || defined(__linux__) || defined(__unix__)
    #define PLATFORM_POSIX
    #include <sys/resource.h>
    #include <sys/time.h>
    #if defined(__APPLE__)
        #include <mach/mach.h>
    #endif
#endif

struct CpuMetrics {

    // ─────────────────────────────────────────────────────────────────────────
    // CPU Time (microseconds)
    // ─────────────────────────────────────────────────────────────────────────
    struct CpuTime {
        uint64_t userTimeUs{0};      // Time spent in user mode
        uint64_t systemTimeUs{0};    // Time spent in kernel mode
        uint64_t totalTimeUs{0};     // User + System

        void reset() {
            userTimeUs = 0;
            systemTimeUs = 0;
            totalTimeUs = 0;
        }
    };

    // ─────────────────────────────────────────────────────────────────────────
    // Context Switches
    // ─────────────────────────────────────────────────────────────────────────
    struct ContextSwitches {
        uint64_t voluntary{0};       // Yielded CPU voluntarily (I/O wait, etc.)
        uint64_t involuntary{0};     // Preempted by scheduler

        void reset() {
            voluntary = 0;
            involuntary = 0;
        }
    };

    // ─────────────────────────────────────────────────────────────────────────
    // Snapshot for interval calculations
    // ─────────────────────────────────────────────────────────────────────────
    struct Snapshot {
        CpuTime cpu;
        ContextSwitches ctx;
        std::chrono::steady_clock::time_point wallTime;
        bool valid{false};
    };

    // ─────────────────────────────────────────────────────────────────────────
    // Current values (updated on demand)
    // ─────────────────────────────────────────────────────────────────────────
    CpuTime current;
    ContextSwitches contextSwitches;

    // For interval-based CPU percentage calculation
    Snapshot lastSnapshot;
    double cpuPercentage{0.0};

    // ─────────────────────────────────────────────────────────────────────────
    // Methods
    // ─────────────────────────────────────────────────────────────────────────

    void reset() {
        current.reset();
        contextSwitches.reset();
        lastSnapshot.valid = false;
        cpuPercentage = 0.0;
    }

    // Capture current CPU usage from OS
    bool update() {
#if defined(PLATFORM_POSIX)
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
            current.userTimeUs = static_cast<uint64_t>(usage.ru_utime.tv_sec) * 1000000ULL
                               + static_cast<uint64_t>(usage.ru_utime.tv_usec);
            current.systemTimeUs = static_cast<uint64_t>(usage.ru_stime.tv_sec) * 1000000ULL
                                 + static_cast<uint64_t>(usage.ru_stime.tv_usec);
            current.totalTimeUs = current.userTimeUs + current.systemTimeUs;

            contextSwitches.voluntary = static_cast<uint64_t>(usage.ru_nvcsw);
            contextSwitches.involuntary = static_cast<uint64_t>(usage.ru_nivcsw);
            return true;
        }
        return false;

#elif defined(PLATFORM_WINDOWS)
        FILETIME createTime, exitTime, kernelTime, userTime;
        if (GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime)) {
            // FILETIME is in 100-nanosecond intervals
            auto fileTimeToUs = [](const FILETIME& ft) -> uint64_t {
                ULARGE_INTEGER uli;
                uli.LowPart = ft.dwLowDateTime;
                uli.HighPart = ft.dwHighDateTime;
                return uli.QuadPart / 10;  // Convert to microseconds
            };

            current.userTimeUs = fileTimeToUs(userTime);
            current.systemTimeUs = fileTimeToUs(kernelTime);
            current.totalTimeUs = current.userTimeUs + current.systemTimeUs;

            // Windows doesn't provide context switches via GetProcessTimes
            // Would need performance counters for that
            contextSwitches.voluntary = 0;
            contextSwitches.involuntary = 0;
            return true;
        }
        return false;
#else
        // Unsupported platform - use clock() as fallback
        std::clock_t cpuTime = std::clock();
        if (cpuTime != static_cast<std::clock_t>(-1)) {
            current.userTimeUs = static_cast<uint64_t>(cpuTime) * 1000000ULL / CLOCKS_PER_SEC;
            current.systemTimeUs = 0;
            current.totalTimeUs = current.userTimeUs;
            return true;
        }
        return false;
#endif
    }

    // Take a snapshot for interval calculations
    void takeSnapshot() {
        update();
        lastSnapshot.cpu = current;
        lastSnapshot.ctx = contextSwitches;
        lastSnapshot.wallTime = std::chrono::steady_clock::now();
        lastSnapshot.valid = true;
    }

    // Calculate CPU percentage since last snapshot
    double calculateCpuPercentage() {
        if (!lastSnapshot.valid) {
            takeSnapshot();
            return 0.0;
        }

        Snapshot prev = lastSnapshot;
        takeSnapshot();

        auto wallElapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
            lastSnapshot.wallTime - prev.wallTime
        ).count();

        if (wallElapsedUs <= 0) {
            return 0.0;
        }

        uint64_t cpuElapsedUs = lastSnapshot.cpu.totalTimeUs - prev.cpu.totalTimeUs;
        cpuPercentage = (static_cast<double>(cpuElapsedUs) / static_cast<double>(wallElapsedUs)) * 100.0;

        return cpuPercentage;
    }

    void print(std::ostream &os) const {
        os << "CPU USAGE\n";
        os << "  User time:             " << formatTime(current.userTimeUs) << "\n";
        os << "  System time:           " << formatTime(current.systemTimeUs) << "\n";
        os << "  Total CPU time:        " << formatTime(current.totalTimeUs) << "\n";
        os << "  CPU percentage:        " << std::fixed << std::setprecision(2)
           << cpuPercentage << "%\n";
#if defined(PLATFORM_POSIX)
        os << "  Voluntary ctx sw:      " << contextSwitches.voluntary << "\n";
        os << "  Involuntary ctx sw:    " << contextSwitches.involuntary << "\n";
#endif
    }

    static std::string formatTime(uint64_t microseconds) {
        std::ostringstream oss;
        if (microseconds >= 1000000) {
            double seconds = static_cast<double>(microseconds) / 1000000.0;
            oss << std::fixed << std::setprecision(3) << seconds << " s";
        } else if (microseconds >= 1000) {
            double milliseconds = static_cast<double>(microseconds) / 1000.0;
            oss << std::fixed << std::setprecision(3) << milliseconds << " ms";
        } else {
            oss << microseconds << " us";
        }
        return oss.str();
    }

    static std::string getPlatformInfo() {
#if defined(PLATFORM_WINDOWS)
        return "Windows";
#elif defined(__APPLE__)
    #if defined(__arm64__)
        return "macOS (ARM64)";
    #else
        return "macOS (x86_64)";
    #endif
#elif defined(__linux__)
    #if defined(__aarch64__)
        return "Linux (ARM64)";
    #elif defined(__x86_64__)
        return "Linux (x86_64)";
    #else
        return "Linux (other)";
    #endif
#else
        return "Unknown";
#endif
    }
};

#endif //CPUMETRICS_H
