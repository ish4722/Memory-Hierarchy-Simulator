#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "allocator.h"
#include "cache_hierarchy.h"
#include "physical.h"
#include "vm.h"

namespace {
constexpr size_t kRamBytes = 1 << 20;
constexpr size_t kPageSize = 4096;
constexpr size_t kMinBlock = 64;
constexpr size_t kOperations = 12000;

struct BenchmarkConfig {
    std::string name;
    CacheLevelConfig l1;
    CacheLevelConfig l2;
    CacheLevelConfig l3;
};

struct Workload {
    std::string name;
    std::vector<uint64_t> addresses;
    bool write_every_fourth = false;
};

struct Result {
    std::string config;
    std::string workload;
    uint64_t operations = 0;
    uint64_t reads = 0;
    uint64_t writes = 0;
    uint64_t tlb_hits = 0;
    uint64_t tlb_misses = 0;
    uint64_t page_faults = 0;
    uint64_t evictions = 0;
    CacheHierarchyStats cache;
    double runtime_ms = 0.0;
};

std::vector<BenchmarkConfig> configs() {
    using RP = ReplacementPolicy;
    using WP = WritePolicy;
    using AP = AllocationPolicy;
    return {
        {"baseline",
         {32 * 1024, 64, 4, RP::LRU, WP::WriteThrough, AP::WriteAllocate, 1},
         {256 * 1024, 64, 8, RP::LRU, WP::WriteThrough, AP::WriteAllocate, 4},
         {512 * 1024, 64, 16, RP::LRU, WP::WriteBack, AP::WriteAllocate, 12}},
        {"small_l1",
         {4 * 1024, 64, 2, RP::LRU, WP::WriteThrough, AP::WriteAllocate, 1},
         {64 * 1024, 64, 4, RP::LRU, WP::WriteThrough, AP::WriteAllocate, 4},
         {256 * 1024, 64, 8, RP::LRU, WP::WriteBack, AP::WriteAllocate, 12}},
        {"larger_caches",
         {64 * 1024, 64, 8, RP::LRU, WP::WriteThrough, AP::WriteAllocate, 1},
         {256 * 1024, 64, 8, RP::LRU, WP::WriteThrough, AP::WriteAllocate, 4},
         {512 * 1024, 64, 16, RP::LRU, WP::WriteBack, AP::WriteAllocate, 12}},
        {"fifo",
         {32 * 1024, 64, 4, RP::FIFO, WP::WriteThrough, AP::WriteAllocate, 1},
         {256 * 1024, 64, 8, RP::FIFO, WP::WriteThrough, AP::WriteAllocate, 4},
         {512 * 1024, 64, 16, RP::FIFO, WP::WriteBack, AP::WriteAllocate, 12}},
        {"lfu",
         {32 * 1024, 64, 4, RP::LFU, WP::WriteThrough, AP::WriteAllocate, 1},
         {256 * 1024, 64, 8, RP::LFU, WP::WriteThrough, AP::WriteAllocate, 4},
         {512 * 1024, 64, 16, RP::LFU, WP::WriteBack, AP::WriteAllocate, 12}},
        {"block_16B",
         {32 * 1024, 16, 4, RP::LRU, WP::WriteThrough, AP::WriteAllocate, 1},
         {256 * 1024, 16, 8, RP::LRU, WP::WriteThrough, AP::WriteAllocate, 4},
         {512 * 1024, 16, 16, RP::LRU, WP::WriteBack, AP::WriteAllocate, 12}},
        {"block_32B",
         {32 * 1024, 32, 4, RP::LRU, WP::WriteThrough, AP::WriteAllocate, 1},
         {256 * 1024, 32, 8, RP::LRU, WP::WriteThrough, AP::WriteAllocate, 4},
         {512 * 1024, 32, 16, RP::LRU, WP::WriteBack, AP::WriteAllocate, 12}},
        {"block_128B",
         {32 * 1024, 128, 4, RP::LRU, WP::WriteThrough, AP::WriteAllocate, 1},
         {256 * 1024, 128, 8, RP::LRU, WP::WriteThrough, AP::WriteAllocate, 4},
         {512 * 1024, 128, 16, RP::LRU, WP::WriteBack, AP::WriteAllocate, 12}}
    };
}

std::vector<Workload> workloads() {
    std::vector<Workload> out;
    Workload sequential{"sequential_stream", {}, false};
    Workload repeated{"small_hot_set", {}, false};
    Workload random{"deterministic_random", {}, false};
    Workload mixed{"mixed_read_write", {}, true};
    Workload multi_process{"multi_process_hot_set", {}, false};
    Workload page_pressure{"page_pressure_300_pages", {}, false};
    Workload l1_conflict{"associativity_probe", {}, false};
    Workload spatial{"spatial_locality_4B_stride", {}, false};
    sequential.addresses.reserve(kOperations);
    repeated.addresses.reserve(kOperations);
    random.addresses.reserve(kOperations);
    mixed.addresses.reserve(kOperations);
    multi_process.addresses.reserve(kOperations);
    page_pressure.addresses.reserve(kOperations);
    l1_conflict.addresses.reserve(kOperations);
    spatial.addresses.reserve(kOperations);

    std::mt19937_64 rng(20261023);
    std::uniform_int_distribution<uint64_t> dist(0, 64 * kPageSize - 1);
    for (size_t i = 0; i < kOperations; ++i) {
        // 64-page stream with cache-line stride.
        sequential.addresses.push_back((i * 64) % (64 * kPageSize));
        // Reuse a compact 8-page working set.
        repeated.addresses.push_back((i % (8 * (kPageSize / 64))) * 64);
        // Fixed seed makes the random workload repeatable across runs.
        random.addresses.push_back((dist(rng) / 64) * 64);
        mixed.addresses.push_back(((i * 37) % (32 * kPageSize) / 64) * 64);
        multi_process.addresses.push_back((i % (8 * (kPageSize / 64))) * 64);
        page_pressure.addresses.push_back((i % 300) * kPageSize);
        // Eight virtual pages with a regular stride; physical frame assignment affects set mapping.
        l1_conflict.addresses.push_back((i % 8) * 128 * 64);
        // Four-byte stride gives smaller lines more opportunities to reuse nearby data.
        spatial.addresses.push_back((i * 4) % (64 * kPageSize));
    }
    out.push_back(std::move(sequential));
    out.push_back(std::move(repeated));
    out.push_back(std::move(random));
    out.push_back(std::move(mixed));
    out.push_back(std::move(multi_process));
    out.push_back(std::move(page_pressure));
    out.push_back(std::move(l1_conflict));
    out.push_back(std::move(spatial));
    return out;
}

Result run_one(const BenchmarkConfig &cfg, const Workload &work) {
    PhysicalMemory pm(kRamBytes);
    BuddyAllocator allocator(kRamBytes, kMinBlock);
    VMManager vm(pm, allocator, kPageSize, 32);
    CacheHierarchy cache(pm, cfg.l1, cfg.l2, cfg.l3);
    Result result;
    result.config = cfg.name;
    result.workload = work.name;
    result.operations = work.addresses.size();

    const auto start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < work.addresses.size(); ++i) {
        const int pid = work.name == "multi_process_hot_set" ? 1 + static_cast<int>(i % 2) : 1;
        auto pa = vm.translate(pid, work.addresses[i]);
        if (!pa) continue;
        if (work.write_every_fourth && i % 4 == 0) {
            const uint8_t value = static_cast<uint8_t>(i & 0xff);
            if (cache.write(*pa, &value, 1)) ++result.writes;
        } else {
            uint8_t value = 0;
            if (cache.read(*pa, &value, 1)) ++result.reads;
        }
    }
    const auto stop = std::chrono::steady_clock::now();
    result.runtime_ms = std::chrono::duration<double, std::milli>(stop - start).count();
    const auto ts = vm.tlb_stats();
    const auto vs = vm.stats();
    result.tlb_hits = ts.hits;
    result.tlb_misses = ts.misses;
    result.page_faults = vs.page_faults;
    result.evictions = vs.evictions;
    result.cache = cache.stats();
    return result;
}

