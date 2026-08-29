#pragma once
#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <deque>
#include <vector>
#include <memory>
#include "physical.h"
#include "allocator.h"
#include "tlb.h"

struct VMStats { uint64_t page_faults = 0; uint64_t evictions = 0; };

class VMManager {
public:
    VMManager(PhysicalMemory &pm, Allocator &alloc, size_t page_size = 4096, size_t tlb_entries = 32);
    std::optional<size_t> translate(int pid, uint64_t va, bool allocate_on_fault = true);
    void set_active_pid(int pid) { active_pid_ = pid; }
    VMStats stats() const { return stats_; }
    TLBStats tlb_stats() const { return tlb_.stats(); }
    const TLB &tlb() const { return tlb_; }

    struct ProcPT { std::unordered_map<uint64_t, size_t> page_table; std::deque<uint64_t> clock_order; std::unordered_map<uint64_t, bool> ref_bits; };
    size_t page_size() const { return page_size_; }
    const std::unordered_map<int, ProcPT> &processes() const { return procs_; }

private:
    PhysicalMemory &pm_;
    Allocator &allocator_;
    size_t page_size_;
    int active_pid_ = 0;
    std::unordered_map<int, ProcPT> procs_;
    VMStats stats_;
    TLB tlb_;
    size_t allocate_frame();
    bool evict_frame(ProcPT &pt, int pid);
};
