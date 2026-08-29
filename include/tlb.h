#pragma once
#include <cstddef>
#include <cstdint>
#include <list>
#include <unordered_map>

struct TLBStats { uint64_t hits = 0; uint64_t misses = 0; };

class TLB {
public:
    explicit TLB(size_t capacity = 32);
    bool lookup(int pid, uint64_t vpn, size_t &frame);
    void insert(int pid, uint64_t vpn, size_t frame);
    void invalidate(int pid, uint64_t vpn);
    void clear();
    TLBStats stats() const { return stats_; }
    size_t capacity() const { return capacity_; }
private:
    struct Key { int pid; uint64_t vpn; bool operator==(const Key& o) const { return pid == o.pid && vpn == o.vpn; } };
    struct Hash { size_t operator()(const Key& k) const { return std::hash<int>{}(k.pid) ^ (std::hash<uint64_t>{}(k.vpn) << 1); } };
    struct Entry { Key key; size_t frame; };
    size_t capacity_;
    std::list<Entry> lru_;
    std::unordered_map<Key, std::list<Entry>::iterator, Hash> map_;
    TLBStats stats_;
};
