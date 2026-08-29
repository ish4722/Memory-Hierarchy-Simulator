#include "physical.h"
#include <algorithm>
#include <cstring>

PhysicalMemory::PhysicalMemory(size_t bytes) : mem_(bytes, 0) {}

size_t PhysicalMemory::size() const { return mem_.size(); }

bool PhysicalMemory::read(size_t phys_addr, void *dst, size_t len) const
{
    if (phys_addr + len > mem_.size())
        return false;
    std::memcpy(dst, mem_.data() + phys_addr, len);
    return true;
}

bool PhysicalMemory::write(size_t phys_addr, const void *src, size_t len)
{
    if (phys_addr + len > mem_.size())
        return false;
    std::memcpy(mem_.data() + phys_addr, src, len);
    return true;
}

const uint8_t *PhysicalMemory::data() const { return mem_.data(); }
