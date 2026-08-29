# Design Notes

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    CLI Interface                         │
│              (main.cpp + tui.h)                          │
├─────────────────────────────────────────────────────────┤
│                                                          │
│   Virtual Address (VA)                                   │
│          │                                               │
│          ▼                                               │
│   ┌─────────────────┐                                    │
│   │   VMManager     │  Per-process page tables           │
│   │   (CLOCK)       │  Page fault handling               │
│   └────────┬────────┘                                    │
│            │ Physical Address (PA)                       │
│            ▼                                             │
│   ┌─────────────────┐                                    │
│   │     Cache       │  4-way set-associative             │
│   │     (LRU)       │  Write-through                     │
│   └────────┬────────┘                                    │
│            │                                             │
│            ▼                                             │
│   ┌─────────────────┐     ┌─────────────────┐            │
│   │ PhysicalMemory  │◄────│ BuddyAllocator  │            │
│   │ (vector<u8>)    │     │ (power-of-2)    │            │
│   └─────────────────┘     └─────────────────┘            │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

## Components

### PhysicalMemory
Contiguous byte array (`std::vector<uint8_t>`). Provides raw read/write.

### BuddyAllocator
- Power-of-two block sizes (min 64B default)
- Free lists per order
- O(log n) alloc/free via split/coalesce
- Thread-safe (mutex)

### VMManager
- Per-process page tables (`unordered_map<vpn, frame>`)
- CLOCK replacement (circular queue with reference bits)
- On-demand frame allocation via BuddyAllocator

### Cache
- Set-associative (default 32KB, 64B blocks, 4-way)
- LRU per set (list ordering)
- Write-through to PhysicalMemory

## Data Flow

1. CPU issues read/write with (pid, virtual_address)
2. VMManager translates VA -> PA (may trigger page fault)
3. Cache checks for PA; hit returns data, miss fetches from RAM
4. Write-through ensures RAM consistency

## Visualization

Terminal UI (`tui.h`) provides:
- Progress bars for utilization/hit-rate
- Tables for buddy free lists and page tables
- Compact block diagrams for cache occupancy

All output uses ANSI escape codes for color/formatting.
