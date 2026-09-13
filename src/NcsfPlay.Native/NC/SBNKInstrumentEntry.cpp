#include "SBNKInstrumentEntry.hpp"

#include "SBNKInstrument.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <functional>
#include <random>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr std::uint32_t Prime2 = 2246822519U;
    constexpr std::uint32_t Prime3 = 3266489917U;
    constexpr std::uint32_t Prime4 = 668265263U;
    constexpr std::uint32_t Prime5 = 374761393U;

    [[noreturn]] void ThrowIndexOutOfRange()
    {
        throw std::out_of_range("Index was outside the bounds of the array.");
    }

    [[noreturn]] void ThrowArgumentOutOfRange()
    {
        throw std::out_of_range("Specified argument was out of the range of valid values.");
    }

    [[nodiscard]] const std::uint8_t& At(std::span<const std::uint8_t> span, std::size_t index)
    {
        if (index >= span.size())
        {
            ThrowIndexOutOfRange();
        }
        return span[index];
    }

    [[nodiscard]] std::uint8_t& At(std::span<std::uint8_t> span, std::size_t index)
    {
        if (index >= span.size())
        {
            ThrowIndexOutOfRange();
        }
        return span[index];
    }

    [[nodiscard]] std::span<const std::uint8_t> Slice(
        std::span<const std::uint8_t> span, std::size_t offset)
    {
        if (offset > span.size())
        {
            ThrowArgumentOutOfRange();
        }
        return span.subspan(offset);
    }

    [[nodiscard]] std::span<std::uint8_t> Slice(
        std::span<std::uint8_t> span, std::size_t offset)
    {
        if (offset > span.size())
        {
            ThrowArgumentOutOfRange();
        }
        return span.subspan(offset);
    }

    [[nodiscard]] std::span<const std::uint8_t> Prefix(
        std::span<const std::uint8_t> span, std::size_t count)
    {
        if (count > span.size())
        {
            ThrowArgumentOutOfRange();
        }
        return span.first(count);
    }

    [[nodiscard]] std::span<std::uint8_t> Range(
        std::span<std::uint8_t> span, std::size_t start, std::size_t end)
    {
        if (start > end || end > span.size())
        {
            ThrowArgumentOutOfRange();
        }
        return span.subspan(start, end - start);
    }

    [[nodiscard]] std::uint16_t ReadUInt16LittleEndian(std::span<const std::uint8_t> span)
    {
        if (span.size() < sizeof(std::uint16_t))
        {
            ThrowArgumentOutOfRange();
        }
        return static_cast<std::uint16_t>(static_cast<std::uint16_t>(span[0])
            | static_cast<std::uint16_t>(static_cast<std::uint16_t>(span[1]) << 8U));
    }

    void WriteUInt16LittleEndian(std::span<std::uint8_t> span, std::uint16_t value)
    {
        if (span.size() < sizeof(std::uint16_t))
        {
            ThrowArgumentOutOfRange();
        }
        span[0] = static_cast<std::uint8_t>(value);
        span[1] = static_cast<std::uint8_t>(value >> 8U);
    }

    // .NET 9 HashCode.Combine uses a process-global randomized seed.
    [[nodiscard]] std::uint32_t GlobalHashSeed()
    {
        static const std::uint32_t seed = []
        {
            std::random_device device;
            std::uniform_int_distribution<std::uint32_t> distribution;
            return distribution(device);
        }();
        return seed;
    }

    [[nodiscard]] std::uint32_t QueueRound(std::uint32_t hash, std::uint32_t queuedValue)
    {
        return std::rotl(hash + queuedValue * Prime3, 17) * Prime4;
    }

    [[nodiscard]] std::uint32_t MixFinal(std::uint32_t hash)
    {
        hash ^= hash >> 15U;
        hash *= Prime2;
        hash ^= hash >> 13U;
        hash *= Prime3;
        hash ^= hash >> 16U;
        return hash;
    }

    [[nodiscard]] std::int32_t CombineHashCodes(
        std::int32_t value1, std::int32_t value2, std::int32_t value3)
    {
        std::uint32_t hash = GlobalHashSeed() + Prime5;
        hash += 12U;
        hash = QueueRound(hash, static_cast<std::uint32_t>(value1));
        hash = QueueRound(hash, static_cast<std::uint32_t>(value2));
        hash = QueueRound(hash, static_cast<std::uint32_t>(value3));
        return std::bit_cast<std::int32_t>(MixFinal(hash));
    }

    // List<T> inherits object.GetHashCode(), so the third HashCode.Combine input
    // is the list object identity rather than a sequence hash.
    [[nodiscard]] std::int32_t IdentityHashCode(const void* object) noexcept
    {
        const std::size_t hash = std::hash<const void*>{}(object);
        if constexpr (sizeof(std::size_t) > sizeof(std::uint32_t))
        {
            const auto wide = static_cast<std::uint64_t>(hash);
            return std::bit_cast<std::int32_t>(
                static_cast<std::uint32_t>(wide ^ (wide >> 32U)));
        }
        else
        {
            return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(hash));
        }
    }
}

