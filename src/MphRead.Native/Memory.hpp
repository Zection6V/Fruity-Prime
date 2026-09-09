#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace fruityprime::memory {

struct AddressInfo {
    std::string name;
    std::size_t offset = 0;
    std::size_t size = 0;
};

// Portable byte-addressable memory used by native format probes and future
// emulated DS/GBA state. It deliberately does not expose raw host pointers,
// which keeps invalid cartridge addresses bounds checked.
class Buffer {
public:
    explicit Buffer(std::size_t size = 0);
    explicit Buffer(std::vector<std::uint8_t> bytes);

    [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }
    [[nodiscard]] bool contains(std::size_t offset,
                                std::size_t count = 1) const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept;
    [[nodiscard]] std::span<std::uint8_t> writable_bytes() noexcept;
    void resize(std::size_t size);
    void fill(std::uint8_t value) noexcept;

    [[nodiscard]] std::uint8_t read_u8(std::size_t offset) const;
    [[nodiscard]] std::uint16_t read_u16_le(std::size_t offset) const;
    [[nodiscard]] std::int16_t read_i16_le(std::size_t offset) const;
    [[nodiscard]] std::uint32_t read_u32_le(std::size_t offset) const;
    [[nodiscard]] std::int32_t read_i32_le(std::size_t offset) const;
    [[nodiscard]] float read_f32_le(std::size_t offset) const;

    void write_u8(std::size_t offset, std::uint8_t value);
    void write_u16_le(std::size_t offset, std::uint16_t value);
    void write_u32_le(std::size_t offset, std::uint32_t value);
    void write_f32_le(std::size_t offset, float value);

private:
    void require(std::size_t offset, std::size_t count) const;
    std::vector<std::uint8_t> bytes_;
};

} // namespace fruityprime::memory

namespace MphReadNative {
namespace Memory = ::fruityprime::memory;
using MemoryBuffer = ::fruityprime::memory::Buffer;
using AddressInfo = ::fruityprime::memory::AddressInfo;
}
