#include "cache.h"
#include <algorithm>
#include <cstring>

Cache::Cache(PhysicalMemory &pm, size_t cache_size, size_t block_size, size_t ways)
    : pm_(pm), cache_size_(cache_size), block_size_(block_size), ways_(ways)
{
    num_sets_ = cache_size_ / (block_size_ * ways_);
    if (num_sets_ == 0)
        num_sets_ = 1;
    sets_.resize(num_sets_);
}

void Cache::fetch_block(size_t block_addr, std::list<Line> &set)
{
    Line line;
    line.tag = block_addr;
    line.valid = true;
    line.data.resize(block_size_);
    pm_.read(block_addr, line.data.data(), block_size_);
    set.push_front(std::move(line));
    if (set.size() > ways_)
        set.pop_back();
}

bool Cache::read(size_t phys_addr, void *dst, size_t len)
{
    size_t block_addr = (phys_addr / block_size_) * block_size_;
    size_t set_idx = (block_addr / block_size_) % num_sets_;
    auto &set = sets_[set_idx];
    for (auto it = set.begin(); it != set.end(); ++it)
    {
        if (it->valid && it->tag == block_addr)
        {
            // hit: move to front (MRU)
            std::vector<uint8_t> tmp;
            tmp.reserve(0);
            auto line = *it;
            set.erase(it);
            set.push_front(line);
            size_t off = phys_addr - block_addr;
            std::memcpy(dst, set.front().data.data() + off, len);
            stats_.hits++;
            return true;
        }
    }
    // miss: fetch
    stats_.misses++;
    fetch_block(block_addr, set);
    size_t off = phys_addr - block_addr;
    std::memcpy(dst, set.front().data.data() + off, len);
    return true;
}

bool Cache::write(size_t phys_addr, const void *src, size_t len)
{
    // simple write-through: write to PM and invalidate/set cache
    pm_.write(phys_addr, src, len);
    // update cache line if present
    size_t block_addr = (phys_addr / block_size_) * block_size_;
    size_t set_idx = (block_addr / block_size_) % num_sets_;
    auto &set = sets_[set_idx];
    for (auto it = set.begin(); it != set.end(); ++it)
    {
        if (it->valid && it->tag == block_addr)
        {
            size_t off = phys_addr - block_addr;
            std::memcpy(it->data.data() + off, src, len);
            // move to front
            auto line = *it;
            set.erase(it);
            set.push_front(line);
            stats_.hits++;
            return true;
        }
    }
    stats_.misses++;
    // fetch block into cache
    fetch_block(block_addr, set);
    size_t off = phys_addr - block_addr;
    std::memcpy(set.front().data.data() + off, src, len);
    return true;
}
