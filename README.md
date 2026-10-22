# Memory Hierarchy Simulator

A modular **C++17 memory-system simulator** that brings together virtual-address translation, a TLB, demand paging, CLOCK-like page replacement, buddy allocation, physical RAM, and a configurable L1/L2/L3 cache hierarchy.

> **Explore the system:** GitHub renders the Mermaid architecture below directly in this README. Expand the sections further down for setup, commands, design notes, and an example access trace.

<p align="center">
  <img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus">
  <img alt="Build" src="https://img.shields.io/badge/build-Makefile-informational">
  <img alt="Memory model" src="https://img.shields.io/badge/RAM-1%20MiB-purple">
  <img alt="Cache levels" src="https://img.shields.io/badge/cache-L1%20%7C%20L2%20%7C%20L3-orange">
</p>

## Contents

- [System architecture](#system-architecture)
- [How an access works](#how-an-access-works)
- [Configuration](#configuration)
- [Build and run](#build-and-run)
- [Interactive command reference](#interactive-command-reference)
- [Project structure](#project-structure)
- [Implementation notes and limitations](#implementation-notes-and-limitations)

## System architecture

The diagram follows the system from startup and creation of **1 MiB of zero-initialized physical memory**, through virtual-address translation and allocation, into the cache hierarchy. The six blocks `B1`–`B6` are an illustrative sequence of memory accesses—not a claim that every access misses.

```mermaid
flowchart TD
    START(["Start simulator"])

    subgraph INIT["1 · Startup — src/main.cpp"]
        RAM["PhysicalMemory pm<br/>mem_ = vector&lt;uint8_t&gt;(1 &lt;&lt; 20, 0)<br/>1 MiB = 1,048,576 bytes"]
        BUDDY["BuddyAllocator alloc<br/>min_block = 64 bytes<br/>Initialize free lists"]
        VM["VMManager vm<br/>page_size = 4096 bytes<br/>TLB capacity = 32 entries"]
        CACHE["CacheHierarchy cache<br/>L1: 32 KiB · 64 B line · 4-way<br/>L2: 256 KiB · 64 B line · 8-way<br/>L3: 512 KiB · 64 B line · 16-way"]
        POLICIES["Configured policies<br/>L1/L2: LRU, write-through, write-allocate<br/>L3: LRU, write-back, write-allocate<br/>Hit latency: 1 / 4 / 12"]
        RAM --- BUDDY
        RAM --- VM
        RAM --- CACHE
        CACHE --- POLICIES
    end

    START --> INIT
    INIT --> LOOP

    LOOP{"2 · Read command, write command,<br/>demo, allocator command, or stats?"}
    LOOP -->|read PID VA| READ["Read request<br/>PID + virtual address VA"]
    LOOP -->|write PID VA value| WRITE["Write request<br/>PID + virtual address VA + byte value"]
    LOOP -->|malloc / free| ALLOC_CMD["Allocator command"]
    LOOP -->|stats / vm / buddy / cache / hierarchy| STATS["Print simulator state and counters"]
    LOOP -->|demo| DEMO["Demo workload<br/>Allocate sample blocks<br/>Write and reread virtual pages<br/>Free selected allocations"]
    LOOP -->|exit / quit| END(["Exit"])

    subgraph VMFLOW["3 · Virtual-memory translation — src/vm.cpp and src/tlb.cpp"]
        SPLIT["Split virtual address<br/>VPN = VA / page_size<br/>offset = VA % page_size"]
        TLB{"TLB lookup<br/>key = (PID, VPN)"}
        TLBH["TLB hit<br/>Return cached frame"]
        TLBM["TLB miss<br/>Look up process page table"]
        PT{"VPN mapped<br/>in page_table?"}
        PTH["Page-table hit<br/>Set ref_bits[VPN] = true<br/>Insert translation into TLB"]
        FAULT["Page fault<br/>Increment page_faults"]
        ALLOCFRAME["Request page-sized frame<br/>allocator.alloc(4096)<br/>Buddy allocator rounds to a power-of-two block"]
        GOTFRAME{"Frame allocated?"}
        CLOCK["CLOCK-like scan of this process's clock_order deque<br/>Referenced page: clear ref bit and rotate to back<br/>Unreferenced mapped page: choose as victim"]
        RECLAIM["Reclaim victim frame<br/>allocator.free(frame, page_size)<br/>Erase page_table and ref_bits entries<br/>Invalidate (PID, VPN) in TLB<br/>Retry frame allocation once"]
        INSTALL["Install new mapping<br/>page_table[VPN] = frame<br/>ref_bits[VPN] = true<br/>clock_order.push_back(VPN)<br/>Insert mapping into TLB"]
        PA["Physical address<br/>PA = frame + offset"]
        FAIL["Translation fails<br/>Return no physical address"]

        SPLIT --> TLB
        TLB -->|hit| TLBH
        TLB -->|miss| TLBM
        TLBM --> PT
        PT -->|yes| PTH
        PT -->|no| FAULT
        FAULT --> ALLOCFRAME
        ALLOCFRAME --> GOTFRAME
        GOTFRAME -->|yes| INSTALL
        GOTFRAME -->|no| CLOCK
        CLOCK -->|victim found| RECLAIM
        RECLAIM --> ALLOCFRAME
        CLOCK -->|no victim| FAIL
        GOTFRAME -. "retry still fails" .-> FAIL
        TLBH --> PA
        PTH --> PA
        INSTALL --> PA
    end

    READ --> SPLIT
    WRITE --> SPLIT

    subgraph CACHEFLOW["4 · Physical-address access — src/cache_hierarchy.cpp"]
        BLOCK["Identify cache line<br/>block_start = floor(PA / 64) × 64<br/>byte_offset = PA − block_start"]
        L1{"L1 lookup"}
        L1H["L1 hit<br/>Return requested bytes<br/>Update LRU order"]
        L1M["L1 miss<br/>Continue to L2"]
        L2{"L2 lookup"}
        L2H["L2 hit<br/>Return line data<br/>Fill/promote into L1"]
        L2M["L2 miss<br/>Continue to L3"]
        L3{"L3 lookup"}
        L3H["L3 hit<br/>Return line data<br/>Fill/promote into L2 and L1"]
        L3M["L3 miss<br/>Read from physical RAM"]
        MEMREAD["PhysicalMemory.read(block_start, buffer, line_size)<br/>Read bytes from mem_<br/>Fill L3, L2, and L1"]
        RESULT["Return requested data<br/>Update cache counters and modeled latency"]

        BLOCK --> L1
        L1 -->|hit| L1H
        L1 -->|miss| L1M
        L1M --> L2
        L2 -->|hit| L2H
        L2 -->|miss| L2M
        L2M --> L3
        L3 -->|hit| L3H
        L3 -->|miss| L3M
        L3M --> MEMREAD
        L1H --> RESULT
        L2H --> RESULT
        L3H --> RESULT
        MEMREAD --> RESULT
    end

    PA --> BLOCK

    subgraph WRITES["5 · Write path"]
        WHIT{"Is the line already in L1?"}
        WH["L1 write hit<br/>Update bytes in the line"]
        WMISS["L1 write miss<br/>If write-allocate: read/fill line first<br/>If no-write-allocate: bypass cache"]
        WTP["Write-through policy<br/>Propagate write to lower levels when present<br/>and write to physical RAM"]
        WBP["Write-back policy<br/>Mark modified line dirty<br/>Write dirty data to RAM on eviction"]
        WEND["Complete write<br/>Update counters / latency"]
        WHIT -->|yes| WH
        WHIT -->|no| WMISS
        WH --> WTP
        WH --> WBP
        WMISS --> WTP
        WMISS --> WBP
        WTP --> WEND
        WBP --> WEND
    end

    WRITE --> WHIT
    BLOCK -. "write request uses cache policy path" .-> WHIT
    RESULT --> AGAIN

    subgraph ALLOCATOR["6 · Buddy allocator — src/allocator.cpp"]
        REQUEST["alloc(bytes)<br/>Compute smallest fitting order"]
        SPLITBLOCK["Find a free block<br/>Split larger blocks into buddies<br/>Return selected block offset"]
        FREECMD["free(offset, bytes)<br/>Compute buddy offset = offset XOR block_size"]
        MERGE["If matching buddy is free:<br/>remove buddy and merge upward<br/>otherwise stop merging"]
        FREELIST["Insert final block into free_lists_<br/>Mutex protects allocator operations"]
        REQUEST --> SPLITBLOCK
        FREECMD --> MERGE
        MERGE --> FREELIST
    end

    BUDDY --> REQUEST
    ALLOC_CMD --> REQUEST
    ALLOC_CMD --> FREECMD
    ALLOCFRAME --> REQUEST
    RECLAIM --> FREECMD

    subgraph MEMORY["7 · Physical RAM — src/physical.cpp"]
        MEM["std::vector&lt;uint8_t&gt; mem_<br/>1 MiB byte-addressable storage<br/>read() / write() copy bytes after bounds checks"]
    end

    RAM --> MEM
    MEMREAD --> MEM
    WTP --> MEM
    WBP --> MEM

    subgraph TLBDETAIL["8 · TLB data structures"]
        MAP["unordered_map&lt;Key, list iterator&gt;<br/>Fast lookup by PID + VPN"]
        LRU["LRU list of translations<br/>Hit: move entry to front<br/>Overflow: remove least-recently-used entry"]
        INV["invalidate(PID, VPN)<br/>Remove stale mapping on page eviction"]
        MAP <--> LRU
        INV --> MAP
    end

    VM --> MAP
    RECLAIM --> INV

    subgraph EXAMPLE["9 · Illustrative six-block sequence"]
        SEQ["B1 → B2 → B3 → B4 → B5 → B6<br/>Each block represents an access to memory"]
        EXPLAIN["For each block:<br/>translate VA → obtain PA → check L1/L2/L3/RAM<br/>read or write bytes → update statistics"]
        SEQ --> EXPLAIN
    end

    DEMO --> SEQ
    AGAIN{"More accesses?"}
    RESULT --> AGAIN
    WEND --> AGAIN
    AGAIN -->|yes| READ
    AGAIN -->|no| LOOP
    STATS --> LOOP
    FREELIST --> LOOP
```

<details>
<summary><strong>Read the diagram: key variables</strong></summary>

| Variable | Meaning |
|---|---|
| `ram_bytes` | Total simulated physical RAM: `1 << 20` bytes (1 MiB) |
| `page_size` | Virtual-memory page size: 4096 bytes (4 KiB) |
| `min_block` | Minimum buddy-allocation block: 64 bytes |
| `VA` | Virtual address supplied by the workload |
| `VPN` | Virtual page number: `VA / page_size` |
| `offset` | Offset inside the page: `VA % page_size` |
| `frame` | Physical frame base offset returned by the allocator |
| `PA` | Physical address returned by `VMManager::translate()`; in this implementation it is `frame + offset` |
| `page_table` | Per-process map from VPN to allocated frame offset |
| `ref_bits` | Per-page reference bits used by CLOCK-like eviction |
| `clock_order` | Deque used to scan and rotate candidate virtual pages |
| `mem_` | The actual byte array representing physical RAM |
| `block_start` | Start address of the cache line containing a physical address |
| `byte_offset` | Position within a cache line |
| `free_lists_` | Buddy allocator's free blocks grouped by allocation order |

</details>

## How an access works

<details>
<summary><strong>1. Startup and memory creation</strong></summary>

The program creates four shared objects:

- `PhysicalMemory pm(1 << 20)`: a 1 MiB zero-initialized byte vector.
- `BuddyAllocator alloc(ram_bytes, 64)`: tracks free blocks in power-of-two orders.
- `VMManager vm(pm, alloc, 4096, 32)`: translates addresses and uses a 32-entry TLB.
- `CacheHierarchy cache(pm, l1, l2, l3)`: all cache levels use the same physical-memory object.

The allocator and virtual-memory manager share the same physical address space. The cache hierarchy reads from and writes to that same RAM object.

</details>

<details>
<summary><strong>2. TLB, page table, and page fault</strong></summary>

For a virtual address `VA`, the manager calculates:

```cpp
vpn    = VA / page_size;
offset = VA % page_size;
```

It first looks up `(PID, VPN)` in the TLB. On a TLB miss, it checks the process's page table. If the mapping exists, it updates the reference bit and inserts the mapping into the TLB. If the mapping is absent, it records a page fault and—when allocation on fault is enabled—requests a page-sized allocation.

If allocation fails, the implementation scans the **current process's** CLOCK deque, reclaims a victim if possible, and retries allocation once. A TLB hit returns directly from the cached mapping; it does not currently update the page's CLOCK reference bit.

</details>

<details>
<summary><strong>3. L1 → L2 → L3 → RAM</strong></summary>

A cache line is 64 bytes by default. The hierarchy checks L1 first, then L2, then L3, and finally physical RAM. A hit at a lower cache level fills/promotes data into upper levels; an L3 miss causes a physical-memory read. The configured cache hit latencies are 1, 4, and 12 units, and RAM accesses add a modeled 100 units.

For the six-block illustration, every block can have a different outcome. Repeated accesses may hit in a cache; conflicting addresses may evict one another. The actual result depends on the addresses and the cache's set mapping.

</details>

<details>
<summary><strong>4. Buddy allocation and coalescing</strong></summary>

The buddy allocator groups free blocks by power-of-two size. An allocation searches for a suitable free block and splits a larger block as needed. When a block is freed, the allocator checks whether its buddy is also free; if so, it merges the pair and continues upward.

The buddy address is calculated with XOR against the block size. The allocator uses a mutex around allocation and deallocation operations.

</details>

## Configuration

| Component | Configuration | Behavior |
|---|---|---|
| Physical RAM | 1 MiB | Zero-initialized `std::vector<uint8_t>` |
| Virtual memory | 4 KiB page | On-demand mapping and page-fault accounting |
| TLB | 32 entries | LRU; key includes PID and VPN |
| L1 cache | 32 KiB, 64 B lines, 4-way | LRU, write-through, write-allocate, hit latency 1 |
| L2 cache | 256 KiB, 64 B lines, 8-way | LRU, write-through, write-allocate, hit latency 4 |
| L3 cache | 512 KiB, 64 B lines, 16-way | LRU, write-back, write-allocate, hit latency 12 |
| Buddy allocator | Minimum block 64 B | Power-of-two splitting and coalescing |

Cache capacities and policies are initialized in `src/main.cpp`. These are simulation settings, not a claim about the exact cache configuration of a real CPU.

## Build and run

Requirements: a C++17 compiler and GNU Make.

```sh
make
./sim
```

To rebuild from scratch:

```sh
make clean
make
./sim
```

## Repeatable benchmarks

Build and run the benchmark executable separately from the interactive simulator:

```sh
make bench
./bench
```

The benchmark runs seven deterministic workloads against four cache configurations (28 combinations total). Every combination starts with fresh simulated RAM, allocator, VM, TLB, and caches, so previous cases do not contaminate later counters.

| Workload | Pattern | What it helps investigate |
|---|---|---|
| `sequential_stream` | Sequential 64-byte cache-line strides across a 64-page region | Streaming access behavior |
| `small_hot_set` | Repeated accesses within an 8-page working set | Temporal locality and cache reuse |
| `deterministic_random` | Seeded pseudo-random line accesses across 64 pages | Less predictable locality |
| `mixed_read_write` | Deterministic line accesses with every fourth operation a write | Read/write behavior under the current write policy implementation |
| `multi_process_hot_set` | Two PIDs alternate accesses to the same virtual-address working set | Per-process virtual mappings and TLB behavior under interleaved processes |
| `page_pressure_300_pages` | Cycles through 300 virtual pages in 1 MiB of physical RAM | Page faults, frame reclamation, and CLOCK-like eviction under memory pressure |
| `associativity_probe` | Reuses eight virtual pages at a regular stride | How regular address strides and physical frame placement interact with cache set mapping; interpret alongside the recorded counters |

| Configuration | L1 | L2 | L3 | Replacement |
|---|---|---|---|---|
| `baseline` | 32 KiB, 4-way | 256 KiB, 8-way | 512 KiB, 16-way | LRU |
| `small_l1` | 4 KiB, 2-way | 64 KiB, 4-way | 256 KiB, 8-way | LRU |
| `larger_caches` | 64 KiB, 8-way | 256 KiB, 8-way | 512 KiB, 16-way | LRU |
| `fifo` | 32 KiB, 4-way | 256 KiB, 8-way | 512 KiB, 16-way | FIFO |

The command prints a summary table and writes detailed rows to `benchmark_results.csv` in the current working directory. L1 counters count top-level cache lookups once, including write misses; lower-level counters represent the lookups reached after an upper-level miss. A write-allocate fill no longer double-counts its internal L1 lookup. The CSV includes operation counts, TLB hits/misses and hit rate, page faults and evictions, cache-level hits/misses and hit rates, memory accesses, writebacks, total modeled latency, and host runtime in milliseconds.

**Metric interpretation:** hit rate is calculated from the simulator's recorded lookup counters. The current cache implementation can perform an additional cache lookup when a write miss triggers line fill, so cache-level lookup counts are not necessarily one lookup per top-level operation. `total_modeled_latency` is the simulator's configured latency model—not measured CPU cycles. `runtime_ms` is actual host wall-clock runtime and can vary with machine load and compiler settings. Compare runs under the same build and environment; do not describe modeled latency as real hardware speedup.

For a repeatable local run, use a fresh build and save the terminal output alongside `benchmark_results.csv`:

```sh
make clean
make bench
./bench
```

## Interactive command reference

Start `./sim`, then enter commands in the terminal.

| Command | Example | Description |
|---|---|---|
| `help` | `help` | Show supported commands |
| `demo` | `demo` | Run allocation, translation, write, reread, and free operations |
| `read` | `read 1 1000` | Translate a hexadecimal virtual address and read one byte |
| `write` | `write 1 1000 42` | Translate a hexadecimal virtual address and write a byte value |
| `malloc` | `malloc 128` | Allocate a block of the requested size |
| `free` | `free 0 128` | Free a block using its offset and requested size |
| `vm` | `vm` | Show virtual-memory, page-fault, and TLB state |
| `buddy` | `buddy` | Show buddy allocator state |
| `cache` | `cache` | Show cache hierarchy statistics |
| `hierarchy` | `hierarchy` | Show L1/L2/L3 hierarchy statistics |
| `stats` | `stats` | Show allocator, VM, and cache statistics |
| `clear` | `clear` | Clear the terminal |
| `exit` / `quit` | `exit` | Quit the simulator |

**Address format:** the `read` and `write` commands parse the virtual address as hexadecimal. The write value is parsed as an integer and stored as a single byte.

## Project structure

```text
Memory-Hierarchy-Simulator/
├── include/
│   ├── allocator.h
│   ├── cache.h
│   ├── cache_hierarchy.h
│   ├── physical.h
│   ├── tlb.h
│   ├── tui.h
│   └── vm.h
├── src/
│   ├── allocator.cpp
│   ├── cache.cpp
│   ├── cache_hierarchy.cpp
│   ├── main.cpp
│   ├── physical.cpp
│   ├── tlb.cpp
│   └── vm.cpp
├── tests/
│   └── workload.txt
├── docs/
│   └── DESIGN.md
├── Makefile
└── README.md
```

## Implementation notes and limitations

This is a learning-oriented simulator: its counters and modeled latencies help explore memory-system concepts, but they are not cycle-accurate hardware measurements.

- CLOCK-like replacement scans the page table for the process currently faulting; it is not a global replacement policy.
- TLB hits currently bypass the page-reference-bit update, so CLOCK metadata may not reflect every recent access.
- The cache implementation models line fills, replacement, and write policies; validate write propagation, dirty-line eviction, and physical-frame reuse carefully before relying on data-consistency or performance claims.
- The `free` command trusts the supplied offset and size; invalid frees, incorrect sizes, and double frees are not comprehensively guarded.
- The bounds checks in physical-memory reads and writes should be hardened against integer overflow for adversarial inputs.
- The legacy single-cache implementation in `cache.cpp` remains in the project; normal application accesses use `CacheHierarchy`.

## Engineering goals

- Make address translation and page faults observable.
- Demonstrate TLB locality and LRU eviction.
- Explore power-of-two allocation, splitting, and buddy coalescing.
- Compare cache hits, misses, replacement policies, write policies, and modeled latency.
- Keep components modular so individual subsystems can be inspected and improved.

---

**Suggested next experiment:** run `demo`, then inspect `vm`, `buddy`, and `hierarchy`. Repeat a read to the same virtual address and compare the counters.
