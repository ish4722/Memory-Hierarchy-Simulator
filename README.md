# Memory Hierarchy Simulator

A modular C++17 simulator demonstrating physical memory, buddy allocation, virtual memory, page replacement, and a set-associative cache. This repository is a derivative work based on the public `beingamanforever/Memory-Hierarchy-Simulator` project.

## Features

- **Physical Memory**: Contiguous `std::vector<uint8_t>` model
- **Buddy Allocator**: Power-of-two allocation with coalescing
- **Virtual Memory**: Per-process page tables with CLOCK replacement
- **Cache**: Set-associative LRU cache with write-through
- **Integrated Pipeline**: VA -> PA translation -> Cache -> RAM

## Build

```sh
make
```

## Run

```sh
./sim
```

## Commands

| Command | Description |
|---|---|
| `malloc <bytes>` | Allocate memory |
| `free <offset> <bytes>` | Free memory |
| `read <pid> <va>` | Read from virtual address (hex) |
| `write <pid> <va> <val>` | Write to virtual address (hex) |
| `buddy` | Show buddy allocator state |
| `vm` | Show VM page tables |
| `cache` | Show cache statistics |
| `stats` | Show all subsystems |
| `demo` | Run demo workload |
| `clear` | Clear screen |
| `help` | Show commands |
| `exit` | Quit |

## Structure

```text
include/     Headers
src/         Implementation files
tests/       Example workloads
docs/        Design notes
```

## Original project

https://github.com/beingamanforever/Memory-Hierarchy-Simulator
