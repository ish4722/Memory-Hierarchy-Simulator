#include "vm.h"
#include <cassert>
#include <iostream>
VMManager::VMManager(PhysicalMemory&pm,Allocator&alloc,size_t page_size):pm_(pm),allocator_(alloc),page_size_(page_size){}
size_t VMManager::allocate_frame(){auto opt=allocator_.alloc(page_size_);if(!opt)return SIZE_MAX;return *opt;}
void VMManager::evict_frame(ProcPT&pt){while(!pt.clock_order.empty()){uint64_t vpn=pt.clock_order.front();pt.clock_order.pop_front();bool ref=pt.ref_bits[vpn];if(!ref){size_t base=pt.page_table[vpn];(void)base;pt.page_table.erase(vpn);pt.ref_bits.erase(vpn);stats_.evictions++;return;}pt.ref_bits[vpn]=false;pt.clock_order.push_back(vpn);}}
std::optional<size_t> VMManager::translate(int pid,uint64_t va,bool allocate_on_fault){uint64_t vpn=va/page_size_;size_t offset=va%page_size_;auto&pt=procs_[pid];auto it=pt.page_table.find(vpn);if(it!=pt.page_table.end()){pt.ref_bits[vpn]=true;return it->second+offset;}stats_.page_faults++;if(!allocate_on_fault)return std::nullopt;size_t frame=allocate_frame();if(frame==SIZE_MAX){evict_frame(pt);frame=allocate_frame();if(frame==SIZE_MAX)return std::nullopt;}pt.page_table[vpn]=frame;pt.ref_bits[vpn]=true;pt.clock_order.push_back(vpn);return frame+offset;}
