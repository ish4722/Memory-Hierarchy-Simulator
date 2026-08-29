# Memory Hierarchy Simulator

A modular C++17 simulator of an end-to-end memory system: virtual-address translation, TLB caching, page faults and replacement, physical-memory allocation, and a configurable L1/L2/L3 cache hierarchy.

This repository is a derivative work based on the public `beingamanforever/Memory-Hierarchy-Simulator` project and retains its core physical-memory, buddy-allocator, VM, and cache concepts while extending the architecture.

## Architecture

```text
CPU / Workload
      |
      v
    TLB
      | miss
      v
  Page Table
      |
  Page Fault -> CLOCK replacement -> Buddy Allocator
      |
      v
 Physical Address
      |
      v
     L1
      | miss
      v
     L2
      | miss
      v
     L3
      | miss
      v
     RAM
```

## Features

### Virtual Memory
- Per-process page tables (`VPN -> physical frame`)
- TLB with LRU replacement and hit/miss statistics
- On-demand page allocation and page-fault tracking
- CLOCK page replacement
- Physical-frame reclamation during eviction

### Cache Hierarchy
- L1: 32 KiB, 64 B lines, 4-way
- L2: 256 KiB, 64 B lines, 8-way
- L3: 512 KiB, 64 B lines, 16-way
- Configurable LRU/FIFO replacement
- Write-through and write-back modes
- Write-allocate / no-write-allocate configuration
- Per-level hit/miss statistics and modeled latency

### Physical Memory
- Contiguous `std::vector<uint8_t>` RAM model
- Buddy allocator with power-of-two blocks
- Splitting and coalescing
- Thread-safe allocation/free operations

## Build and Run

```sh
make
./sim
```

## Commands

| Command | Description |
|---|---|
| `malloc <bytes>` | Allocate physical memory |
| `free <offset> <bytes>` | Free physical memory |
| `read <pid> <va_hex>` | Translate and read a virtual address |
| `write <pid> <va_hex> <value>` | Translate and write a virtual address |
| `vm` | Show page-fault and TLB state |
| `buddy` | Show buddy allocator state |
| `cache` / `hierarchy` | Show L1/L2/L3 statistics |
| `stats` | Show complete simulator statistics |
| `demo` | Run an integrated workload |
| `help` | Show commands |
| `exit` | Quit |

## Repository Structure

```text
include/     Public interfaces
src/         Implementations
tests/       Workloads
docs/        Architecture and design notes
```

## Original Project

https://github.com/beingamanforever/Memory-Hierarchy-Simulator
