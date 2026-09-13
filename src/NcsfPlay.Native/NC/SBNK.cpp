#include "SBNK.hpp"

#include "../Common.hpp"
#include "SBNKInstrumentEntry.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr std::uint32_t Prime2 = 2246822519U;
    constexpr std::uint32_t Prime3 = 3266489917U;
    constexpr std::uint32_t Prime4 = 668265263U;
    constexpr std::uint32_t Prime5 = 374761393U;
    constexpr std::uint32_t ArrayMaxLength = 0x7FFFFFC7U;

    class InvalidDataException final : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    [[noreturn]] void ThrowArgumentOutOfRange()
    {
        throw std::out_of_range("Specified argument was out of the range of valid values.");
    }

    [[noreturn]] void ThrowNullReference()
    {
        throw std::runtime_error("Object reference not set to an instance of an object.");
    }

    [[nodiscard]] std::int32_t ToInt32Unchecked(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::size_t ToSize(std::int32_t value)
    {
        if (value < 0)
        {
            ThrowArgumentOutOfRange();
        }
        return static_cast<std::size_t>(value);
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

#ifndef NDEBUG
    [[nodiscard]] std::span<const std::uint8_t> Slice(
        std::span<const std::uint8_t> span, std::size_t offset, std::size_t count)
    {
        if (offset > span.size() || count > span.size() - offset)
        {
            ThrowArgumentOutOfRange();
        }
        return span.subspan(offset, count);
    }
#endif

    [[nodiscard]] std::span<std::uint8_t> Slice(
        std::span<std::uint8_t> span, std::size_t offset)
    {
        if (offset > span.size())
        {
            ThrowArgumentOutOfRange();
        }
        return span.subspan(offset);
    }

    [[nodiscard]] std::span<std::uint8_t> Slice(
        std::span<std::uint8_t> span, std::size_t offset, std::int32_t count)
    {
        const std::size_t length = ToSize(count);
        if (offset > span.size() || length > span.size() - offset)
        {
            ThrowArgumentOutOfRange();
        }
        return span.subspan(offset, length);
    }

    [[nodiscard]] std::span<std::uint8_t> SliceFromInt32(
        std::span<std::uint8_t> span, std::uint32_t offset)
    {
        return Slice(span, ToSize(ToInt32Unchecked(offset)));
    }

    [[nodiscard]] std::span<const std::uint8_t> SliceFromInt32(
        std::span<const std::uint8_t> span, std::int32_t offset)
    {
        return Slice(span, ToSize(offset));
    }

    [[nodiscard]] std::uint32_t ReadUInt32LittleEndian(std::span<const std::uint8_t> span)
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

    void WriteUInt32LittleEndian(std::span<std::uint8_t> span, std::uint32_t value)
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

    void CopyTo(std::span<const std::uint8_t> source, std::span<std::uint8_t> destination)
    {
        if (source.size() > destination.size())
        {
            throw std::invalid_argument("Destination is too short.");
        }
        std::copy(source.begin(), source.end(), destination.begin());
    }

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

    [[nodiscard]] std::uint32_t QueueRound(std::uint32_t hash, std::uint32_t queuedValue) noexcept
    {
        return std::rotl(hash + queuedValue * Prime3, 17) * Prime4;
    }

    [[nodiscard]] std::uint32_t MixFinal(std::uint32_t hash) noexcept
    {
        hash ^= hash >> 15U;
        hash *= Prime2;
        hash ^= hash >> 13U;
        hash *= Prime3;
        hash ^= hash >> 16U;
        return hash;
    }

    [[nodiscard]] std::int32_t CombineHashCodes(std::int32_t value1, std::int32_t value2)
    {
        std::uint32_t hash = GlobalHashSeed() + Prime5;
        hash += 8U;
        hash = QueueRound(hash, static_cast<std::uint32_t>(value1));
        hash = QueueRound(hash, static_cast<std::uint32_t>(value2));
        return std::bit_cast<std::int32_t>(MixFinal(hash));
    }

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

    [[nodiscard]] bool EntryEquality(
        const std::shared_ptr<NCSFCommon::NC::SBNKInstrumentEntry>& left,
        const std::shared_ptr<NCSFCommon::NC::SBNKInstrumentEntry>& right) noexcept
    {
        if (left == nullptr)
        {
            return right == nullptr;
        }
        return left->Equals(right.get());
    }

    void CopyEntries(
        std::span<const std::shared_ptr<NCSFCommon::NC::SBNKInstrumentEntry>> source,
        std::span<std::shared_ptr<NCSFCommon::NC::SBNKInstrumentEntry>> destination)
    {
        if (source.size() > destination.size())
        {
            throw std::invalid_argument("Destination is too short.");
        }
        if (source.empty())
        {
            return;
        }

        const auto* sourceBegin = source.data();
        const auto* sourceEnd = sourceBegin + source.size();
        auto* destinationBegin = destination.data();
        auto* destinationEnd = destinationBegin + source.size();

        const std::less<const std::shared_ptr<NCSFCommon::NC::SBNKInstrumentEntry>*> less;
        const bool overlaps = less(destinationBegin, sourceEnd) && less(sourceBegin, destinationEnd);
        if (overlaps && less(sourceBegin, destinationBegin))
        {
            for (std::size_t i = source.size(); i != 0U; --i)
            {
                destinationBegin[i - 1U] = sourceBegin[i - 1U];
            }
        }
        else
        {
            for (std::size_t i = 0; i < source.size(); ++i)
            {
                destinationBegin[i] = sourceBegin[i];
            }
        }
    }
}

namespace NCSFCommon::NC
{
    SBNK::EntryListState::EntryListState()
        : Items(std::make_shared<std::vector<Entry>>())
    {
    }

    SBNK::SBNK(std::optional<std::u16string> filename)
        : _filename(std::move(filename))
    {
    }

    std::uint32_t SBNK::Magic() const noexcept
    {
        return 0x0100FEFFU;
    }

    std::uint32_t SBNK::FileSize() const
    {
        return Size();
    }

    std::uint16_t SBNK::HeaderSize() const noexcept
    {
        return 0x10U;
    }

    std::uint16_t SBNK::Blocks() const noexcept
    {
        return 1U;
    }

    const std::optional<std::u16string>& SBNK::Filename() const noexcept
    {
        return _filename;
    }

    void SBNK::Filename(std::optional<std::u16string> value)
    {
        _filename = std::move(value);
    }

    std::span<const std::shared_ptr<SBNKInstrumentEntry>> SBNK::Entries() const noexcept
    {
        return std::span<const Entry>(
            _entries.Items->data(), static_cast<std::size_t>(_entries.Count));
    }

    std::int32_t SBNK::EntryNumber() const noexcept
    {
        return _entryNumber;
    }

    void SBNK::EntryNumber(std::int32_t value) noexcept
    {
        _entryNumber = value;
    }

    const std::shared_ptr<INFOEntryBANK>& SBNK::Info() const noexcept
    {
        return _info;
    }

    void SBNK::Info(std::shared_ptr<INFOEntryBANK> value) noexcept
    {
        _info = std::move(value);
    }

    std::uint32_t SBNK::Size() const
    {
        const std::uint16_t headerSize = HeaderSize();
        const std::uint32_t dataSize = DataSize();
        return static_cast<std::uint32_t>(headerSize)
            + ((dataSize + 3U) & ~0x03U);
    }

    std::uint32_t SBNK::DataSize() const
    {
        std::uint64_t sum = 0U;
        for (const Entry& entry : Entries())
        {
            if (entry == nullptr)
            {
                ThrowNullReference();
            }
            sum += static_cast<std::uint64_t>(entry->Size());
        }
        return 0x2CU + static_cast<std::uint32_t>(sum);
    }

    std::vector<std::uint8_t> SBNK::ExpectedHeader() const
    {
        return {
            static_cast<std::uint8_t>('S'),
            static_cast<std::uint8_t>('B'),
            static_cast<std::uint8_t>('N'),
            static_cast<std::uint8_t>('K')
        };
    }

    void SBNK::SetEntryCount(std::int32_t count)
    {
        if (count < 0)
        {
            ThrowArgumentOutOfRange();
        }
        _entries.Version = std::bit_cast<std::int32_t>(
            std::bit_cast<std::uint32_t>(_entries.Version) + 1U);

        if (count < _entries.Count)
        {
            auto& items = *_entries.Items;
            for (std::int32_t i = count; i < _entries.Count; ++i)
            {
                items[static_cast<std::size_t>(i)].reset();
            }
            _entries.Count = count;
            return;
        }

        const std::size_t requested = static_cast<std::size_t>(count);
        if (requested > _entries.Items->size())
        {
            const std::uint32_t oldCapacity = static_cast<std::uint32_t>(_entries.Items->size());
            std::uint32_t newCapacity = oldCapacity == 0U ? 4U : oldCapacity * 2U;
            if (newCapacity > ArrayMaxLength)
            {
                newCapacity = ArrayMaxLength;
            }
            if (newCapacity < static_cast<std::uint32_t>(count))
            {
                newCapacity = static_cast<std::uint32_t>(count);
            }

            auto newItems = std::make_shared<std::vector<Entry>>(
                static_cast<std::size_t>(newCapacity));
            std::copy_n(
                _entries.Items->begin(),
                static_cast<std::size_t>(_entries.Count),
                newItems->begin());
            _entries.Items = std::move(newItems);
        }

        _entries.Count = count;
    }

    void SBNK::Read(std::span<const std::uint8_t> span, bool failOnMissingFile)
    {
        try
        {
            NDSStandardHeader::Read(span);
        }
        catch (const InvalidDataException&)
        {
            if (failOnMissingFile)
            {
                throw;
            }
            return;
        }

#ifndef NDEBUG
        assert(Common::VerifyHeader(
            Slice(span, std::size_t{0x10U}, std::size_t{0x04U}),
            Common::DataBytes.Span()));
#endif

        const std::uint32_t count = ReadUInt32LittleEndian(Slice(span, 0x38U));
        SetEntryCount(ToInt32Unchecked(count));

        std::int32_t pos = 0x3C;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            auto entry = std::make_shared<SBNKInstrumentEntry>();
            entry->ReadHeader(SliceFromInt32(span, pos));
            (*_entries.Items)[static_cast<std::size_t>(i)] = std::move(entry);
            pos = ToInt32Unchecked(
                static_cast<std::uint32_t>(pos) + SBNKInstrumentEntry::HeaderSize);
        }

        for (const Entry& entry : Entries())
        {
            if (entry == nullptr)
            {
                ThrowNullReference();
            }
            entry->ReadInstruments(Slice(span, static_cast<std::size_t>(entry->Offset())));
        }
    }

    void SBNK::FixOffsets()
    {
        const std::uint32_t initial =
            0x3CU + 4U * static_cast<std::uint32_t>(_entries.Count);
        std::uint16_t offset = static_cast<std::uint16_t>(initial);
        for (const Entry& entry : Entries())
        {
            if (entry == nullptr)
            {
                ThrowNullReference();
            }
            offset = static_cast<std::uint16_t>(
                static_cast<std::uint32_t>(offset) + entry->FixOffset(offset));
        }
    }

    void SBNK::Write(std::span<std::uint8_t> span)
    {
        const std::int32_t clearLength = ToInt32Unchecked(Size());
        std::span<std::uint8_t> clearRange = Slice(span, 0U, clearLength);
        std::fill(clearRange.begin(), clearRange.end(), std::uint8_t{0});

        NDSStandardHeader::Write(span);
        CopyTo(Common::DataBytes.Span(), Slice(span, 0x10U));

        const std::uint32_t size = DataSize();
        const std::uint32_t sizeMulOf4 = (size + 3U) & ~0x03U;
        WriteUInt32LittleEndian(Slice(span, 0x14U), sizeMulOf4);

        WriteUInt32LittleEndian(
            Slice(span, 0x38U), static_cast<std::uint32_t>(_entries.Count));

        std::uint32_t pos = 0x3CU;
        for (const Entry& entry : Entries())
        {
            std::span<std::uint8_t> entrySpan = SliceFromInt32(span, pos);
            if (entry == nullptr)
            {
                ThrowNullReference();
            }
            entry->WriteHeader(entrySpan);
            pos += SBNKInstrumentEntry::HeaderSize;
        }

        for (const Entry& entry : Entries())
        {
            std::span<std::uint8_t> entrySpan = SliceFromInt32(span, pos);
            if (entry == nullptr)
            {
                ThrowNullReference();
            }
            entry->WriteData(entrySpan);
            pos += entry->DataSize();
        }
    }

    void SBNK::ReplaceInstruments(std::span<const std::shared_ptr<SBNKInstrumentEntry>> instruments)
    {
        if (instruments.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            ThrowArgumentOutOfRange();
        }

        SetEntryCount(static_cast<std::int32_t>(instruments.size()));
        std::span<Entry> destination(
            _entries.Items->data(), static_cast<std::size_t>(_entries.Count));
        CopyEntries(instruments, destination);
    }

    bool SBNK::Equals(const SBNK* other) const
    {
        if (other == nullptr || DataSize() != other->DataSize())
        {
            return false;
        }

        const std::span<const Entry> left = Entries();
        const std::span<const Entry> right = other->Entries();
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            if (!EntryEquality(left[i], right[i]))
            {
                return false;
            }
        }
        return true;
    }

    bool SBNK::Equals(const std::any& obj) const
    {
        if (const auto* other = std::any_cast<SBNK*>(&obj))
        {
            return Equals(*other);
        }
        if (const auto* other = std::any_cast<const SBNK*>(&obj))
        {
            return Equals(*other);
        }
        return false;
    }

    std::int32_t SBNK::GetHashCode() const
    {
        const std::int32_t dataSizeHash = std::bit_cast<std::int32_t>(DataSize());
        const std::int32_t entriesHash = IdentityHashCode(&_entries);
        return CombineHashCodes(dataSizeHash, entriesHash);
    }

    bool SBNK::EqualityOperator(const SBNK* left, const SBNK* right)
    {
        return left != nullptr && left->Equals(right);
    }

    bool SBNK::InequalityOperator(const SBNK* left, const SBNK* right)
    {
        return !EqualityOperator(left, right);
    }

    bool operator==(const SBNK& left, const SBNK& right)
    {
        return left.Equals(&right);
    }

    bool operator!=(const SBNK& left, const SBNK& right)
    {
        return !(left == right);
    }
}
