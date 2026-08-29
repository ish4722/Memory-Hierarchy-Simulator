# Design Notes

## Baseline Architecture

```text
CLI Interface
     |
Virtual Address
     |
 VMManager (per-process page tables + CLOCK)
     |
Physical Address
     |
 Cache (4-way set associative + LRU + write-through)
     |
 PhysicalMemory (vector<uint8_t>)
     ^
     |
BuddyAllocator (power-of-two blocks + coalescing)
```

## Components

### PhysicalMemory
Contiguous byte array (`std::vector<uint8_t>`) providing raw read/write.

### BuddyAllocator
- Power-of-two block sizes, 64 B minimum by default
- Free lists per order
- Splitting and coalescing
- Thread-safe allocation/free using a mutex

### VMManager
- Per-process page tables (`VPN -> physical frame`)
- Page faults with on-demand frame allocation
- CLOCK replacement with reference bits

### Cache
- Default 32 KiB cache
- 64 B blocks
- 4-way set associativity
- LRU ordering within each set
- Write-through to physical memory

## Upgrade Roadmap

The ACM-project version will extend this baseline with:

1. TLB for cached VPN -> frame translations and TLB hit/miss statistics.
2. Explicit page-fault/frame-reclamation handling and configurable CLOCK/LRU replacement.
3. Multi-level L1/L2/L3 cache hierarchy.
4. Configurable LRU/FIFO cache replacement.
5. Write-through and write-back cache modes, with write-allocate/no-write-allocate policy.
6. Physical-memory fragmentation and utilization metrics.
7. Workload-driven benchmarking and end-to-end memory hierarchy statistics.
