#include "SYMBRecord.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace
{
    [[noreturn]] void ThrowOutOfRange()
    {
        throw std::out_of_range("Specified argument was out of the range of valid values.");
    }

    [[nodiscard]] std::int32_t ToInt32Unchecked(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::span<const std::uint8_t> Slice(
        std::span<const std::uint8_t> span, std::int32_t offset)
    {
        if (offset < 0)
        {
            ThrowOutOfRange();
        }
        const std::size_t start = static_cast<std::size_t>(offset);
        if (start > span.size())
        {
            ThrowOutOfRange();
        }
        return span.subspan(start);
    }

    [[nodiscard]] std::span<const std::uint8_t> Slice(
        std::span<const std::uint8_t> span, std::int32_t offset, std::int32_t count)
    {
        if (offset < 0 || count < 0)
        {
            ThrowOutOfRange();
        }
        const std::size_t start = static_cast<std::size_t>(offset);
        const std::size_t length = static_cast<std::size_t>(count);
        if (start > span.size() || length > span.size() - start)
        {
            ThrowOutOfRange();
        }
        return span.subspan(start, length);
    }

    [[nodiscard]] std::span<std::uint8_t> Slice(
        std::span<std::uint8_t> span, std::int32_t offset)
    {
        if (offset < 0)
        {
            ThrowOutOfRange();
        }
        const std::size_t start = static_cast<std::size_t>(offset);
        if (start > span.size())
        {
            ThrowOutOfRange();
        }
        return span.subspan(start);
    }

    [[nodiscard]] std::uint32_t ReadUInt32LittleEndian(std::span<const std::uint8_t> span)
    {
        if (span.size() < sizeof(std::uint32_t))
        {
            ThrowOutOfRange();
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
            ThrowOutOfRange();
        }
        span[0] = static_cast<std::uint8_t>(value);
        span[1] = static_cast<std::uint8_t>(value >> 8U);
        span[2] = static_cast<std::uint8_t>(value >> 16U);
        span[3] = static_cast<std::uint8_t>(value >> 24U);
    }

    [[nodiscard]] std::u16string ReadNullTerminatedString(std::span<const std::uint8_t> span)
    {
        std::u16string result;
        std::size_t pos = 0;
        std::uint8_t chr;
        do
        {
            if (pos >= span.size())
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            chr = span[pos++];
            if (chr != 0)
            {
                result.push_back(static_cast<char16_t>(chr));
            }
        } while (chr != 0);
        return result;
    }

    void WriteNullTerminatedString(std::span<std::uint8_t> span, std::u16string_view str)
    {
        std::size_t pos = 0;
        for (char16_t chr : str)
        {
            if (pos >= span.size())
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            span[pos++] = static_cast<std::uint8_t>(chr);
        }
        if (pos >= span.size())
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        span[pos] = 0;
    }

    [[nodiscard]] std::int32_t AddInt32Unchecked(std::int32_t value, std::uint32_t increment) noexcept
    {
        const std::uint32_t bits = std::bit_cast<std::uint32_t>(value) + increment;
        return std::bit_cast<std::int32_t>(bits);
    }
}

namespace NCSFCommon::NC
{
    std::span<const SYMBRecord::Entry> SYMBRecord::Entries() const noexcept
    {
        return std::span<const Entry>(_entries.data(), _entries.size());
    }

    std::uint32_t SYMBRecord::Size() const
    {
        return HeaderSize() + SizeOfNames();
    }

    std::uint32_t SYMBRecord::HeaderSize() const noexcept
    {
        return 0x04U + 4U * static_cast<std::uint32_t>(_entries.size());
    }

    std::uint32_t SYMBRecord::SizeOfNames() const
    {
        std::int32_t sum = 0;
        for (const Entry& entry : _entries)
        {
            if (!entry.Name.has_value() || entry.Name->empty())
            {
                continue;
            }

            if (entry.Name->size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
            {
                throw std::overflow_error("Arithmetic operation resulted in an overflow.");
            }
            const std::uint32_t lengthBits = static_cast<std::uint32_t>(entry.Name->size());
            const std::int32_t lengthWithTerminator =
                ToInt32Unchecked(lengthBits + 1U);
            const std::int64_t next = static_cast<std::int64_t>(sum)
                + static_cast<std::int64_t>(lengthWithTerminator);
            if (next > std::numeric_limits<std::int32_t>::max()
                || next < std::numeric_limits<std::int32_t>::min())
            {
                throw std::overflow_error("Arithmetic operation resulted in an overflow.");
            }
            sum = static_cast<std::int32_t>(next);
        }
        return static_cast<std::uint32_t>(sum);
    }

    void SYMBRecord::Read(std::span<const std::uint8_t> span, std::uint32_t offset)
    {
        const std::uint32_t count = ReadUInt32LittleEndian(span);
        const std::uint32_t byteCountBits = 4U * count;
        const std::int32_t byteCount = ToInt32Unchecked(byteCountBits);
        const std::span<const std::uint8_t> entryOffsetBytes = Slice(span, 0x04, byteCount);

        _entries.clear();

        const std::int32_t requestedCapacity = ToInt32Unchecked(count);
        if (requestedCapacity < 0)
        {
            ThrowOutOfRange();
        }
        _entries.reserve(static_cast<std::size_t>(requestedCapacity));

        for (std::size_t pos = 0; pos < entryOffsetBytes.size(); pos += sizeof(std::uint32_t))
        {
            const std::uint32_t entryOffset = ReadUInt32LittleEndian(entryOffsetBytes.subspan(pos));
            std::optional<std::u16string> name;
            if (entryOffset != 0)
            {
                const std::uint32_t relativeBits = entryOffset - offset;
                const std::int32_t relativeOffset = ToInt32Unchecked(relativeBits);
                name = ReadNullTerminatedString(Slice(span, relativeOffset));
            }
            _entries.push_back(Entry{entryOffset, std::move(name)});
        }
    }

    void SYMBRecord::FixOffsets(std::uint32_t startOffset) noexcept
    {
        std::uint32_t offset = startOffset;
        for (Entry& entry : _entries)
        {
            entry.Offset = offset;
            if (entry.Name.has_value() && !entry.Name->empty())
            {
                offset += static_cast<std::uint32_t>(entry.Name->size()) + 1U;
            }
        }
    }

    void SYMBRecord::WriteHeader(std::span<std::uint8_t> span) const
    {
        WriteUInt32LittleEndian(span, static_cast<std::uint32_t>(_entries.size()));
        std::int32_t pos = 0x04;
        for (const Entry& entry : _entries)
        {
            WriteUInt32LittleEndian(Slice(span, pos), entry.Offset);
            pos = AddInt32Unchecked(pos, 0x04U);
        }
    }

    void SYMBRecord::WriteData(std::span<std::uint8_t> span) const
    {
        std::int32_t pos = 0;
        for (const Entry& entry : _entries)
        {
            if (entry.Name.has_value() && !entry.Name->empty())
            {
                WriteNullTerminatedString(Slice(span, pos), *entry.Name);
                const std::uint32_t increment = static_cast<std::uint32_t>(entry.Name->size()) + 1U;
                pos = AddInt32Unchecked(pos, increment);
            }
        }
    }

    SYMBRecord operator+(const SYMBRecord& symbRecord1, const SYMBRecord& symbRecord2)
    {
        SYMBRecord newSYMBRecord;
        newSYMBRecord._entries.reserve(symbRecord1._entries.size() + symbRecord2._entries.size());
        newSYMBRecord._entries.insert(
            newSYMBRecord._entries.end(), symbRecord1._entries.begin(), symbRecord1._entries.end());
        newSYMBRecord._entries.insert(
            newSYMBRecord._entries.end(), symbRecord2._entries.begin(), symbRecord2._entries.end());
        return newSYMBRecord;
    }

    void SYMBRecord::SetNumberOfEntries(std::uint32_t count)
    {
        const std::int32_t signedCount = ToInt32Unchecked(count);
        if (signedCount < 0)
        {
            ThrowOutOfRange();
        }
        _entries.resize(static_cast<std::size_t>(signedCount));
        std::fill(_entries.begin(), _entries.end(), Entry{});
    }

    void SYMBRecord::ExpandNumberOfEntries(std::uint32_t count)
    {
        if (static_cast<std::uint64_t>(count) <= static_cast<std::uint64_t>(_entries.size()))
        {
            return;
        }

        const std::int32_t signedCount = ToInt32Unchecked(count);
        if (signedCount < 0)
        {
            ThrowOutOfRange();
        }
        _entries.resize(static_cast<std::size_t>(signedCount));
    }

    void SYMBRecord::SetEntry(std::uint32_t i, Entry entry)
    {
#ifndef NDEBUG
        assert(static_cast<std::uint64_t>(i) <= static_cast<std::uint64_t>(_entries.size()));
#endif
        if (static_cast<std::uint64_t>(i) >= static_cast<std::uint64_t>(_entries.size()))
        {
            throw std::out_of_range("Index was out of range. Must be non-negative and less than the size of the collection.");
        }
        _entries[static_cast<std::size_t>(i)] = std::move(entry);
    }
}
