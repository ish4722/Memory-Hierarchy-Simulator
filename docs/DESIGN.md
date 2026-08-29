# Design Notes

## Current Architecture

```text
CPU / Workload
      |
     TLB  (VPN -> frame, LRU)
      | miss
  Page Table
      |
  Page Fault -> CLOCK victim -> BuddyAllocator frame reclaim
      |
      v
 Physical Address
      |
     L1  (32 KiB, 4-way, LRU)
      | miss
     L2  (256 KiB, 8-way, LRU)
      | miss
     L3  (512 KiB, 16-way, LRU)
      | miss
     RAM
```

## Virtual Memory

`VMManager::translate()` first checks the TLB using `(pid, VPN)`. A TLB hit returns the cached physical frame immediately. On a TLB miss, the page table is checked. If the page is not mapped, a page fault is recorded and the buddy allocator is asked for a page-sized frame. If physical memory is exhausted, CLOCK scans the process's reference bits, evicts a victim with reference bit 0, returns its physical frame to the buddy allocator, invalidates its TLB entry, and retries allocation.

## TLB

The TLB stores recent `(PID, VPN) -> frame` mappings. It uses an LRU list and hash map for average O(1) lookup plus O(1) recency movement. Statistics track TLB hits and misses.

## Cache Hierarchy

The hierarchy models L1, L2 and L3 levels. Each level has configurable size, line size, associativity, replacement policy, write policy, allocation policy and modeled hit latency. Reads search from L1 to L3 and finally physical memory. Hits promote the block into upper levels.

Supported replacement policies:
- LRU
- FIFO

Supported write modes:
- Write-through
- Write-back

Supported allocation modes:
- Write-allocate
- No-write-allocate

The simulator tracks per-level hits/misses, RAM accesses, writebacks and accumulated modeled latency.

## Physical Memory

`PhysicalMemory` is a contiguous `std::vector<uint8_t>`. `BuddyAllocator` manages page-sized and general power-of-two allocations using free lists, splitting, coalescing and a mutex for thread-safe allocator state.

## Observability

The TUI reports allocator utilization, page faults, evictions, TLB hit rate, and L1/L2/L3 hit/miss statistics. The `demo` command exercises allocation, virtual-address translation, TLB locality, cache accesses and freeing.

## Notes

The original single-cache implementation remains in `cache.h/cache.cpp` as the baseline component; the application now uses `CacheHierarchy` for normal memory accesses. This preserves the original project while making the upgraded hierarchy explicit.
