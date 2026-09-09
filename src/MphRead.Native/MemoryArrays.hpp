#pragma once

#include "Memory.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace fruityprime::memory {

class ByteArrayView {
public:
    ByteArrayView(Buffer& buffer, std::size_t offset, std::size_t count);
    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] std::uint8_t get(std::size_t index) const;
    void set(std::size_t index, std::uint8_t value);
    void fill(std::uint8_t value) noexcept;
    [[nodiscard]] std::vector<std::uint8_t> copy() const;

private:
    Buffer* buffer_ = nullptr;
    std::size_t offset_ = 0;
    std::size_t count_ = 0;
};

template <typename T>
class ScalarArrayView {
public:
    ScalarArrayView(Buffer& buffer, std::size_t offset, std::size_t count)
        : buffer_(&buffer), offset_(offset), count_(count) {
        if (count > (static_cast<std::size_t>(-1) / sizeof(T))
            || !buffer.contains(offset, count * sizeof(T))) {
            throw std::out_of_range("memory scalar array is outside the view");
        }
    }

    [[nodiscard]] std::size_t size() const noexcept { return count_; }

    [[nodiscard]] T get(std::size_t index) const {
        require_index(index);
        return read(index);
    }

    void set(std::size_t index, T value) {
        require_index(index);
        write(index, value);
    }

private:
    void require_index(std::size_t index) const {
        if (index >= count_) {
            throw std::out_of_range("memory scalar array index is outside the view");
        }
    }

    [[nodiscard]] T read(std::size_t index) const {
        const std::size_t offset = offset_ + index * sizeof(T);
        if constexpr (std::is_same_v<T, std::int8_t>) {
            return static_cast<T>(buffer_->read_u8(offset));
        } else if constexpr (std::is_same_v<T, std::uint8_t>) {
            return buffer_->read_u8(offset);
        } else if constexpr (std::is_same_v<T, std::int16_t>) {
            return buffer_->read_i16_le(offset);
        } else if constexpr (std::is_same_v<T, std::uint16_t>) {
            return buffer_->read_u16_le(offset);
        } else if constexpr (std::is_same_v<T, std::int32_t>) {
            return buffer_->read_i32_le(offset);
        } else if constexpr (std::is_same_v<T, std::uint32_t>) {
            return buffer_->read_u32_le(offset);
        } else if constexpr (std::is_same_v<T, float>) {
            return buffer_->read_f32_le(offset);
        } else {
            static_assert(std::is_same_v<T, void>,
                          "unsupported native memory array scalar");
        }
    }

    void write(std::size_t index, T value) {
        const std::size_t offset = offset_ + index * sizeof(T);
        if constexpr (std::is_same_v<T, std::int8_t>) {
            buffer_->write_u8(offset, static_cast<std::uint8_t>(value));
        } else if constexpr (std::is_same_v<T, std::uint8_t>) {
            buffer_->write_u8(offset, value);
        } else if constexpr (std::is_same_v<T, std::int16_t>) {
            buffer_->write_u16_le(offset, static_cast<std::uint16_t>(value));
        } else if constexpr (std::is_same_v<T, std::uint16_t>) {
            buffer_->write_u16_le(offset, value);
        } else if constexpr (std::is_same_v<T, std::int32_t>) {
            buffer_->write_u32_le(offset, static_cast<std::uint32_t>(value));
        } else if constexpr (std::is_same_v<T, std::uint32_t>) {
            buffer_->write_u32_le(offset, value);
        } else if constexpr (std::is_same_v<T, float>) {
            buffer_->write_f32_le(offset, value);
        } else {
            static_assert(std::is_same_v<T, void>,
                          "unsupported native memory array scalar");
        }
    }

    Buffer* buffer_ = nullptr;
    std::size_t offset_ = 0;
    std::size_t count_ = 0;
};

using SByteArrayView = ScalarArrayView<std::int8_t>;
using UInt16ArrayView = ScalarArrayView<std::uint16_t>;
using Int16ArrayView = ScalarArrayView<std::int16_t>;
using Int32ArrayView = ScalarArrayView<std::int32_t>;
using UInt32ArrayView = ScalarArrayView<std::uint32_t>;
using FloatArrayView = ScalarArrayView<float>;

template <typename Enum>
class EnumArrayView {
    static_assert(std::is_enum_v<Enum>);
    using Raw = std::underlying_type_t<Enum>;

public:
    EnumArrayView(Buffer& buffer, std::size_t offset, std::size_t count)
        : values_(buffer, offset, count) {}

    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }
    [[nodiscard]] Enum get(std::size_t index) const {
        return static_cast<Enum>(values_.get(index));
    }
    void set(std::size_t index, Enum value) {
        values_.set(index, static_cast<Raw>(value));
    }

private:
    ScalarArrayView<Raw> values_;
};

} // namespace fruityprime::memory

namespace MphReadNative {
using MemoryArray = ::fruityprime::memory::ByteArrayView;
using SByteArray = ::fruityprime::memory::SByteArrayView;
using Int16Array = ::fruityprime::memory::Int16ArrayView;
using UInt16Array = ::fruityprime::memory::UInt16ArrayView;
using Int32Array = ::fruityprime::memory::Int32ArrayView;
using UInt32Array = ::fruityprime::memory::UInt32ArrayView;
using FloatArray = ::fruityprime::memory::FloatArrayView;
}
