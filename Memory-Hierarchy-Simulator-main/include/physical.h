#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <optional>

class PhysicalMemory
{
public:
    explicit PhysicalMemory(size_t bytes = 1 << 20); // default 1MiB
    size_t size() const;
    bool read(size_t phys_addr, void *dst, size_t len) const;
    bool write(size_t phys_addr, const void *src, size_t len);
    const uint8_t *data() const;

private:
    std::vector<uint8_t> mem_;
};
