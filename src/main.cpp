#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include "physical.h"
#include "allocator.h"
#include "vm.h"
#include "cache_hierarchy.h"
#include "tui.h"
using namespace std; using namespace tui;

void run_demo(BuddyAllocator &alloc, VMManager &vm, CacheHierarchy &cache) {
    print_info("Running memory hierarchy demo...");
    vector<pair<size_t,size_t>> allocs;
    for(size_t sz: {64UL,128UL,256UL,512UL,1024UL,2048UL}) { auto off=alloc.alloc(sz); if(off) allocs.push_back({*off,sz}); }
    for(int pid=1;pid<=2;++pid) for(uint64_t va=0;va<0x8000;va+=0x1000){ auto pa=vm.translate(pid,va); if(pa){uint8_t v=(uint8_t)((pid*100+va/0x1000)&0xff);cache.write(*pa,&v,1);} }
    print_info("Reading the same pages again to demonstrate TLB/cache locality...");
    for(int pid=1;pid<=2;++pid) for(uint64_t va=0;va<0x4000;va+=0x1000){auto pa=vm.translate(pid,va);if(pa){uint8_t v;cache.read(*pa,&v,1);}}
    for(size_t i=0;i<allocs.size();i+=2) alloc.free(allocs[i].first,allocs[i].second);
    print_success("Demo complete.");
}

int main(){
    const size_t ram_bytes=1<<20, page_size=4096, min_block=64;
    PhysicalMemory pm(ram_bytes);
    BuddyAllocator alloc(ram_bytes,min_block);
    VMManager vm(pm,alloc,page_size,32);
    CacheLevelConfig l1{32*1024,64,4,ReplacementPolicy::LRU,WritePolicy::WriteThrough,AllocationPolicy::WriteAllocate,1};
    CacheLevelConfig l2{256*1024,64,8,ReplacementPolicy::LRU,WritePolicy::WriteThrough,AllocationPolicy::WriteAllocate,4};
    CacheLevelConfig l3{2*1024*1024,64,16,ReplacementPolicy::LRU,WritePolicy::WriteBack,AllocationPolicy::WriteAllocate,12};
    CacheHierarchy cache(pm,l1,l2,l3);
    clear_screen(); print_header();
    cout<<color::dim<<"RAM: "<<format_bytes(ram_bytes)<<" | Page: "<<format_bytes(page_size)<<" | TLB: "<<vm.tlb().capacity()<<" entries | L1/L2/L3 enabled"<<color::reset<<"\n";
    cout<<"Type "<<color::cyan<<"help"<<color::reset<<" for commands.\n\n";
    string line;
    while(getline(cin,line)){
        if(line.empty())continue; istringstream iss(line); string cmd; iss>>cmd;
        if(cmd=="quit"||cmd=="exit"){print_info("Goodbye.");break;}
        else if(cmd=="help")show_help();
        else if(cmd=="clear"){clear_screen();print_header();}
        else if(cmd=="stats"){show_buddy_state(alloc);show_vm_state(vm);show_hierarchy_state(cache);}
        else if(cmd=="buddy")show_buddy_state(alloc);
        else if(cmd=="vm")show_vm_state(vm);
        else if(cmd=="cache"||cmd=="hierarchy")show_hierarchy_state(cache);
        else if(cmd=="demo"){run_demo(alloc,vm,cache);show_buddy_state(alloc);show_vm_state(vm);show_hierarchy_state(cache);}
        else if(cmd=="malloc"){size_t bytes;if(!(iss>>bytes)){print_error("Usage: malloc <bytes>");continue;}auto off=alloc.alloc(bytes);if(off){ostringstream o;o<<"Allocated "<<format_bytes(bytes)<<" at offset 0x"<<hex<<*off;print_success(o.str());}else print_error("Allocation failed.");}
        else if(cmd=="free"){size_t off,bytes;if(!(iss>>off>>bytes)){print_error("Usage: free <offset> <bytes>");continue;}alloc.free(off,bytes);print_success("Freed.");}
        else if(cmd=="read"){int pid;uint64_t va;if(!(iss>>pid>>hex>>va)){print_error("Usage: read <pid> <va_hex>");continue;}auto pa=vm.translate(pid,va);if(!pa){print_error("Translation failed.");continue;}uint8_t val;if(cache.read(*pa,&val,1)){ostringstream o;o<<"Read [PID "<<pid<<" VA 0x"<<hex<<va<<"] -> "<<dec<<(int)val;print_success(o.str());}}
        else if(cmd=="write"){int pid;uint64_t va;int v;if(!(iss>>pid>>hex>>va>>v)){print_error("Usage: write <pid> <va_hex> <value>");continue;}auto pa=vm.translate(pid,va);if(!pa){print_error("Translation failed.");continue;}uint8_t val=(uint8_t)v;if(cache.write(*pa,&val,1)){ostringstream o;o<<"Wrote "<<v<<" to [PID "<<pid<<" VA 0x"<<hex<<va<<"]";print_success(o.str());}}
        else print_error("Unknown command. Type 'help'.");
    }
    return 0;
}
