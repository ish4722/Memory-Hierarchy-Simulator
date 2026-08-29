#include "cache_hierarchy.h"
#include <algorithm>
#include <cstring>

CacheHierarchy::Level::Level(PhysicalMemory &pm, CacheLevelConfig cfg) : pm_(pm), cfg_(cfg) {
    sets_count_ = std::max<size_t>(1, cfg_.size_bytes / (cfg_.block_size * cfg_.ways));
    sets_.resize(sets_count_);
}

void CacheHierarchy::Level::touch(std::list<Line>::iterator it, std::list<Line> &set) {
    if (cfg_.replacement == ReplacementPolicy::LRU) set.splice(set.begin(), set, it);
}

bool CacheHierarchy::Level::read(size_t addr, void *dst, size_t len, size_t &latency) {
    latency = cfg_.hit_latency;
    const size_t block = block_addr(addr), idx = set_index(block), t = tag(block), off = addr - block;
    auto &set = sets_[idx];
    for (auto it=set.begin(); it!=set.end(); ++it) {
        if (it->valid && it->tag==t) {
            if (off + len > it->data.size()) return false;
            std::memcpy(dst, it->data.data()+off, len);
            touch(it,set);
            return true;
        }
    }
    return false;
}

bool CacheHierarchy::Level::contains(size_t addr) const {
    const size_t block=block_addr(addr), idx=set_index(block), t=tag(block);
    for(const auto &line:sets_[idx]) if(line.valid && line.tag==t) return true;
    return false;
}

bool CacheHierarchy::Level::write_hit(size_t addr, const void *src, size_t len) {
    const size_t block=block_addr(addr), idx=set_index(block), t=tag(block), off=addr-block;
    auto &set=sets_[idx];
    for(auto it=set.begin();it!=set.end();++it) if(it->valid && it->tag==t){
        if(off+len>it->data.size()) return false;
        std::memcpy(it->data.data()+off,src,len);
        if(cfg_.write_policy==WritePolicy::WriteBack) it->dirty=true;
        touch(it,set); return true;
    }
    return false;
}

bool CacheHierarchy::Level::insert(size_t addr,const void *data,size_t len,bool dirty,Line *evicted) {
    const size_t block=block_addr(addr), idx=set_index(block), t=tag(block); auto &set=sets_[idx];
    for(auto it=set.begin();it!=set.end();++it) if(it->valid && it->tag==t){
        std::memcpy(it->data.data(),data,std::min(len,it->data.size())); it->dirty=dirty; touch(it,set); return true;
    }
    if(set.size()>=cfg_.ways){
        auto it=(cfg_.replacement==ReplacementPolicy::LRU) ? std::prev(set.end()) : std::min_element(set.begin(),set.end(),[](const Line&a,const Line&b){return a.seq<b.seq;});
        if(evicted) *evicted=*it; set.erase(it);
    }
    Line line; line.tag=t; line.valid=true; line.dirty=dirty; line.seq=tick_++; line.data.resize(cfg_.block_size,0);
    std::memcpy(line.data.data(),data,std::min(len,line.data.size())); set.push_front(std::move(line)); return true;
}

CacheHierarchy::CacheHierarchy(PhysicalMemory &pm,CacheLevelConfig l1,CacheLevelConfig l2,CacheLevelConfig l3)
    : pm_(pm),l1_cfg_(l1),l2_cfg_(l2),l3_cfg_(l3),l1_(pm,l1_cfg_),l2_(pm,l2_cfg_),l3_(pm,l3_cfg_) {}

bool CacheHierarchy::write_back_line(size_t, size_t block, const Line &line) {
    if(!line.dirty) return true;
    if(!pm_.write(block,line.data.data(),line.data.size())) return false;
    ++stats_.writebacks; ++stats_.memory_accesses; return true;
}

