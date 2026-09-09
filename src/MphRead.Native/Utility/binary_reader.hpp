#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>

namespace fruityprime::core {

// Bounds-checked little-endian reader for the binary formats used by the
// cartridge and its extracted files. Keeping cursor movement here makes the
// native readers fail at the field that is outside the input instead of
// turning a corrupt offset into an unrelated access violation later.
class BinaryReader {
public:
    explicit BinaryReader(std::span<const std::uint8_t> bytes) noexcept
        : bytes_(bytes) {}

    [[nodiscard]] std::size_t position() const noexcept { return position_; }
    [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }
    [[nodiscard]] std::size_t remaining() const noexcept {
        return bytes_.size() - position_;
    }

    void seek(std::size_t position) {
        if (position > bytes_.size()) {
            throw std::out_of_range("binary reader seek is outside the input");
        }
        position_ = position;
    }

    void skip(std::size_t count) { static_cast<void>(take(count)); }

    [[nodiscard]] std::span<const std::uint8_t> read_bytes(std::size_t count) {
        return take(count);
    }

    [[nodiscard]] std::span<const std::uint8_t> peek_bytes(
        std::size_t count) const {
        if (count > bytes_.size() - position_) {
            throw std::out_of_range("binary reader read is outside the input");
        }
        return bytes_.subspan(position_, count);
    }

    [[nodiscard]] std::uint8_t read_u8() {
        return take(1)[0];
    }

    [[nodiscard]] std::int8_t read_i8() {
        return static_cast<std::int8_t>(read_u8());
    }

    [[nodiscard]] std::uint16_t read_u16_le() {
        const auto bytes = take(2);
        return static_cast<std::uint16_t>(bytes[0])
            | static_cast<std::uint16_t>(bytes[1]) << 8;
    }

    [[nodiscard]] std::uint16_t read_u16_be() {
        const auto bytes = take(2);
        return static_cast<std::uint16_t>(bytes[1])
            | static_cast<std::uint16_t>(bytes[0]) << 8;
    }

    [[nodiscard]] std::int16_t read_i16_le() {
        return static_cast<std::int16_t>(read_u16_le());
    }

    [[nodiscard]] std::uint32_t read_u32_le() {
        const auto bytes = take(4);
        return static_cast<std::uint32_t>(bytes[0])
            | static_cast<std::uint32_t>(bytes[1]) << 8
            | static_cast<std::uint32_t>(bytes[2]) << 16
            | static_cast<std::uint32_t>(bytes[3]) << 24;
    }

    [[nodiscard]] std::uint32_t read_u32_be() {
        const auto bytes = take(4);
        return static_cast<std::uint32_t>(bytes[3])
            | static_cast<std::uint32_t>(bytes[2]) << 8
            | static_cast<std::uint32_t>(bytes[1]) << 16
            | static_cast<std::uint32_t>(bytes[0]) << 24;
    }

    [[nodiscard]] std::int32_t read_i32_le() {
        return static_cast<std::int32_t>(read_u32_le());
    }

    [[nodiscard]] float read_f32_le() {
        const std::uint32_t bits = read_u32_le();
        float value = 0;
        static_assert(sizeof(value) == sizeof(bits));
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    // Reads a fixed-width, NUL-terminated byte string without interpreting
    // bytes above ASCII as locale-dependent text. The game files use ASCII
    // names; an unsupported byte is represented visibly rather than leaking
    // an encoding decision into every caller.
    [[nodiscard]] std::string read_ascii(std::size_t width) {
        const auto bytes = take(width);
        std::string result;
        result.reserve(width);
        for (std::uint8_t value : bytes) {
            if (value == 0) {
                break;
            }
            result.push_back(value < 32 || value > 126
                ? '?' : static_cast<char>(value));
        }
        return result;
    }

    // Same as read_ascii, but preserves the bytes as chars. This is for raw
    // names whose non-ASCII values are part of a format rather than player
    // text (for example the DS cartridge title).
    [[nodiscard]] std::string read_raw_string(std::size_t width) {
        const auto bytes = take(width);
        std::size_t length = 0;
        while (length < bytes.size() && bytes[length] != 0) {
            ++length;
        }
        return std::string(reinterpret_cast<const char*>(bytes.data()), length);
    }

private:
    [[nodiscard]] std::span<const std::uint8_t> take(std::size_t count) {
        if (count > bytes_.size() - position_) {
            throw std::out_of_range("binary reader read is outside the input");
        }
        const auto result = bytes_.subspan(position_, count);
        position_ += count;
        return result;
    }

    std::span<const std::uint8_t> bytes_;
    std::size_t position_ = 0;
};

} // namespace fruityprime::core
