#include "tlb.h"

TLB::TLB(size_t capacity) : capacity_(capacity) {}

bool TLB::lookup(int pid, uint64_t vpn, size_t &frame) {
    if (capacity_ == 0) { ++stats_.misses; return false; }
    Key key{pid, vpn};
    auto it = map_.find(key);
    if (it == map_.end()) { ++stats_.misses; return false; }
    frame = it->second->frame;
    lru_.splice(lru_.begin(), lru_, it->second);
    ++stats_.hits;
    return true;
}

void TLB::insert(int pid, uint64_t vpn, size_t frame) {
    if (capacity_ == 0) return;
    Key key{pid, vpn};
    auto it = map_.find(key);
    if (it != map_.end()) {
        it->second->frame = frame;
        lru_.splice(lru_.begin(), lru_, it->second);
        return;
    }
    lru_.push_front({key, frame});
    map_[key] = lru_.begin();
    if (lru_.size() > capacity_) {
        auto last = std::prev(lru_.end());
        map_.erase(last->key);
        lru_.pop_back();
    }
}

void TLB::invalidate(int pid, uint64_t vpn) {
    Key key{pid, vpn};
    auto it = map_.find(key);
    if (it == map_.end()) return;
    lru_.erase(it->second);
    map_.erase(it);
}

void TLB::clear() { lru_.clear(); map_.clear(); }
