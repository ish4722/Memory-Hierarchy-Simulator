#include "vm.h"
#include <limits>

VMManager::VMManager(PhysicalMemory &pm, Allocator &alloc, size_t page_size, size_t tlb_entries)
    : pm_(pm), allocator_(alloc), page_size_(page_size), tlb_(tlb_entries) {}

size_t VMManager::allocate_frame() {
    auto opt = allocator_.alloc(page_size_);
    return opt ? *opt : std::numeric_limits<size_t>::max();
}

bool VMManager::evict_frame(ProcPT &pt, int pid) {
    while (!pt.clock_order.empty()) {
        uint64_t vpn = pt.clock_order.front();
        pt.clock_order.pop_front();
        auto ref_it = pt.ref_bits.find(vpn);
        bool ref = ref_it != pt.ref_bits.end() && ref_it->second;
        if (!ref) {
            auto page_it = pt.page_table.find(vpn);
            if (page_it == pt.page_table.end()) continue;
            size_t frame = page_it->second;
            allocator_.free(frame, page_size_);
            pt.page_table.erase(page_it);
            pt.ref_bits.erase(vpn);
            tlb_.invalidate(pid, vpn);
            ++stats_.evictions;
            return true;
        }
        pt.ref_bits[vpn] = false;
        pt.clock_order.push_back(vpn);
    }
    return false;
}

std::optional<size_t> VMManager::translate(int pid, uint64_t va, bool allocate_on_fault) {
    uint64_t vpn = va / page_size_;
    size_t offset = va % page_size_;

    size_t frame = 0;
    if (tlb_.lookup(pid, vpn, frame)) return frame + offset;

    auto &pt = procs_[pid];
    auto it = pt.page_table.find(vpn);
    if (it != pt.page_table.end()) {
        frame = it->second;
        pt.ref_bits[vpn] = true;
        tlb_.insert(pid, vpn, frame);
        return frame + offset;
    }

    ++stats_.page_faults;
    if (!allocate_on_fault) return std::nullopt;

    frame = allocate_frame();
    if (frame == std::numeric_limits<size_t>::max()) {
        if (!evict_frame(pt, pid)) return std::nullopt;
        frame = allocate_frame();
        if (frame == std::numeric_limits<size_t>::max()) return std::nullopt;
    }

    pt.page_table[vpn] = frame;
    pt.ref_bits[vpn] = true;
    pt.clock_order.push_back(vpn);
    tlb_.insert(pid, vpn, frame);
    return frame + offset;
}
