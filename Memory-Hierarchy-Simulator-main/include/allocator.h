#pragma once
#include <cstddef>
#include <vector>
#include <list>
#include <mutex>
#include <optional>

class Allocator
{
public:
    virtual ~Allocator() = default;
    virtual std::optional<size_t> alloc(size_t bytes) = 0; // returns physical offset
    virtual void free(size_t offset, size_t bytes) = 0;
};

// Simple buddy allocator (power-of-two total)
class BuddyAllocator : public Allocator
{
public:
    BuddyAllocator(size_t total_bytes, size_t min_block = 64);
    std::optional<size_t> alloc(size_t bytes) override;
    void free(size_t offset, size_t bytes) override;

    // For visualization
    size_t total_size() const { return total_size_; }
    size_t min_block() const { return min_block_; }
    unsigned max_order() const { return max_order_; }
    const std::vector<std::list<size_t>> &free_lists() const { return free_lists_; }
    size_t block_size(unsigned order) const { return block_size_for_order(order); }

private:
    size_t total_size_;
    size_t min_block_;
    unsigned max_order_;
    std::vector<std::list<size_t>> free_lists_; // offsets per order
    std::mutex mtx_;
    unsigned order_for_size(size_t size) const;
    size_t block_size_for_order(unsigned order) const;
    size_t buddy_of(size_t offset, unsigned order) const;
};