namespace NCSFCommon::NC
{
    std::uint8_t SBNKInstrumentEntry::Record() const noexcept
    {
        return _record;
    }

    void SBNKInstrumentEntry::Record(std::uint8_t value) noexcept
    {
        _record = value;
    }

    std::uint16_t SBNKInstrumentEntry::Offset() const noexcept
    {
        return _offset;
    }

    void SBNKInstrumentEntry::Offset(std::uint16_t value) noexcept
    {
        _offset = value;
    }

    std::span<const std::shared_ptr<SBNKInstrument>> SBNKInstrumentEntry::Instruments() const noexcept
    {
        return _instruments;
    }

    std::uint32_t SBNKInstrumentEntry::Size() const noexcept
    {
        return HeaderSize + DataSize();
    }

    std::uint32_t SBNKInstrumentEntry::DataSize() const noexcept
    {
        if (_record > 0U && _record < 6U)
        {
            return SBNKInstrument::Size;
        }
        if (_record == 16U)
        {
            return 0x02U + (SBNKInstrument::Size + 0x02U)
                * static_cast<std::uint32_t>(_instruments.size());
        }
        if (_record == 17U)
        {
            return 0x08U + (SBNKInstrument::Size + 0x02U)
                * static_cast<std::uint32_t>(_instruments.size());
        }
        return 0x00U;
    }

    SBNKInstrumentEntry& SBNKInstrumentEntry::ReadHeader(std::span<const std::uint8_t> span)
    {
        _record = At(span, 0x00U);
        _offset = ReadUInt16LittleEndian(Slice(span, 0x01U));
        return *this;
    }

    void SBNKInstrumentEntry::ReadInstruments(std::span<const std::uint8_t> span)
    {
        _instruments.clear();
        if (_record == 16U)
        {
            const std::uint8_t lowNote = At(span, 0x00U);
            const std::uint8_t highNote = At(span, 0x01U);
            const std::uint8_t num = static_cast<std::uint8_t>(
                static_cast<int>(highNote) - static_cast<int>(lowNote) + 1);
            _instruments.reserve(num);
            std::size_t pos = 0x02U;
            for (std::uint8_t i = 0; i < num; ++i)
            {
                const std::uint8_t note = static_cast<std::uint8_t>(
                    static_cast<unsigned int>(lowNote) + static_cast<unsigned int>(i));
                auto instrument = std::make_shared<SBNKInstrument>(note, note, At(span, pos));
                instrument->Read(Slice(span, pos + 0x02U));
                _instruments.push_back(std::move(instrument));
                pos += static_cast<std::size_t>(SBNKInstrument::Size + 0x02U);
            }
        }
        else if (_record == 17U)
        {
            const std::span<const std::uint8_t> thisRanges = Prefix(span, 0x08U);
            std::uint8_t i = 0;
            _instruments.reserve(8U);
            std::size_t pos = 0x08U;
            while (i < 8U && At(thisRanges, i) != 0U)
            {
                const std::uint8_t lowNote = i != 0U
                    ? static_cast<std::uint8_t>(static_cast<unsigned int>(At(thisRanges, i - 1U)) + 1U)
                    : std::uint8_t{0};
                const std::uint8_t highNote = At(thisRanges, i);
                auto instrument = std::make_shared<SBNKInstrument>(
                    lowNote, highNote, At(span, pos));
                instrument->Read(Slice(span, pos + 0x02U));
                _instruments.push_back(std::move(instrument));
                pos += static_cast<std::size_t>(SBNKInstrument::Size + 0x02U);
                ++i;
            }
        }
        else if (_record != 0U)
        {
            _instruments.reserve(1U);
            auto instrument = std::make_shared<SBNKInstrument>(0U, 127U, _record);
            instrument->Read(span);
            _instruments.push_back(std::move(instrument));
        }
    }

