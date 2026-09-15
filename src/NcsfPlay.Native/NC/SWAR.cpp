#include "SWAR.hpp"

#include "../Common.hpp"
#include "SWAV.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstring>
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

    [[nodiscard]] std::span<const std::uint8_t> Slice(
        std::span<const std::uint8_t> span, std::size_t offset)
    {
        if (offset > span.size())
        {
            ThrowArgumentOutOfRange();
        }
        return span.subspan(offset);
    }

    [[nodiscard]] std::span<const std::uint8_t> Slice(
        std::span<const std::uint8_t> span, std::size_t offset, std::size_t count)
    {
        if (offset > span.size() || count > span.size() - offset)
        {
            ThrowArgumentOutOfRange();
        }
        return span.subspan(offset, count);
    }

    [[nodiscard]] std::span<const std::uint8_t> Slice(
        std::span<const std::uint8_t> span, std::size_t offset, std::int32_t count)
    {
        if (count < 0)
        {
            ThrowArgumentOutOfRange();
        }
        return Slice(span, offset, static_cast<std::size_t>(count));
    }

    [[nodiscard]] std::span<const std::uint8_t> SliceFromInt32(
        std::span<const std::uint8_t> span, std::uint32_t offset)
    {
        const std::int32_t signedOffset = ToInt32Unchecked(offset);
        if (signedOffset < 0)
        {
            ThrowArgumentOutOfRange();
        }
        return Slice(span, static_cast<std::size_t>(signedOffset));
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

    [[nodiscard]] std::span<std::uint8_t> Slice(
        std::span<std::uint8_t> span, std::size_t offset, std::int32_t count)
    {
        if (count < 0)
        {
            ThrowArgumentOutOfRange();
        }
        const std::size_t length = static_cast<std::size_t>(count);
        if (offset > span.size() || length > span.size() - offset)
        {
            ThrowArgumentOutOfRange();
        }
        return span.subspan(offset, length);
    }

    [[nodiscard]] std::span<std::uint8_t> SliceFromInt32(
        std::span<std::uint8_t> span, std::uint32_t offset)
    {
        const std::int32_t signedOffset = ToInt32Unchecked(offset);
        if (signedOffset < 0)
        {
            ThrowArgumentOutOfRange();
        }
        return Slice(span, static_cast<std::size_t>(signedOffset));
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

    [[nodiscard]] std::uint32_t ReadUInt32NativeEndian(std::span<const std::uint8_t> span)
    {
        if (span.size() < sizeof(std::uint32_t))
        {
            ThrowArgumentOutOfRange();
        }
        std::uint32_t value;
        std::memcpy(&value, span.data(), sizeof(value));
        return value;
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

    [[nodiscard]] bool SWAVEquality(
        const std::shared_ptr<NCSFCommon::NC::SWAV>& left,
        const std::shared_ptr<NCSFCommon::NC::SWAV>& right) noexcept
    {
        return NCSFCommon::NC::SWAV::OpEquality(left.get(), right.get());
    }
}

namespace NCSFCommon::NC
{
    std::int32_t SWAVDictionary::Count() const noexcept
    {
        return static_cast<std::int32_t>(_entries.size());
    }

    std::span<const SWAVDictionary::Entry> SWAVDictionary::Entries() const noexcept
    {
        return std::span<const Entry>(_entries.data(), _entries.size());
    }

    bool SWAVDictionary::ContainsKey(std::uint32_t key) const noexcept
    {
        return std::find_if(_entries.begin(), _entries.end(), [key](const Entry& entry)
        {
            return entry.Key == key;
        }) != _entries.end();
    }

    bool SWAVDictionary::TryGetValue(std::uint32_t key, std::shared_ptr<SWAV>& value) const noexcept
    {
        const auto found = std::find_if(_entries.begin(), _entries.end(), [key](const Entry& entry)
        {
            return entry.Key == key;
        });
        if (found == _entries.end())
        {
            value.reset();
            return false;
        }
        value = found->Value;
        return true;
    }

    const std::shared_ptr<SWAV>& SWAVDictionary::At(std::uint32_t key) const
    {
        const auto found = std::find_if(_entries.begin(), _entries.end(), [key](const Entry& entry)
        {
            return entry.Key == key;
        });
        if (found == _entries.end())
        {
            throw std::out_of_range("The given key was not present in the dictionary.");
        }
        return found->Value;
    }

    void SWAVDictionary::Clear() noexcept
    {
        _entries.clear();
    }

    void SWAVDictionary::Set(std::uint32_t key, std::shared_ptr<SWAV> value)
    {
        const auto found = std::find_if(_entries.begin(), _entries.end(), [key](const Entry& entry)
        {
            return entry.Key == key;
        });
        if (found != _entries.end())
        {
            found->Value = std::move(value);
            return;
        }
        _entries.push_back(Entry{key, std::move(value)});
    }

    ReadOnlySWAVDictionary::ReadOnlySWAVDictionary(const SWAVDictionary* dictionary) noexcept
        : _dictionary(dictionary)
    {
    }

    std::int32_t ReadOnlySWAVDictionary::Count() const noexcept
    {
        return _dictionary->Count();
    }

    std::span<const SWAVDictionary::Entry> ReadOnlySWAVDictionary::Entries() const noexcept
    {
        return _dictionary->Entries();
    }

    bool ReadOnlySWAVDictionary::ContainsKey(std::uint32_t key) const noexcept
    {
        return _dictionary->ContainsKey(key);
    }

    bool ReadOnlySWAVDictionary::TryGetValue(std::uint32_t key, std::shared_ptr<SWAV>& value) const noexcept
    {
        return _dictionary->TryGetValue(key, value);
    }

    const std::shared_ptr<SWAV>& ReadOnlySWAVDictionary::At(std::uint32_t key) const
    {
        return _dictionary->At(key);
    }

    SWAR::SWAR(std::optional<std::u16string> filename)
        : _filename(std::move(filename)),
          _swavsView(&_swavs)
    {
    }

    std::uint32_t SWAR::Magic() const noexcept
    {
        return 0x0100FEFFU;
    }

    std::uint32_t SWAR::FileSize() const
    {
        return Size();
    }

    std::uint16_t SWAR::HeaderSize() const noexcept
    {
        return 0x10U;
    }

    std::uint16_t SWAR::Blocks() const noexcept
    {
        return 1U;
    }

    const std::optional<std::u16string>& SWAR::Filename() const noexcept
    {
        return _filename;
    }

    void SWAR::Filename(std::optional<std::u16string> value)
    {
        _filename = std::move(value);
    }

    const ReadOnlySWAVDictionary& SWAR::SWAVs() const noexcept
    {
        return _swavsView;
    }

    std::int32_t SWAR::EntryNumber() const noexcept
    {
        return _entryNumber;
    }

    void SWAR::EntryNumber(std::int32_t value) noexcept
    {
        _entryNumber = value;
    }

    std::uint32_t SWAR::Size() const
    {
        const std::uint16_t headerSize = HeaderSize();
        const std::uint32_t dataSize = DataSize();
        return static_cast<std::uint32_t>(headerSize) + dataSize;
    }

    std::uint32_t SWAR::DataSize() const
    {
        const std::int32_t count = _swavs.Count();
        std::int64_t waveSize = 0;
        for (const SWAVDictionary::Entry& entry : _swavs.Entries())
        {
            if (entry.Value == nullptr)
            {
                ThrowNullReference();
            }
            waveSize += static_cast<std::int64_t>(entry.Value->Size());
        }
        return 0x2CU
            + 4U * static_cast<std::uint32_t>(count)
            + static_cast<std::uint32_t>(waveSize);
    }

    std::vector<std::uint8_t> SWAR::ExpectedHeader() const
    {
        return {
            static_cast<std::uint8_t>('S'),
            static_cast<std::uint8_t>('W'),
            static_cast<std::uint8_t>('A'),
            static_cast<std::uint8_t>('R')
        };
    }

    void SWAR::Read(std::span<const std::uint8_t> span, bool failOnMissingFile)
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
        assert(Common::VerifyHeader(Slice(span, std::size_t{0x10U}, std::size_t{0x04U}), Common::DataBytes.Span()));
#endif

        const std::uint32_t count = ReadUInt32LittleEndian(Slice(span, 0x38U));
        const std::uint32_t offsetByteCountBits = 4U * count;
        const std::int32_t offsetByteCount = ToInt32Unchecked(offsetByteCountBits);
        const std::span<const std::uint8_t> offsetBytes = Slice(span, 0x3CU, offsetByteCount);

        for (std::int32_t i = 0;
             static_cast<std::int64_t>(i) < static_cast<std::int64_t>(count);
             ++i)
        {
            const std::size_t offsetPosition = static_cast<std::size_t>(i) * sizeof(std::uint32_t);
            const std::uint32_t offset = ReadUInt32NativeEndian(Slice(offsetBytes, offsetPosition));
            if (offset != 0U)
            {
                auto swav = std::make_shared<SWAV>();
                swav->Read(SliceFromInt32(span, offset));
                _swavs.Set(static_cast<std::uint32_t>(i), std::move(swav));
            }
        }
    }

    void SWAR::Write(std::span<std::uint8_t> span)
    {
        const std::int32_t clearLength = ToInt32Unchecked(Size());
        std::span<std::uint8_t> clearRange = Slice(span, 0U, clearLength);
        std::fill(clearRange.begin(), clearRange.end(), std::uint8_t{0});

        NDSStandardHeader::Write(span);
        CopyTo(Common::DataBytes.Span(), Slice(span, 0x10U));
        WriteUInt32LittleEndian(Slice(span, 0x14U), DataSize());

        const std::uint32_t count = static_cast<std::uint32_t>(_swavs.Count());
        WriteUInt32LittleEndian(Slice(span, 0x38U), count);

        std::uint32_t offset = 0x3CU + 4U * count;
        std::uint32_t pos = 0x3CU;
        for (const SWAVDictionary::Entry& entry : _swavs.Entries())
        {
            WriteUInt32LittleEndian(SliceFromInt32(span, pos), offset);
            if (entry.Value == nullptr)
            {
                ThrowNullReference();
            }
            offset += entry.Value->Size();
            pos += 0x04U;
        }

        for (const SWAVDictionary::Entry& entry : _swavs.Entries())
        {
            std::span<std::uint8_t> swavSpan = SliceFromInt32(span, pos);
            if (entry.Value == nullptr)
            {
                ThrowNullReference();
            }
            entry.Value->Write(swavSpan);
            pos += entry.Value->Size();
        }
    }

    void SWAR::ReplaceSWAVs(const SWAVDictionary* swavs)
    {
        _swavs.Clear();
        if (swavs == nullptr)
        {
            ThrowNullReference();
        }
        for (const SWAVDictionary::Entry& entry : swavs->Entries())
        {
            _swavs.Set(entry.Key, entry.Value);
        }
    }

    bool SWAR::Equals(const SWAR* other) const
    {
        if (other == nullptr)
        {
            return false;
        }

        const std::uint32_t dataSize = DataSize();
        const std::uint32_t otherDataSize = other->DataSize();
        if (dataSize != otherDataSize)
        {
            return false;
        }

        const std::int32_t count = _swavs.Count();
        const std::int32_t otherCount = other->_swavs.Count();
        if (count != otherCount)
        {
            return false;
        }

        for (const SWAVDictionary::Entry& entry : _swavs.Entries())
        {
            std::shared_ptr<SWAV> otherSWAV;
            if (!other->_swavs.TryGetValue(entry.Key, otherSWAV) || !SWAVEquality(entry.Value, otherSWAV))
            {
                return false;
            }
        }
        return true;
    }

    bool SWAR::Equals(const std::any& obj) const
    {
        if (const auto* other = std::any_cast<SWAR*>(&obj))
        {
            return Equals(*other);
        }
        if (const auto* other = std::any_cast<const SWAR*>(&obj))
        {
            return Equals(*other);
        }
        return false;
    }

    std::int32_t SWAR::GetHashCode() const
    {
        const std::int32_t dataSizeHash = std::bit_cast<std::int32_t>(DataSize());
        const std::int32_t dictionaryHash = IdentityHashCode(&_swavs);
        return CombineHashCodes(dataSizeHash, dictionaryHash);
    }

    bool SWAR::EqualityOperator(const SWAR* left, const SWAR* right)
    {
        return left != nullptr && left->Equals(right);
    }

    bool SWAR::InequalityOperator(const SWAR* left, const SWAR* right)
    {
        return !EqualityOperator(left, right);
    }

    bool operator==(const SWAR& left, const SWAR& right)
    {
        return left.Equals(&right);
    }

    bool operator!=(const SWAR& left, const SWAR& right)
    {
        return !(left == right);
    }
}