bool CacheHierarchy::read_block(size_t addr,std::vector<uint8_t>&data,size_t&latency,bool count_stats) {
    const size_t block=l1_.block_addr(addr); latency=0; data.resize(l1_cfg_.block_size); size_t l=0;
    if(l1_.read(block,data.data(),data.size(),l)){if(count_stats)++stats_.l1_hits;latency+=l;return true;}
    if(count_stats)++stats_.l1_misses; latency+=l1_cfg_.hit_latency;

    data.resize(l2_cfg_.block_size);
    if(l2_.read(block,data.data(),std::min(l2_cfg_.block_size,l1_cfg_.block_size),l)){
        if(count_stats)++stats_.l2_hits; latency+=l; data.resize(l1_cfg_.block_size);
        Line e; l1_.insert(block,data.data(),data.size(),false,&e); if(e.valid&&e.dirty)write_back_line(1,block,e); return true;
    }
    if(count_stats)++stats_.l2_misses; latency+=l2_cfg_.hit_latency;

    data.resize(l3_cfg_.block_size);
    if(l3_.read(block,data.data(),std::min(l3_cfg_.block_size,l1_cfg_.block_size),l)){
        if(count_stats)++stats_.l3_hits; latency+=l; data.resize(l1_cfg_.block_size);
        Line e2; l2_.insert(block,data.data(),data.size(),false,&e2); if(e2.valid&&e2.dirty)write_back_line(2,block,e2);
        Line e1; l1_.insert(block,data.data(),data.size(),false,&e1); if(e1.valid&&e1.dirty)write_back_line(1,block,e1); return true;
    }
    if(count_stats)++stats_.l3_misses; latency+=l3_cfg_.hit_latency;

    data.resize(l1_cfg_.block_size);
    if(!pm_.read(block,data.data(),data.size())) return false;
    if(count_stats)++stats_.memory_accesses; latency+=100;
    Line e3; l3_.insert(block,data.data(),data.size(),false,&e3); if(e3.valid&&e3.dirty)write_back_line(3,block,e3);
    Line e2; l2_.insert(block,data.data(),data.size(),false,&e2); if(e2.valid&&e2.dirty)write_back_line(2,block,e2);
    Line e1; l1_.insert(block,data.data(),data.size(),false,&e1); if(e1.valid&&e1.dirty)write_back_line(1,block,e1);
    return true;
}

bool CacheHierarchy::read(size_t addr,void *dst,size_t len){
    ++stats_.accesses; std::vector<uint8_t> data; size_t latency=0;
    if(!read_block(addr,data,latency,true))return false; const size_t off=addr-l1_.block_addr(addr); if(off+len>data.size())return false;
    std::memcpy(dst,data.data()+off,len); stats_.total_latency+=latency; return true;
}

bool CacheHierarchy::write(size_t addr,const void *src,size_t len){
    ++stats_.accesses; size_t latency=0;
    if(l1_.contains(addr)){
        ++stats_.l1_hits; l1_.write_hit(addr,src,len); latency+=l1_cfg_.hit_latency;
        if(l1_cfg_.write_policy==WritePolicy::WriteThrough){ l2_.write_hit(addr,src,len); l3_.write_hit(addr,src,len); if(!pm_.write(addr,src,len))return false; ++stats_.memory_accesses; latency+=100; }
        stats_.total_latency+=latency; return true;
    }
    ++stats_.l1_misses; latency+=l1_cfg_.hit_latency;
    if(l1_cfg_.allocation==AllocationPolicy::NoWriteAllocate){ if(!pm_.write(addr,src,len))return false; ++stats_.memory_accesses; stats_.total_latency+=latency+100; return true; }

    std::vector<uint8_t> block; size_t lower_latency=0;
    if(!read_block(addr,block,lower_latency,true))return false;
    latency+=lower_latency;
    Line e; l1_.insert(addr,block.data(),block.size(),l1_cfg_.write_policy==WritePolicy::WriteBack,&e); if(e.valid&&e.dirty)write_back_line(1,l1_.block_addr(addr),e);
    l1_.write_hit(addr,src,len);
    if(l1_cfg_.write_policy==WritePolicy::WriteThrough){ l2_.write_hit(addr,src,len); l3_.write_hit(addr,src,len); if(!pm_.write(addr,src,len))return false; ++stats_.memory_accesses; latency+=100; }
    stats_.total_latency+=latency; return true;
}

void CacheHierarchy::reset_stats(){stats_=CacheHierarchyStats{};}
