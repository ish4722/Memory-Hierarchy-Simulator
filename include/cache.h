#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <list>
#include <unordered_map>
#include "physical.h"

struct CacheStats
{
    uint64_t hits = 0;
    uint64_t misses = 0;
};

class Cache
{
public:
    Cache(PhysicalMemory &pm, size_t cache_size = 32 * 1024, size_t block_size = 64, size_t ways = 4);
    bool read(size_t phys_addr, void *dst, size_t len);
    bool write(size_t phys_addr, const void *src, size_t len);
    CacheStats stats() const { return stats_; }

    // For visualization
    struct Line
    {
        size_t tag;
        bool valid;
        std::vector<uint8_t> data;
    };
    size_t cache_size() const { return cache_size_; }
    size_t block_size() const { return block_size_; }
    size_t ways() const { return ways_; }
    size_t num_sets() const { return num_sets_; }
    const std::vector<std::list<Line>> &sets() const { return sets_; }

private:
    PhysicalMemory &pm_;
    size_t cache_size_, block_size_, ways_, num_sets_;
    std::vector<std::list<Line>> sets_;
    CacheStats stats_;
    void fetch_block(size_t block_addr, std::list<Line> &set);
};
