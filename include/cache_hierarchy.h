#pragma once
#include <cstddef>
#include <cstdint>
#include <list>
#include <vector>
#include "physical.h"

enum class ReplacementPolicy { LRU, FIFO };
enum class WritePolicy { WriteThrough, WriteBack };
enum class AllocationPolicy { WriteAllocate, NoWriteAllocate };

struct CacheLevelConfig { size_t size_bytes; size_t block_size; size_t ways; ReplacementPolicy replacement=ReplacementPolicy::LRU; WritePolicy write_policy=WritePolicy::WriteThrough; AllocationPolicy allocation=AllocationPolicy::WriteAllocate; size_t hit_latency=1; };
struct CacheHierarchyStats { uint64_t accesses=0; uint64_t l1_hits=0,l1_misses=0; uint64_t l2_hits=0,l2_misses=0; uint64_t l3_hits=0,l3_misses=0; uint64_t memory_accesses=0; uint64_t writebacks=0; uint64_t total_latency=0; };

class CacheHierarchy {
public:
    CacheHierarchy(PhysicalMemory &pm, CacheLevelConfig l1={32*1024,64,4,ReplacementPolicy::LRU,WritePolicy::WriteThrough,AllocationPolicy::WriteAllocate,1}, CacheLevelConfig l2={256*1024,64,8,ReplacementPolicy::LRU,WritePolicy::WriteThrough,AllocationPolicy::WriteAllocate,4}, CacheLevelConfig l3={2*1024*1024,64,16,ReplacementPolicy::LRU,WritePolicy::WriteThrough,AllocationPolicy::WriteAllocate,12});
    bool read(size_t addr, void *dst, size_t len);
    bool write(size_t addr, const void *src, size_t len);
    CacheHierarchyStats stats() const { return stats_; }
    const CacheLevelConfig &l1_config() const { return l1_cfg_; }
    const CacheLevelConfig &l2_config() const { return l2_cfg_; }
    const CacheLevelConfig &l3_config() const { return l3_cfg_; }
    void reset_stats();
private:
    struct Line { size_t tag=0; bool valid=false; bool dirty=false; uint64_t seq=0; std::vector<uint8_t> data; };
    class Level {
    public:
        Level(PhysicalMemory &pm, CacheLevelConfig cfg);
        bool read(size_t addr, void *dst, size_t len, size_t &latency);
        bool write_hit(size_t addr, const void *src, size_t len);
        bool contains(size_t addr) const;
        bool insert(size_t addr, const void *data, size_t len, bool dirty, Line *evicted);
        size_t block_addr(size_t addr) const { return (addr/cfg_.block_size)*cfg_.block_size; }
    private:
        PhysicalMemory &pm_; CacheLevelConfig cfg_; size_t sets_count_; uint64_t tick_=0; std::vector<std::list<Line>> sets_;
        size_t set_index(size_t block) const { return (block/cfg_.block_size)%sets_count_; }
        size_t tag(size_t block) const { return (block/cfg_.block_size)/sets_count_; }
        void touch(std::list<Line>::iterator it,std::list<Line>&set);
    };
    PhysicalMemory &pm_; CacheLevelConfig l1_cfg_,l2_cfg_,l3_cfg_; Level l1_,l2_,l3_; CacheHierarchyStats stats_;
    bool read_block(size_t addr,std::vector<uint8_t>&data,size_t&latency,bool count_stats=true);
    bool write_back_line(size_t level,size_t block,const Line&line);
};