void write_csv(const std::vector<Result> &results, const std::string &path) {
    std::ofstream out(path);
    if (!out) {
        std::cerr << "Could not open " << path << " for writing.\n";
        return;
    }
    out << "config,workload,operations,reads,writes,tlb_hits,tlb_misses,tlb_hit_rate_pct,"
           "page_faults,page_evictions,l1_hits,l1_misses,l1_hit_rate_pct,"
           "l2_hits,l2_misses,l2_hit_rate_pct,l3_hits,l3_misses,l3_hit_rate_pct,"
           "memory_accesses,writebacks,total_modeled_latency,runtime_ms\n";
    out << std::fixed << std::setprecision(3);
    for (const auto &r : results) {
        const auto rate = [](uint64_t hits, uint64_t misses) {
            const uint64_t total = hits + misses;
            return total ? 100.0 * static_cast<double>(hits) / total : 0.0;
        };
        out << r.config << ',' << r.workload << ',' << r.operations << ','
            << r.reads << ',' << r.writes << ','
            << r.tlb_hits << ',' << r.tlb_misses << ',' << rate(r.tlb_hits, r.tlb_misses) << ','
            << r.page_faults << ',' << r.evictions << ','
            << r.cache.l1_hits << ',' << r.cache.l1_misses << ',' << rate(r.cache.l1_hits, r.cache.l1_misses) << ','
            << r.cache.l2_hits << ',' << r.cache.l2_misses << ',' << rate(r.cache.l2_hits, r.cache.l2_misses) << ','
            << r.cache.l3_hits << ',' << r.cache.l3_misses << ',' << rate(r.cache.l3_hits, r.cache.l3_misses) << ','
            << r.cache.memory_accesses << ',' << r.cache.writebacks << ','
            << r.cache.total_latency << ',' << r.runtime_ms << '\n';
    }
}

