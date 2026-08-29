#pragma once
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include "allocator.h"
#include "vm.h"
#include "cache.h"
#include "cache_hierarchy.h"

namespace tui {
namespace color { constexpr const char *reset="\033[0m"; constexpr const char *bold="\033[1m"; constexpr const char *dim="\033[2m"; constexpr const char *red="\033[31m"; constexpr const char *green="\033[32m"; constexpr const char *yellow="\033[33m"; constexpr const char *blue="\033[34m"; constexpr const char *magenta="\033[35m"; constexpr const char *cyan="\033[36m"; constexpr const char *white="\033[37m"; }
namespace box { constexpr const char *tl="\u250C"; constexpr const char *tr="\u2510"; constexpr const char *bl="\u2514"; constexpr const char *br="\u2518"; constexpr const char *h="\u2500"; constexpr const char *v="\u2502"; }
inline std::string hrule(int width){std::string s;for(int i=0;i<width;++i)s+=box::h;return s;}
inline std::string format_bytes(size_t bytes){if(bytes>=1024*1024)return std::to_string(bytes/(1024*1024))+" MiB";if(bytes>=1024)return std::to_string(bytes/1024)+" KiB";return std::to_string(bytes)+" B";}
inline void clear_screen(){std::cout<<"\033[2J\033[H";}
inline void print_header(){std::cout<<color::bold<<color::cyan<<box::tl<<hrule(58)<<box::tr<<"\n"<<box::v<<"  Memory Hierarchy Simulator                              "<<box::v<<"\n"<<box::v<<"  TLB | VM | L1 | L2 | L3 | Buddy Allocator               "<<box::v<<"\n"<<box::bl<<hrule(58)<<box::br<<color::reset<<"\n\n";}
inline void print_bar(const std::string&label,double ratio,int width=40,const char*fill=color::green,const char*empty=color::dim){ratio=std::max(0.0,std::min(1.0,ratio));int filled=(int)(ratio*width);std::cout<<std::setw(14)<<label<<" ["<<fill;for(int i=0;i<filled;++i)std::cout<<"\u2588";std::cout<<empty;for(int i=filled;i<width;++i)std::cout<<"\u2591";std::cout<<color::reset<<"] "<<std::fixed<<std::setprecision(1)<<ratio*100<<"%\n";}
inline void show_buddy_state(const BuddyAllocator&alloc){std::cout<<color::bold<<color::yellow<<"\n[Buddy Allocator State]"<<color::reset<<"\n";std::cout<<"  Total: "<<format_bytes(alloc.total_size())<<"  Min Block: "<<format_bytes(alloc.min_block())<<"\n";size_t free=0;const auto&lists=alloc.free_lists();for(unsigned o=0;o<=alloc.max_order();++o)free+=lists[o].size()*alloc.block_size(o);print_bar("Used",1.0-(double)free/alloc.total_size(),40,color::red,color::dim);}
inline void show_vm_state(const VMManager&vm){std::cout<<color::bold<<color::magenta<<"\n[Virtual Memory State]"<<color::reset<<"\n";auto s=vm.stats();auto t=vm.tlb_stats();std::cout<<"  Page Size: "<<format_bytes(vm.page_size())<<"  Page Faults: "<<s.page_faults<<"  Evictions: "<<s.evictions<<"\n";uint64_t ta=t.hits+t.misses;std::cout<<"  TLB Entries: "<<vm.tlb().capacity()<<"  Hits: "<<t.hits<<"  Misses: "<<t.misses<<"\n";if(ta)print_bar("TLB Hit Rate",(double)t.hits/ta);for(const auto&[pid,pt]:vm.processes())std::cout<<"  PID "<<pid<<": "<<pt.page_table.size()<<" mapped pages\n";}
inline void show_cache_state(const Cache&cache){std::cout<<color::bold<<color::blue<<"\n[Legacy Single Cache]"<<color::reset<<"\n";auto s=cache.stats();uint64_t n=s.hits+s.misses;std::cout<<"  Size: "<<format_bytes(cache.cache_size())<<"  Block: "<<format_bytes(cache.block_size())<<"  Ways: "<<cache.ways()<<"\n";std::cout<<"  Hits: "<<s.hits<<"  Misses: "<<s.misses<<"\n";if(n)print_bar("Hit Rate",(double)s.hits/n);}
inline void show_hierarchy_state(const CacheHierarchy&h){auto s=h.stats();std::cout<<color::bold<<color::blue<<"\n[Cache Hierarchy]"<<color::reset<<"\n";auto show=[&](const char*n,uint64_t hit,uint64_t miss,const CacheLevelConfig&c){uint64_t total=hit+miss;std::cout<<"  "<<n<<": "<<format_bytes(c.size_bytes)<<", "<<c.ways<<"-way, "<<(c.replacement==ReplacementPolicy::LRU?"LRU":"FIFO")<<", "<<(c.write_policy==WritePolicy::WriteBack?"write-back":"write-through")<<"\n    Hits="<<hit<<" Misses="<<miss; if(total)std::cout<<" HitRate="<<std::fixed<<std::setprecision(1)<<(100.0*hit/total)<<"%";std::cout<<"\n";};show("L1",s.l1_hits,s.l1_misses,h.l1_config());show("L2",s.l2_hits,s.l2_misses,h.l2_config());show("L3",s.l3_hits,s.l3_misses,h.l3_config());std::cout<<"  Memory accesses: "<<s.memory_accesses<<"  Writebacks: "<<s.writebacks<<"  Total modeled latency: "<<s.total_latency<<"\n";}
inline void show_all(const BuddyAllocator&a,const VMManager&v,const Cache&c,const CacheHierarchy&h){show_buddy_state(a);show_vm_state(v);show_cache_state(c);show_hierarchy_state(h);}
inline void show_help(){std::cout<<color::bold<<"\nCommands:"<<color::reset<<"\n  malloc <bytes> / free <offset> <bytes>\n  read <pid> <va_hex> / write <pid> <va_hex> <value>\n  stats       Show allocator, VM, TLB and cache hierarchy\n  buddy       Show buddy allocator state\n  vm          Show VM and TLB state\n  cache       Show legacy cache state\n  hierarchy   Show L1/L2/L3 statistics\n  demo        Run demo workload\n  clear       Clear screen\n  help        Show help\n  exit        Quit\n";}
inline void print_success(const std::string&msg){std::cout<<color::green<<"\u2713 "<<color::reset<<msg<<"\n";}
inline void print_error(const std::string&msg){std::cout<<color::red<<"\u2717 "<<color::reset<<msg<<"\n";}
inline void print_info(const std::string&msg){std::cout<<color::cyan<<"\u25B6 "<<color::reset<<msg<<"\n";}
}
