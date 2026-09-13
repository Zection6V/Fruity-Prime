#include "FATRecord.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace
{
    [[nodiscard]] std::span<const std::uint8_t> Slice(
        std::span<const std::uint8_t> span, std::size_t offset)
    {
        if (offset > span.size())
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return span.subspan(offset);
    }

    [[nodiscard]] std::span<std::uint8_t> Slice(
        std::span<std::uint8_t> span, std::size_t offset)
    {
        if (offset > span.size())
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return span.subspan(offset);
    }

    [[nodiscard]] std::span<std::uint8_t> Slice(
        std::span<std::uint8_t> span, std::size_t offset, std::size_t count)
    {
        if (offset > span.size() || count > span.size() - offset)
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return span.subspan(offset, count);
    }

    [[nodiscard]] std::uint32_t ReadUInt32LittleEndian(std::span<const std::uint8_t> span)
    {
        if (span.size() < sizeof(std::uint32_t))
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return static_cast<std::uint32_t>(span[0])
            | (static_cast<std::uint32_t>(span[1]) << 8U)
            | (static_cast<std::uint32_t>(span[2]) << 16U)
            | (static_cast<std::uint32_t>(span[3]) << 24U);
    }

    void WriteUInt32LittleEndian(std::span<std::uint8_t> span, std::uint32_t value)
    {
        if (span.size() < sizeof(std::uint32_t))
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        span[0] = static_cast<std::uint8_t>(value);
        span[1] = static_cast<std::uint8_t>(value >> 8U);
        span[2] = static_cast<std::uint8_t>(value >> 16U);
        span[3] = static_cast<std::uint8_t>(value >> 24U);
    }
}

namespace NCSFCommon::NC
{
    std::uint32_t FATRecord::Offset() const noexcept
    {
        return _offset;
    }

    void FATRecord::Offset(std::uint32_t value) noexcept
    {
        _offset = value;
    }

    std::uint32_t FATRecord::Size() const noexcept
    {
        return _size;
    }

    void FATRecord::Size(std::uint32_t value) noexcept
    {
        _size = value;
    }

    FATRecord& FATRecord::Read(std::span<const std::uint8_t> span)
    {
        _offset = ReadUInt32LittleEndian(span);
        _size = ReadUInt32LittleEndian(Slice(span, 0x04));
        return *this;
    }

    void FATRecord::Write(std::span<std::uint8_t> span)
    {
        WriteUInt32LittleEndian(span, _offset);
        WriteUInt32LittleEndian(Slice(span, 0x04), _size);
        std::span<std::uint8_t> reserved = Slice(span, 0x08, 0x08);
        std::fill(reserved.begin(), reserved.end(), std::uint8_t{0});
    }
}
