#pragma once
#include <array>
#include <chrono>
#include <cstdio>
#include <unistd.h>
#include <sys/syscall.h>
namespace SyncTiming {
using Clock = std::chrono::steady_clock;
struct Entry { unsigned long long calls{}; double total{}; double max{}; };
struct Counters {
    std::array<Entry, 5> entries{};
    Clock::time_point last{Clock::now()};
};
inline thread_local Counters counters;
struct Scope {
    unsigned index;
    Clock::time_point start{Clock::now()};
    explicit Scope(unsigned i) : index(i) {}
    ~Scope() {
        const auto now = Clock::now();
        const double ms = std::chrono::duration<double, std::milli>(now-start).count();
        auto& e = counters.entries[index];
        ++e.calls; e.total += ms; if (ms > e.max) e.max = ms;
        const double elapsed = std::chrono::duration<double>(now-counters.last).count();
        if (index != 0 || elapsed < 5) return;
        const double unix_time = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::fprintf(stderr,"SYNC_TIMING unix=%.6f tid=%ld seconds=%.6f",unix_time,syscall(SYS_gettid),elapsed);
        for (unsigned n=0;n<5;++n) {
            const auto& v=counters.entries[n];
            std::fprintf(stderr," slot%u=%llu,%.6f,%.6f",n,v.calls,v.total,v.max);
        }
        std::fputc('\n',stderr);
        counters.entries={};counters.last=now;
    }
};
}
