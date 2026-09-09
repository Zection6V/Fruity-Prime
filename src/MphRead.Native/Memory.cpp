#include "Memory.hpp"

#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace fruityprime::memory {

Buffer::Buffer(std::size_t size) : bytes_(size, 0) {}

Buffer::Buffer(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}

bool Buffer::contains(std::size_t offset, std::size_t count) const noexcept {
    return offset <= bytes_.size() && count <= bytes_.size() - offset;
}

std::span<const std::uint8_t> Buffer::bytes() const noexcept {
    return bytes_;
}

std::span<std::uint8_t> Buffer::writable_bytes() noexcept {
    return bytes_;
}

void Buffer::resize(std::size_t size) {
    bytes_.resize(size, 0);
}

void Buffer::fill(std::uint8_t value) noexcept {
    std::fill(bytes_.begin(), bytes_.end(), value);
}

void Buffer::require(std::size_t offset, std::size_t count) const {
    if (!contains(offset, count)) {
        throw std::out_of_range("memory access is outside the buffer");
    }
}

std::uint8_t Buffer::read_u8(std::size_t offset) const {
    require(offset, 1);
    return bytes_[offset];
}

std::uint16_t Buffer::read_u16_le(std::size_t offset) const {
    require(offset, 2);
    return static_cast<std::uint16_t>(bytes_[offset])
        | static_cast<std::uint16_t>(bytes_[offset + 1]) << 8;
}

std::int16_t Buffer::read_i16_le(std::size_t offset) const {
    return static_cast<std::int16_t>(read_u16_le(offset));
}

std::uint32_t Buffer::read_u32_le(std::size_t offset) const {
    require(offset, 4);
    return static_cast<std::uint32_t>(bytes_[offset])
        | static_cast<std::uint32_t>(bytes_[offset + 1]) << 8
        | static_cast<std::uint32_t>(bytes_[offset + 2]) << 16
        | static_cast<std::uint32_t>(bytes_[offset + 3]) << 24;
}

std::int32_t Buffer::read_i32_le(std::size_t offset) const {
    return static_cast<std::int32_t>(read_u32_le(offset));
}

float Buffer::read_f32_le(std::size_t offset) const {
    const std::uint32_t bits = read_u32_le(offset);
    float value = 0.0F;
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void Buffer::write_u8(std::size_t offset, std::uint8_t value) {
    require(offset, 1);
    bytes_[offset] = value;
}

void Buffer::write_u16_le(std::size_t offset, std::uint16_t value) {
    require(offset, 2);
    bytes_[offset] = static_cast<std::uint8_t>(value);
    bytes_[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void Buffer::write_u32_le(std::size_t offset, std::uint32_t value) {
    require(offset, 4);
    bytes_[offset] = static_cast<std::uint8_t>(value);
    bytes_[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes_[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes_[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

void Buffer::write_f32_le(std::size_t offset, float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&bits, &value, sizeof(value));
    write_u32_le(offset, bits);
}

} // namespace fruityprime::memory
