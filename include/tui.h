#pragma once
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>
#include "allocator.h"
#include "vm.h"
#include "cache.h"

namespace tui
{
    namespace color
    {
        constexpr const char *reset = "\033[0m";
        constexpr const char *bold = "\033[1m";
        constexpr const char *dim = "\033[2m";
        constexpr const char *red = "\033[31m";
        constexpr const char *green = "\033[32m";
        constexpr const char *yellow = "\033[33m";
        constexpr const char *blue = "\033[34m";
        constexpr const char *magenta = "\033[35m";
        constexpr const char *cyan = "\033[36m";
        constexpr const char *white = "\033[37m";
        constexpr const char *bg_red = "\033[41m";
        constexpr const char *bg_green = "\033[42m";
        constexpr const char *bg_blue = "\033[44m";
    }
    namespace box
    {
        constexpr const char *tl = "\u250C"; constexpr const char *tr = "\u2510";
        constexpr const char *bl = "\u2514"; constexpr const char *br = "\u2518";
        constexpr const char *h = "\u2500"; constexpr const char *v = "\u2502";
        constexpr const char *lj = "\u251C"; constexpr const char *rj = "\u2524";
        constexpr const char *tj = "\u252C"; constexpr const char *bj = "\u2534";
        constexpr const char *cr = "\u253C";
    }
    inline std::string hrule(int width) { std::string s; for (int i=0;i<width;++i) s += box::h; return s; }
    inline std::string format_bytes(size_t bytes)
    {
        if (bytes >= 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MiB";
        if (bytes >= 1024) return std::to_string(bytes / 1024) + " KiB";
        return std::to_string(bytes) + " B";
    }
    inline void clear_screen() { std::cout << "\033[2J\033[H"; }
    inline void print_header()
    {
        std::cout << color::bold << color::cyan << box::tl << hrule(58) << box::tr << "\n";
        std::cout << box::v << "  Memory Hierarchy Simulator                              " << box::v << "\n";
        std::cout << box::v << "  Physical Memory | Buddy Allocator | VM | Cache          " << box::v << "\n";
        std::cout << box::bl << hrule(58) << box::br << color::reset << "\n\n";
    }
    inline void print_bar(const std::string &label, double ratio, int width = 40,
                          const char *fill_color = color::green, const char *empty_color = color::dim)
    {
        int filled = static_cast<int>(ratio * width);
        std::cout << std::setw(12) << label << " [" << fill_color;
        for (int i=0;i<filled;++i) std::cout << "\u2588";
        std::cout << empty_color;
        for (int i=filled;i<width;++i) std::cout << "\u2591";
        std::cout << color::reset << "] " << std::fixed << std::setprecision(1) << (ratio * 100) << "%\n";
    }
    inline void show_buddy_state(const BuddyAllocator &alloc)
    {
        std::cout << color::bold << color::yellow << "\n[Buddy Allocator State]" << color::reset << "\n";
        std::cout << "  Total: " << format_bytes(alloc.total_size()) << "  Min Block: " << format_bytes(alloc.min_block()) << "\n";
        size_t total_free=0; const auto &lists=alloc.free_lists();
        for(unsigned o=0;o<=alloc.max_order();++o) total_free += lists[o].size()*alloc.block_size(o);
        print_bar("Used", 1.0-static_cast<double>(total_free)/alloc.total_size(), 40, color::red, color::dim);
        std::cout << "\n  " << color::dim << "Order" << color::reset << "  " << color::dim << "Block Size" << color::reset << "  " << color::dim << "Free Blocks" << color::reset << "\n  " << hrule(36) << "\n";
        for(unsigned o=0;o<=alloc.max_order();++o) {
            size_t bsz=alloc.block_size(o), count=lists[o].size();
            std::cout << "  " << std::setw(5) << o << "  " << std::setw(10) << format_bytes(bsz) << "  ";
            if(count) { std::cout << color::green << std::setw(4) << count << color::reset << " "; for(size_t i=0;i<std::min(count,(size_t)10);++i) std::cout << color::green << "\u25A0" << color::reset; if(count>10) std::cout << color::dim << "..." << color::reset; }
            else std::cout << color::dim << "   0" << color::reset;
            std::cout << "\n";
        }
    }
    inline void show_vm_state(const VMManager &vm)
    {
        std::cout << color::bold << color::magenta << "\n[Virtual Memory State]" << color::reset << "\n";
        std::cout << "  Page Size: " << format_bytes(vm.page_size()) << "\n";
        auto vs=vm.stats(); std::cout << "  Page Faults: " << color::yellow << vs.page_faults << color::reset << "  Evictions: " << color::red << vs.evictions << color::reset << "\n";
        const auto &procs=vm.processes(); if(procs.empty()){std::cout<<color::dim<<"  No processes mapped yet.\n"<<color::reset;return;}
        for(const auto &[pid,pt]:procs){std::cout<<"\n  "<<color::cyan<<"PID "<<pid<<color::reset<<" ("<<pt.page_table.size()<<" pages)\n"; if(pt.page_table.size()<=8){std::cout<<"    "<<color::dim<<"VPN"<<color::reset<<"      "<<color::dim<<"Frame"<<color::reset<<"   "<<color::dim<<"Ref"<<color::reset<<"\n"; for(const auto &[vpn,frame]:pt.page_table){bool ref=pt.ref_bits.count(vpn)?pt.ref_bits.at(vpn):false;std::cout<<"    0x"<<std::hex<<std::setw(4)<<std::setfill('0')<<vpn<<"   0x"<<std::setw(6)<<frame<<std::dec<<std::setfill(' ')<<"   "<<(ref?color::green:color::dim)<<(ref?"Y":"N")<<color::reset<<"\n";}} else {std::cout<<"    ";int col=0;for(const auto &[vpn,frame]:pt.page_table){bool ref=pt.ref_bits.count(vpn)?pt.ref_bits.at(vpn):false;std::cout<<(ref?color::green:color::dim)<<"\u25A0"<<color::reset;if(++col>=32){std::cout<<"\n    ";col=0;}}std::cout<<"\n";}}
    }
    inline void show_cache_state(const Cache &cache)
    {
        std::cout<<color::bold<<color::blue<<"\n[Cache State]"<<color::reset<<"\n";
        std::cout<<"  Size: "<<format_bytes(cache.cache_size())<<"  Block: "<<format_bytes(cache.block_size())<<"  Ways: "<<cache.ways()<<"  Sets: "<<cache.num_sets()<<"\n";
        auto cs=cache.stats();uint64_t total=cs.hits+cs.misses;double hr=total?static_cast<double>(cs.hits)/total:0.0;std::cout<<"  Hits: "<<color::green<<cs.hits<<color::reset<<"  Misses: "<<color::red<<cs.misses<<color::reset<<"\n";print_bar("Hit Rate",hr,40,color::green,color::red);
        const auto &sets=cache.sets();size_t valid=0;for(const auto &set:sets)for(const auto &line:set)if(line.valid)++valid;size_t max_lines=cache.num_sets()*cache.ways();print_bar("Occupancy",max_lines?static_cast<double>(valid)/max_lines:0.0,40,color::cyan,color::dim);
    }
    inline void show_all(const BuddyAllocator &alloc,const VMManager &vm,const Cache &cache){show_buddy_state(alloc);show_vm_state(vm);show_cache_state(cache);}
    inline void show_help(){std::cout<<color::bold<<"\nCommands:"<<color::reset<<"\n"<<color::cyan<<"  malloc <bytes>"<<color::reset<<"           Allocate memory\n"<<color::cyan<<"  free <offset> <bytes>"<<color::reset<<"    Free memory\n"<<color::cyan<<"  read <pid> <va>"<<color::reset<<"          Read from virtual address (hex)\n"<<color::cyan<<"  write <pid> <va> <val>"<<color::reset<<"   Write to virtual address (hex)\n"<<color::cyan<<"  stats"<<color::reset<<"                    Show all statistics\n"<<color::cyan<<"  buddy"<<color::reset<<"                    Show buddy allocator state\n"<<color::cyan<<"  vm"<<color::reset<<"                       Show VM state\n"<<color::cyan<<"  cache"<<color::reset<<"                    Show cache state\n"<<color::cyan<<"  demo"<<color::reset<<"                     Run demo workload\n"<<color::cyan<<"  clear"<<color::reset<<"                    Clear screen\n"<<color::cyan<<"  help"<<color::reset<<"                     Show this help\n"<<color::cyan<<"  exit"<<color::reset<<"                     Quit\n";}
    inline void print_success(const std::string &msg){std::cout<<color::green<<"\u2713 "<<color::reset<<msg<<"\n";}
    inline void print_error(const std::string &msg){std::cout<<color::red<<"\u2717 "<<color::reset<<msg<<"\n";}
    inline void print_info(const std::string &msg){std::cout<<color::cyan<<"\u25B6 "<<color::reset<<msg<<"\n";}
}