void print_results(const std::vector<Result> &results) {
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "\nRepeatable Memory Hierarchy Benchmark\n"
              << "Each row starts with fresh RAM, allocator, VM, TLB, and caches.\n"
              << "Hit rates are calculated from the simulator's recorded lookup counters.\n"
              << "runtime_ms is host execution time; total_modeled_latency is simulator units.\n\n";
    std::cout << std::left << std::setw(17) << "CONFIG"
              << std::setw(23) << "WORKLOAD"
              << std::right << std::setw(9) << "TLB%"
              << std::setw(9) << "L1%"
              << std::setw(9) << "L2%"
              << std::setw(9) << "L3%"
              << std::setw(10) << "FAULTS"
              << std::setw(12) << "MODEL_LAT"
              << std::setw(12) << "RUNTIMEms" << '\n';
    for (const auto &r : results) {
        const auto rate = [](uint64_t h, uint64_t m) {
            return (h + m) ? 100.0 * static_cast<double>(h) / (h + m) : 0.0;
        };
        std::cout << std::left << std::setw(17) << r.config << std::setw(23) << r.workload
                  << std::right << std::setw(8) << rate(r.tlb_hits, r.tlb_misses)
                  << std::setw(9) << rate(r.cache.l1_hits, r.cache.l1_misses)
                  << std::setw(9) << rate(r.cache.l2_hits, r.cache.l2_misses)
                  << std::setw(9) << rate(r.cache.l3_hits, r.cache.l3_misses)
                  << std::setw(10) << r.page_faults
                  << std::setw(12) << r.cache.total_latency
                  << std::setw(12) << r.runtime_ms << '\n';
    }
}

} // namespace

int main() {
    std::vector<Result> results;
    for (const auto &cfg : configs()) {
        for (const auto &work : workloads()) {
            results.push_back(run_one(cfg, work));
        }
    }
    print_results(results);
    write_csv(results, "benchmark_results.csv");
    std::cout << "\nWrote detailed metrics to benchmark_results.csv ("
              << results.size() << " configuration/workload combinations).\n";
    std::cout << "Note: cache counters reflect current simulator instrumentation; "
                 "they are experimental metrics, not real CPU performance counters.\n";
    std::cout << "Replacement policies: LRU, FIFO, LFU (LFU ties evict the oldest inserted line).\n";
    std::cout << "Block-size configurations vary all three cache levels together at fixed capacities.\n";
    return 0;
}
