# Memory Hierarchy Simulator

A modular **C++17 memory-system simulator** that models virtual-address translation, TLB caching, page faults and replacement, physical-memory allocation, and a configurable multi-level L1/L2/L3 cache hierarchy.

## Architecture

```text
                         CPU / Workload
                              |
                              v
                            TLB
                         hit | miss
                             |   \
                             |    v
                             | Page Table
                             |    |
                             | Page Fault
                             |    |
                             | CLOCK Replacement
                             |    |
                             | Buddy Allocator
                             |    |
                             +----+
                                  |
                           Physical Address
                                  |
                                  v
                             +---------+
                             |   L1    |
                             | 32 KiB  |
                             +----+----+
                                  | miss
                                  v
                             +---------+
                             |   L2    |
                             | 256 KiB |
                             +----+----+
                                  | miss
                                  v
                             +---------+
                             |   L3    |
                             | 512 KiB |
                             +----+----+
                                  | miss
                                  v
                                RAM
```

## Features

### Virtual Memory

- Per-process page tables mapping virtual page numbers to physical frames
- TLB for caching recent address translations
- TLB LRU replacement with hit/miss statistics
- On-demand page allocation and page-fault tracking
- CLOCK page replacement when physical frames are exhausted
- Physical-frame reclamation during eviction

### Cache Hierarchy

- Three configurable cache levels: L1, L2, and L3
- Set-associative cache organization
- Configurable cache-line size and associativity
- LRU/FIFO replacement support
- Write-through and write-back policies
- Write-allocate and no-write-allocate modes
- Per-level hit/miss statistics and modeled access latency

### Physical Memory

- Contiguous `std::vector<uint8_t>` RAM model
- Buddy allocator using power-of-two blocks
- Block splitting and buddy coalescing
- Thread-safe allocation and deallocation

## End-to-End Access Path

A virtual-memory read/write follows this conceptual path:

```text
Virtual Address
      |
      v
     TLB
      |
      +---- HIT ----> Physical Address
      |
     MISS
      |
      v
  Page Table
      |
      +---- Valid ----> Physical Address
      |
   Page Fault
      |
      v
Frame Allocation / CLOCK Eviction
      |
      v
  Physical Address
      |
      v
     L1
      |
    miss
      v
     L2
      |
    miss
      v
     L3
      |
    miss
      v
     RAM
```

## Build and Run

```sh
make
./sim
```

## Commands

| Command | Description |
|---|---|
| `malloc <bytes>` | Allocate physical memory using the buddy allocator |
| `free <offset> <bytes>` | Free an allocated physical-memory block |
| `read <pid> <va_hex>` | Translate and read a virtual address |
| `write <pid> <va_hex> <value>` | Translate and write a virtual address |
| `vm` | Show virtual-memory, page-fault, and TLB state |
| `buddy` | Show buddy-allocator state |
| `cache` | Show cache state/statistics |
| `hierarchy` | Show L1/L2/L3 hierarchy statistics |
| `stats` | Show complete simulator statistics |
| `demo` | Run an integrated demonstration workload |
| `clear` | Clear the terminal |
| `help` | Show available commands |
| `exit` | Quit the simulator |

## Project Structure

```text
include/
  allocator.h
  cache.h
  cache_hierarchy.h
  physical.h
  tlb.h
  tui.h
  vm.h

src/
  allocator.cpp
  cache.cpp
  cache_hierarchy.cpp
  main.cpp
  physical.cpp
  tlb.cpp
  vm.cpp

tests/
  workload.txt

docs/
  DESIGN.md

Makefile
README.md
```

## Engineering Focus

The project is designed to make memory-system behavior observable rather than treating memory as a single flat abstraction. It exposes translation, replacement, allocation, cache hierarchy, and performance statistics through a single interactive simulator.
