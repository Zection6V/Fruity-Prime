#pragma once

#include "INFOEntry.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace NCSFCommon::NC
{
    template <typename T>
        requires std::derived_from<T, INFOEntry> && std::default_initializable<T>
    struct INFORecordEntry final
    {
        std::uint32_t Offset = 0;
        std::shared_ptr<T> Entry;
    };

    template <typename T>
        requires std::derived_from<T, INFOEntry> && std::default_initializable<T>
    class INFORecord
    {
    public:
        using Entry = INFORecordEntry<T>;

        INFORecord() = default;

        INFORecord(const INFORecord&) = delete;
        INFORecord& operator=(const INFORecord&) = delete;
        INFORecord(INFORecord&&) noexcept = default;
        INFORecord& operator=(INFORecord&&) noexcept = default;

        [[nodiscard]] std::span<const Entry> Entries() const noexcept
        {
            return std::span<const Entry>(_entries.data(), _entries.size());
        }

        [[nodiscard]] std::uint32_t Size() const
        {
            return HeaderSize() + SizeOfEntries();
        }

        [[nodiscard]] std::uint32_t HeaderSize() const noexcept
        {
            return 0x04U + 4U * static_cast<std::uint32_t>(_entries.size());
        }

        [[nodiscard]] std::uint32_t SizeOfEntries() const
        {
            std::int64_t sum = 0;
            for (const Entry& entry : _entries)
            {
                const std::uint32_t size = entry.Entry == nullptr ? 0U : entry.Entry->Size();
                if (static_cast<std::uint64_t>(size)
                    > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() - sum))
                {
                    throw std::overflow_error("Arithmetic operation resulted in an overflow.");
                }
                sum += static_cast<std::int64_t>(size);
            }
            return static_cast<std::uint32_t>(sum);
        }

        void Read(std::span<const std::uint8_t> span, std::uint32_t offset)
        {
            const std::uint32_t count = ReadUInt32LittleEndian(span);
            const std::uint32_t byteCountBits = 4U * count;
            const std::int32_t byteCount = ToInt32Unchecked(byteCountBits);
            const std::span<const std::uint8_t> entryOffsetBytes = Slice(span, 0x04, byteCount);

            _entries.clear();

            const std::int32_t requestedCapacity = ToInt32Unchecked(count);
            if (requestedCapacity < 0)
            {
                ThrowArgumentOutOfRange();
            }
            _entries.reserve(static_cast<std::size_t>(requestedCapacity));

            for (std::size_t pos = 0; pos < entryOffsetBytes.size(); pos += sizeof(std::uint32_t))
            {
                const std::uint32_t entryOffset = ReadUInt32LittleEndian(entryOffsetBytes.subspan(pos));
                std::shared_ptr<T> entry;
                if (entryOffset != 0U)
                {
                    const std::uint32_t relativeBits = entryOffset - offset;
                    const std::int32_t relativeOffset = ToInt32Unchecked(relativeBits);
                    std::shared_ptr<T> created = std::make_shared<T>();
                    INFOEntry* readResult = static_cast<INFOEntry*>(created.get())->Read(Slice(span, relativeOffset));
                    if (T* typedResult = dynamic_cast<T*>(readResult); typedResult != nullptr)
                    {
                        entry = std::shared_ptr<T>(created, typedResult);
                    }
                }
                _entries.push_back(Entry{entryOffset, std::move(entry)});
            }
        }

        void FixOffsets(std::uint32_t startOffset)
        {
            std::uint32_t offset = startOffset;
            for (Entry& entry : _entries)
            {
                entry.Offset = offset;
                offset += entry.Entry == nullptr ? 0U : entry.Entry->Size();
            }
        }

        void WriteHeader(std::span<std::uint8_t> span) const
        {
            WriteUInt32LittleEndian(span, static_cast<std::uint32_t>(_entries.size()));
            std::int32_t pos = 0x04;
            for (const Entry& entry : _entries)
            {
                WriteUInt32LittleEndian(Slice(span, pos), entry.Offset);
                pos = AddInt32Unchecked(pos, 0x04U);
            }
        }

        void WriteData(std::span<std::uint8_t> span) const
        {
            std::uint32_t pos = 0;
            for (const Entry& entry : _entries)
            {
                if (entry.Entry != nullptr)
                {
                    entry.Entry->Write(Slice(span, ToInt32Unchecked(pos)));
                    pos += entry.Entry->Size();
                }
            }
        }

        friend INFORecord operator+(const INFORecord& infoRecord1, const INFORecord& infoRecord2)
        {
            INFORecord newINFORecord;
            newINFORecord._entries.insert(
                newINFORecord._entries.end(), infoRecord1._entries.begin(), infoRecord1._entries.end());
            newINFORecord._entries.insert(
                newINFORecord._entries.end(), infoRecord2._entries.begin(), infoRecord2._entries.end());
            return newINFORecord;
        }

        void SetNumberOfEntries(std::uint32_t count)
        {
            const std::int32_t signedCount = ToInt32Unchecked(count);
            if (signedCount < 0)
            {
                ThrowArgumentOutOfRange();
            }
            _entries.resize(static_cast<std::size_t>(signedCount));
            std::fill(_entries.begin(), _entries.end(), Entry{});
        }

        void ExpandNumberOfEntries(std::uint32_t count)
        {
            if (static_cast<std::uint64_t>(count) <= static_cast<std::uint64_t>(_entries.size()))
            {
                return;
            }

            const std::int32_t signedCount = ToInt32Unchecked(count);
            if (signedCount < 0)
            {
                ThrowArgumentOutOfRange();
            }
            _entries.resize(static_cast<std::size_t>(signedCount));
        }

        void SetEntry(std::uint32_t i, Entry entry)
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

    private:
        [[noreturn]] static void ThrowArgumentOutOfRange()
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }

        [[nodiscard]] static std::int32_t ToInt32Unchecked(std::uint32_t value) noexcept
        {
            return std::bit_cast<std::int32_t>(value);
        }

        [[nodiscard]] static std::span<const std::uint8_t> Slice(
            std::span<const std::uint8_t> span, std::int32_t offset)
        {
            if (offset < 0)
            {
                ThrowArgumentOutOfRange();
            }
            const std::size_t start = static_cast<std::size_t>(offset);
            if (start > span.size())
            {
                ThrowArgumentOutOfRange();
            }
            return span.subspan(start);
        }

        [[nodiscard]] static std::span<const std::uint8_t> Slice(
            std::span<const std::uint8_t> span, std::int32_t offset, std::int32_t count)
        {
            if (offset < 0 || count < 0)
            {
                ThrowArgumentOutOfRange();
            }
            const std::size_t start = static_cast<std::size_t>(offset);
            const std::size_t length = static_cast<std::size_t>(count);
            if (start > span.size() || length > span.size() - start)
            {
                ThrowArgumentOutOfRange();
            }
            return span.subspan(start, length);
        }

        [[nodiscard]] static std::span<std::uint8_t> Slice(
            std::span<std::uint8_t> span, std::int32_t offset)
        {
            if (offset < 0)
            {
                ThrowArgumentOutOfRange();
            }
            const std::size_t start = static_cast<std::size_t>(offset);
            if (start > span.size())
            {
                ThrowArgumentOutOfRange();
            }
            return span.subspan(start);
        }

        [[nodiscard]] static std::uint32_t ReadUInt32LittleEndian(std::span<const std::uint8_t> span)
        {
            if (span.size() < sizeof(std::uint32_t))
            {
                ThrowArgumentOutOfRange();
            }
            return static_cast<std::uint32_t>(span[0])
                | (static_cast<std::uint32_t>(span[1]) << 8U)
                | (static_cast<std::uint32_t>(span[2]) << 16U)
                | (static_cast<std::uint32_t>(span[3]) << 24U);
        }

        static void WriteUInt32LittleEndian(std::span<std::uint8_t> span, std::uint32_t value)
        {
            if (span.size() < sizeof(std::uint32_t))
            {
                ThrowArgumentOutOfRange();
            }
            span[0] = static_cast<std::uint8_t>(value);
            span[1] = static_cast<std::uint8_t>(value >> 8U);
            span[2] = static_cast<std::uint8_t>(value >> 16U);
            span[3] = static_cast<std::uint8_t>(value >> 24U);
        }

        [[nodiscard]] static std::int32_t AddInt32Unchecked(
            std::int32_t value, std::uint32_t increment) noexcept
        {
            const std::uint32_t bits = std::bit_cast<std::uint32_t>(value) + increment;
            return std::bit_cast<std::int32_t>(bits);
        }

        std::vector<Entry> _entries;
    };
}
