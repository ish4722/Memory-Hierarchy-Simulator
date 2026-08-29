#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include "physical.h"
#include "allocator.h"
#include "vm.h"
#include "cache.h"
#include "tui.h"

using namespace std;
using namespace tui;

void run_demo(BuddyAllocator &alloc, VMManager &vm, Cache &cache, PhysicalMemory &pm)
{
    print_info("Running demo workload...");
    this_thread::sleep_for(chrono::milliseconds(200));

    vector<pair<size_t, size_t>> allocs;
    size_t sizes[] = {64, 128, 256, 512, 1024, 2048};
    for (size_t sz : sizes)
    {
        auto off = alloc.alloc(sz);
        if (off)
        {
            allocs.push_back({*off, sz});
            cout << color::dim << "  alloc(" << sz << ") -> 0x" << hex << *off << dec << color::reset << "\n";
        }
        this_thread::sleep_for(chrono::milliseconds(50));
    }

    print_info("Accessing virtual memory for PID 1 and PID 2...");
    for (int pid = 1; pid <= 2; ++pid)
    {
        for (uint64_t va = 0; va < 0x8000; va += 0x1000)
        {
            auto pa = vm.translate(pid, va);
            if (pa)
            {
                uint8_t val = static_cast<uint8_t>((pid * 100 + va / 0x1000) & 0xFF);
                cache.write(*pa, &val, 1);
            }
            this_thread::sleep_for(chrono::milliseconds(20));
        }
    }

    print_info("Reading back (expect cache hits)...");
    for (int pid = 1; pid <= 2; ++pid)
    {
        for (uint64_t va = 0; va < 0x4000; va += 0x1000)
        {
            auto pa = vm.translate(pid, va);
            if (pa)
            {
                uint8_t val;
                cache.read(*pa, &val, 1);
            }
        }
    }

    print_info("Freeing some blocks...");
    for (size_t i = 0; i < allocs.size(); i += 2)
    {
        alloc.free(allocs[i].first, allocs[i].second);
        cout << color::dim << "  free(0x" << hex << allocs[i].first << ", " << dec << allocs[i].second << ")" << color::reset << "\n";
        this_thread::sleep_for(chrono::milliseconds(50));
    }

    print_success("Demo complete.");
}

int main(int argc, char **argv)
{
    size_t ram_bytes = 1 << 20;
    size_t page_size = 4096;
    size_t min_block = 64;

    PhysicalMemory pm(ram_bytes);
    BuddyAllocator alloc(ram_bytes, min_block);
    VMManager vm(pm, alloc, page_size);
    Cache cache(pm);

    clear_screen();
    print_header();
    cout << color::dim << "RAM: " << format_bytes(ram_bytes)
         << " | Page: " << format_bytes(page_size)
         << " | Cache: " << format_bytes(cache.cache_size()) << color::reset << "\n";
    cout << "Type " << color::cyan << "help" << color::reset << " for commands.\n\n";

    string line;
    while (true)
    {
        cout << color::bold << color::white << "> " << color::reset;
        if (!getline(cin, line))
            break;
        if (line.empty())
            continue;

        istringstream iss(line);
        string cmd;
        iss >> cmd;

        if (cmd == "quit" || cmd == "exit")
        {
            print_info("Goodbye.");
            break;
        }
        else if (cmd == "help")
        {
            show_help();
        }
        else if (cmd == "clear")
        {
            clear_screen();
            print_header();
        }
        else if (cmd == "stats")
        {
            show_all(alloc, vm, cache);
        }
        else if (cmd == "buddy")
        {
            show_buddy_state(alloc);
        }
        else if (cmd == "vm")
        {
            show_vm_state(vm);
        }
        else if (cmd == "cache")
        {
            show_cache_state(cache);
        }
        else if (cmd == "demo")
        {
            run_demo(alloc, vm, cache, pm);
            show_all(alloc, vm, cache);
        }
        else if (cmd == "malloc")
        {
            size_t bytes;
            if (!(iss >> bytes))
            {
                print_error("Usage: malloc <bytes>");
                continue;
            }
            auto off = alloc.alloc(bytes);
            if (off)
            {
                ostringstream oss;
                oss << "Allocated " << format_bytes(bytes) << " at offset 0x" << hex << *off;
                print_success(oss.str());
            }
            else
            {
                print_error("Allocation failed.");
            }
        }
        else if (cmd == "free")
        {
            size_t off, bytes;
            if (!(iss >> off >> bytes))
            {
                print_error("Usage: free <offset> <bytes>");
                continue;
            }
            alloc.free(off, bytes);
            print_success("Freed.");
        }
        else if (cmd == "read")
        {
            int pid;
            uint64_t va;
            if (!(iss >> pid >> hex >> va))
            {
                print_error("Usage: read <pid> <va_hex>");
                continue;
            }
            auto pa = vm.translate(pid, va);
            if (!pa)
            {
                print_error("Translation failed.");
                continue;
            }
            uint8_t val;
            cache.read(*pa, &val, 1);
            ostringstream oss;
            oss << "Read [PID " << pid << " VA 0x" << hex << va << "] -> " << dec << (int)val;
            print_success(oss.str());
        }
        else if (cmd == "write")
        {
            int pid;
            uint64_t va;
            int v;
            if (!(iss >> pid >> hex >> va >> v))
            {
                print_error("Usage: write <pid> <va_hex> <value>");
                continue;
            }
            auto pa = vm.translate(pid, va);
            if (!pa)
            {
                print_error("Translation failed.");
                continue;
            }
            uint8_t val = static_cast<uint8_t>(v);
            cache.write(*pa, &val, 1);
            ostringstream oss;
            oss << "Wrote " << v << " to [PID " << pid << " VA 0x" << hex << va << "]";
            print_success(oss.str());
        }
        else
        {
            print_error("Unknown command. Type 'help'.");
        }
    }
    return 0;
}