    std::uint16_t SBNKInstrumentEntry::FixOffset(std::uint16_t newOffset) noexcept
    {
        _offset = newOffset;
        return static_cast<std::uint16_t>(Size() - HeaderSize);
    }

    void SBNKInstrumentEntry::WriteHeader(std::span<std::uint8_t> span)
    {
        At(span, 0x00U) = _record;
        WriteUInt16LittleEndian(Slice(span, 0x01U), _offset);
        At(span, 0x03U) = 0U;
    }

    void SBNKInstrumentEntry::WriteData(std::span<std::uint8_t> span)
    {
        const auto instrumentAt = [this](std::size_t index) -> const std::shared_ptr<SBNKInstrument>&
        {
            if (index >= _instruments.size())
            {
                throw std::out_of_range("Index was out of range. Must be non-negative and less than the size of the collection.");
            }
            return _instruments[index];
        };

        if (_record == 16U)
        {
            const std::uint8_t lowNote = instrumentAt(0U)->LowNote();
            const std::uint8_t highNote = instrumentAt(_instruments.size() - 1U)->LowNote();
            const std::uint8_t num = static_cast<std::uint8_t>(
                static_cast<int>(highNote) - static_cast<int>(lowNote) + 1);
            At(span, 0x00U) = lowNote;
            At(span, 0x01U) = highNote;
            std::size_t pos = 0x02U;
            for (std::uint8_t i = 0; i < num; ++i)
            {
                const auto& instrument = instrumentAt(i);
                At(span, pos) = instrument->Record();
                At(span, pos + 0x01U) = 0U;
                instrument->Write(Slice(span, pos + 0x02U));
                pos += static_cast<std::size_t>(SBNKInstrument::Size + 0x02U);
            }
        }
        else if (_record == 17U)
        {
            const std::uint8_t actualRanges = static_cast<std::uint8_t>(_instruments.size());
            std::uint8_t i = 0;
            for (; i < actualRanges; ++i)
            {
                At(span, i) = instrumentAt(i)->HighNote();
            }
            if (actualRanges != 8U)
            {
                std::span<std::uint8_t> remainder = Range(span, actualRanges, 0x08U);
                std::fill(remainder.begin(), remainder.end(), std::uint8_t{0});
            }
            std::size_t pos = 0x08U;
            for (i = 0; i < actualRanges; ++i)
            {
                const auto& instrument = instrumentAt(i);
                At(span, pos) = instrument->Record();
                At(span, pos + 0x01U) = 0U;
                instrument->Write(Slice(span, pos + 0x02U));
                pos += static_cast<std::size_t>(SBNKInstrument::Size + 0x02U);
            }
        }
        else if (_record != 0U)
        {
            instrumentAt(0U)->Write(span);
        }
    }

    bool SBNKInstrumentEntry::Equals(const SBNKInstrumentEntry* other) const noexcept
    {
        if (other == nullptr || _record != other->_record || _offset != other->_offset
            || _instruments.size() != other->_instruments.size())
        {
            return false;
        }

        for (std::size_t i = 0; i < _instruments.size(); ++i)
        {
            const auto& left = _instruments[i];
            const auto& right = other->_instruments[i];
            if (left == nullptr)
            {
                if (right != nullptr)
                {
                    return false;
                }
            }
            else if (!left->Equals(right.get()))
            {
                return false;
            }
        }
        return true;
    }

    std::int32_t SBNKInstrumentEntry::GetHashCode() const
    {
        return CombineHashCodes(
            static_cast<std::int32_t>(_record),
            static_cast<std::int32_t>(_offset),
            IdentityHashCode(std::addressof(_instruments)));
    }

    // Preserve left?.Equals(right) ?? false, including null == null being false.
    bool SBNKInstrumentEntry::OpEquality(
        const SBNKInstrumentEntry* left, const SBNKInstrumentEntry* right) noexcept
    {
        return left != nullptr && left->Equals(right);
    }

    bool SBNKInstrumentEntry::OpInequality(
        const SBNKInstrumentEntry* left, const SBNKInstrumentEntry* right) noexcept
    {
        return !OpEquality(left, right);
    }

    bool operator==(const SBNKInstrumentEntry& left, const SBNKInstrumentEntry& right) noexcept
    {
        return left.Equals(std::addressof(right));
    }

    bool operator!=(const SBNKInstrumentEntry& left, const SBNKInstrumentEntry& right) noexcept
    {
        return !(left == right);
    }
}
