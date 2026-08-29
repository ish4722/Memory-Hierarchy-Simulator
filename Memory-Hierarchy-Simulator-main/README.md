# Memory Hierarchy Simulator

A modular C++17 simulator demonstrating:

- **Physical Memory**: Contiguous `std::vector<uint8_t>` model
- **Buddy Allocator**: Power-of-two allocation with coalescing
- **Virtual Memory**: Per-process page tables, CLOCK replacement
- **Cache**: Set-associative LRU with write-through
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
|---------|-------------|
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

## Example

```
> malloc 4096
Allocated 4 KiB at offset 0x0

> write 1 0x1000 42
Wrote 42 to [PID 1 VA 0x1000]

> read 1 0x1000
Read [PID 1 VA 0x1000] -> 42

> stats
[Buddy Allocator State]
  Total: 1 MiB  Min Block: 64 B
        Used [██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░] 0.8%
...
```

## Structure

```
include/     Headers (physical.h, allocator.h, vm.h, cache.h, tui.h)
src/         Implementation files
tests/       Example workloads
docs/        Design notes
```

See [docs/DESIGN.md](docs/DESIGN.md) for architecture details.