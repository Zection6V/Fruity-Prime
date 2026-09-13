#include "FATSection.hpp"

#include "../Common.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr std::array<std::uint8_t, 4> Header{
        static_cast<std::uint8_t>('F'),
        static_cast<std::uint8_t>('A'),
        static_cast<std::uint8_t>('T'),
        static_cast<std::uint8_t>(' ')
    };

    [[nodiscard]] std::span<const std::uint8_t> Slice(
        std::span<const std::uint8_t> span, std::size_t offset)
    {
        if (offset > span.size())
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return span.subspan(offset);
    }

#ifndef NDEBUG
    [[nodiscard]] std::span<const std::uint8_t> Slice(
        std::span<const std::uint8_t> span, std::size_t offset, std::size_t count)
    {
        if (offset > span.size() || count > span.size() - offset)
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return span.subspan(offset, count);
    }
#endif

    [[nodiscard]] std::span<std::uint8_t> Slice(
        std::span<std::uint8_t> span, std::size_t offset)
    {
        if (offset > span.size())
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return span.subspan(offset);
    }

    [[nodiscard]] std::int32_t ToInt32Unchecked(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::span<const std::uint8_t> SliceFromInt32(
        std::span<const std::uint8_t> span, std::uint32_t offset)
    {
        const std::int32_t signedOffset = ToInt32Unchecked(offset);
        if (signedOffset < 0)
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return Slice(span, static_cast<std::size_t>(signedOffset));
    }

    [[nodiscard]] std::span<std::uint8_t> SliceFromInt32(
        std::span<std::uint8_t> span, std::uint32_t offset)
    {
        const std::int32_t signedOffset = ToInt32Unchecked(offset);
        if (signedOffset < 0)
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return Slice(span, static_cast<std::size_t>(signedOffset));
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

    void CopyTo(std::span<const std::uint8_t> source, std::span<std::uint8_t> destination)
    {
        if (source.size() > destination.size())
        {
            throw std::invalid_argument("Destination is too short.");
        }
        std::copy(source.begin(), source.end(), destination.begin());
    }

    [[nodiscard]] std::size_t ListCount(std::uint32_t count)
    {
        const std::int32_t signedCount = ToInt32Unchecked(count);
        if (signedCount < 0)
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return static_cast<std::size_t>(signedCount);
    }

    [[nodiscard]] NCSFCommon::NC::FATRecord& Dereference(
        const std::shared_ptr<NCSFCommon::NC::FATRecord>& record)
    {
        if (!record)
        {
            throw std::runtime_error("Object reference not set to an instance of an object.");
        }
        return *record;
    }
}

namespace NCSFCommon::NC
{
    std::span<const std::shared_ptr<FATRecord>> FATSection::Records() const noexcept
    {
        return std::span<const std::shared_ptr<FATRecord>>(_records.data(), _records.size());
    }

    std::uint32_t FATSection::Size() const noexcept
    {
        return 0x0CU + static_cast<std::uint32_t>(_records.size()) * FATRecord::RecordSize;
    }

    void FATSection::Read(std::span<const std::uint8_t> span)
    {
#ifndef NDEBUG
        assert(Common::VerifyHeader(Slice(span, 0, Header.size()), Header));
#endif

        const std::uint32_t count = ReadUInt32LittleEndian(Slice(span, 0x08));
        _records.clear();

        const std::size_t requestedCount = ListCount(count);
        _records.reserve(requestedCount);

        std::uint32_t pos = 0x0CU;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            auto record = std::make_shared<FATRecord>();
            record->Read(SliceFromInt32(span, pos));
            _records.push_back(std::move(record));
            pos += FATRecord::RecordSize;
        }
    }

    void FATSection::Write(std::span<std::uint8_t> span)
    {
        CopyTo(Header, span);
        WriteUInt32LittleEndian(Slice(span, 0x04), Size());
        WriteUInt32LittleEndian(Slice(span, 0x08), static_cast<std::uint32_t>(_records.size()));

        std::uint32_t pos = 0x0CU;
        for (const auto& record : _records)
        {
            Dereference(record).Write(SliceFromInt32(span, pos));
            pos += FATRecord::RecordSize;
        }
    }

    void FATSection::ResizeRecords(std::uint32_t newSize)
    {
        const std::size_t requestedCount = ListCount(newSize);
        const std::size_t oldCount = _records.size();

        _records.resize(requestedCount);
        if (requestedCount > oldCount)
        {
            for (std::size_t i = oldCount; i < requestedCount; ++i)
            {
                _records[i] = std::make_shared<FATRecord>();
            }
        }
    }

    void FATSection::SetNumberOfRecords(std::uint32_t count)
    {
        const std::size_t requestedCount = ListCount(count);
        _records.resize(requestedCount);
        for (std::size_t i = 0; i < requestedCount; ++i)
        {
            _records[i] = std::make_shared<FATRecord>();
        }
    }

    FATSection FATSection::Add(const FATSection* fatSection1, const FATSection* fatSection2)
    {
        assert(fatSection1 != nullptr || fatSection2 != nullptr);

        FATSection result;
        if (fatSection1 != nullptr)
        {
            result._records.insert(
                result._records.end(), fatSection1->_records.begin(), fatSection1->_records.end());
        }
        if (fatSection2 != nullptr)
        {
            result._records.insert(
                result._records.end(), fatSection2->_records.begin(), fatSection2->_records.end());
        }
        return result;
    }

    FATSection operator+(const FATSection& fatSection1, const FATSection& fatSection2)
    {
        return FATSection::Add(&fatSection1, &fatSection2);
    }

    FATSection operator+(const FATSection& fatSection, std::nullptr_t)
    {
        return FATSection::Add(&fatSection, nullptr);
    }

    FATSection operator+(std::nullptr_t, const FATSection& fatSection)
    {
        return FATSection::Add(nullptr, &fatSection);
    }
}
