#include "allocator.h"
#include <cmath>
#include <cassert>

static unsigned ceil_log2(size_t v)
{
    unsigned r = 0;
    size_t x = 1;
    while (x < v)
    {
        x <<= 1;
        ++r;
    }
    return r;
}

BuddyAllocator::BuddyAllocator(size_t total_bytes, size_t min_block)
    : total_size_(total_bytes), min_block_(min_block)
{
    assert((min_block_ & (min_block_ - 1)) == 0);
    assert((total_size_ & (total_size_ - 1)) == 0);
    max_order_ = ceil_log2(total_size_ / min_block_);
    free_lists_.resize(max_order_ + 1);
    // initial free block is full memory at max_order_
    free_lists_[max_order_].push_back(0);
}

unsigned BuddyAllocator::order_for_size(size_t size) const
{
    if (size <= min_block_)
        return 0;
    size_t blocks = (size + min_block_ - 1) / min_block_;
    return ceil_log2(blocks);
}

size_t BuddyAllocator::block_size_for_order(unsigned order) const
{
    return min_block_ << order;
}

size_t BuddyAllocator::buddy_of(size_t offset, unsigned order) const
{
    size_t bsz = block_size_for_order(order);
    return offset ^ bsz;
}

std::optional<size_t> BuddyAllocator::alloc(size_t bytes)
{
    std::lock_guard<std::mutex> lk(mtx_);
    unsigned order = order_for_size(bytes);
    for (unsigned o = order; o <= max_order_; ++o)
    {
        if (!free_lists_[o].empty())
        {
            size_t off = free_lists_[o].front();
            free_lists_[o].pop_front();
            // split down to desired order
            for (unsigned cur = o; cur > order; --cur)
            {
                size_t half = block_size_for_order(cur - 1);
                size_t buddy = off + half;
                free_lists_[cur - 1].push_back(buddy);
            }
            return off;
        }
    }
    return std::nullopt;
}

void BuddyAllocator::free(size_t offset, size_t bytes)
{
    std::lock_guard<std::mutex> lk(mtx_);
    unsigned order = order_for_size(bytes);
    size_t off = offset;
    while (order < max_order_)
    {
        size_t bud = buddy_of(off, order);
        auto &list = free_lists_[order];
        auto it = std::find(list.begin(), list.end(), bud);
        if (it == list.end())
            break; // buddy not free
        list.erase(it);
        off = std::min(off, bud);
        ++order;
    }
    free_lists_[order].push_back(off);
}
