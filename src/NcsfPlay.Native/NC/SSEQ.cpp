#include "SSEQ.hpp"

#include "../Common.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstring>
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

    class InvalidDataException final : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    [[noreturn]] void ThrowArgumentOutOfRange()
    {
        throw std::out_of_range("Specified argument was out of the range of valid values.");
    }

    [[noreturn]] void ThrowArgumentNull()
    {
        throw std::invalid_argument("Value cannot be null.");
    }

    [[noreturn]] void ThrowOverflow()
    {
        throw std::overflow_error("Arithmetic operation resulted in an overflow.");
    }

    [[noreturn]] void ThrowInvalidCollectionAdapter()
    {
        throw std::logic_error("The enumerable does not expose ICollection<byte> semantics.");
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
        std::span<const std::uint8_t> span, std::int32_t offset, std::int32_t count)
    {
        if (offset < 0 || count < 0)
        {
            ThrowArgumentOutOfRange();
        }
        return Slice(
            span,
            static_cast<std::size_t>(offset),
            static_cast<std::size_t>(count));
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
        if (!source.empty())
        {
            std::memmove(destination.data(), source.data(), source.size());
        }
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

    [[nodiscard]] const std::shared_ptr<std::vector<std::uint8_t>>& EmptyDataStorage()
    {
        static const auto storage = std::make_shared<std::vector<std::uint8_t>>();
        return storage;
    }
}

namespace NCSFCommon::NC
{
    std::int32_t SSEQByteEnumerable::Count() const
    {
        std::unique_ptr<SSEQByteEnumerator> enumerator = GetEnumerator();
        if (enumerator == nullptr)
        {
            throw std::runtime_error("Object reference not set to an instance of an object.");
        }

        std::int32_t count = 0;
        while (enumerator->MoveNext())
        {
            if (count == std::numeric_limits<std::int32_t>::max())
            {
                ThrowOverflow();
            }
            ++count;
        }
        return count;
    }

    bool SSEQByteEnumerable::TryGetCollectionCount(std::int32_t& count) const
    {
        count = 0;
        return false;
    }

    void SSEQByteEnumerable::CopyCollectionTo(
        std::span<std::uint8_t> destination, std::int32_t index) const
    {
        (void)destination;
        (void)index;
        ThrowInvalidCollectionAdapter();
    }

    SSEQReadOnlyMemory::SSEQReadOnlyMemory(
        std::shared_ptr<const std::vector<std::uint8_t>> owner,
        std::size_t length) noexcept
        : _owner(std::move(owner)),
          _length(length)
    {
    }

    std::size_t SSEQReadOnlyMemory::Length() const noexcept
    {
        return _length;
    }

    bool SSEQReadOnlyMemory::IsEmpty() const noexcept
    {
        return _length == 0U;
    }

    std::span<const std::uint8_t> SSEQReadOnlyMemory::Span() const noexcept
    {
        if (_owner == nullptr)
        {
            return {};
        }
        return std::span<const std::uint8_t>(_owner->data(), _length);
    }

    const std::uint8_t& SSEQReadOnlyMemory::operator[](std::size_t index) const
    {
        if (index >= _length)
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        return (*_owner)[index];
    }

    SSEQ::SSEQ(
        std::optional<std::u16string> filename,
        std::optional<std::u16string> originalFilename)
        : _filename(std::move(filename)),
          _originalFilename(std::move(originalFilename)),
          _dataStorage(EmptyDataStorage())
    {
    }

    std::uint32_t SSEQ::Magic() const noexcept
    {
        return 0x0100FEFFU;
    }

    std::uint32_t SSEQ::FileSize() const noexcept
    {
        return Size();
    }

    std::uint16_t SSEQ::HeaderSize() const noexcept
    {
        return 0x10U;
    }

    std::uint16_t SSEQ::Blocks() const noexcept
    {
        return 1U;
    }

    const std::optional<std::u16string>& SSEQ::Filename() const noexcept
    {
        return _filename;
    }

    void SSEQ::Filename(std::optional<std::u16string> value)
    {
        _filename = std::move(value);
    }

    const std::optional<std::u16string>& SSEQ::OriginalFilename() const noexcept
    {
        return _originalFilename;
    }

    void SSEQ::OriginalFilename(std::optional<std::u16string> value)
    {
        _originalFilename = std::move(value);
    }

    SSEQReadOnlyMemory SSEQ::Data() const noexcept
    {
        return SSEQReadOnlyMemory(
            std::shared_ptr<const std::vector<std::uint8_t>>(_dataStorage),
            static_cast<std::size_t>(_dataCount));
    }

    std::int32_t SSEQ::EntryNumber() const noexcept
    {
        return _entryNumber;
    }

    void SSEQ::EntryNumber(std::int32_t value) noexcept
    {
        _entryNumber = value;
    }

    const std::shared_ptr<INFOEntrySEQ>& SSEQ::Info() const noexcept
    {
        return _info;
    }

    void SSEQ::Info(std::shared_ptr<INFOEntrySEQ> value) noexcept
    {
        _info = std::move(value);
    }

    std::uint32_t SSEQ::Size() const noexcept
    {
        return static_cast<std::uint32_t>(HeaderSize()) + DataSize();
    }

    std::uint32_t SSEQ::DataSize() const noexcept
    {
        return 0x0CU + static_cast<std::uint32_t>(_dataCount);
    }

    std::vector<std::uint8_t> SSEQ::ExpectedHeader() const
    {
        return {
            static_cast<std::uint8_t>('S'),
            static_cast<std::uint8_t>('S'),
            static_cast<std::uint8_t>('E'),
            static_cast<std::uint8_t>('Q')
        };
    }

    std::int32_t SSEQ::DataCapacity() const noexcept
    {
        return static_cast<std::int32_t>(_dataStorage->size());
    }

    std::span<std::uint8_t> SSEQ::DataSpan() noexcept
    {
        return std::span<std::uint8_t>(
            _dataStorage->data(),
            static_cast<std::size_t>(_dataCount));
    }

    std::span<const std::uint8_t> SSEQ::DataSpan() const noexcept
    {
        return std::span<const std::uint8_t>(
            _dataStorage->data(),
            static_cast<std::size_t>(_dataCount));
    }

    void SSEQ::GrowData(std::int32_t required)
    {
        if (required < 0)
        {
            ThrowArgumentOutOfRange();
        }

        constexpr std::uint32_t ArrayMaxLength = 0x7FFFFFC7U;
        const std::int32_t currentCapacity = DataCapacity();
        std::int32_t newCapacity = 4;
        if (currentCapacity != 0)
        {
            const std::uint32_t doubled = static_cast<std::uint32_t>(currentCapacity) * 2U;
            newCapacity = std::bit_cast<std::int32_t>(doubled);
        }
        if (static_cast<std::uint32_t>(newCapacity) > ArrayMaxLength)
        {
            newCapacity = std::bit_cast<std::int32_t>(ArrayMaxLength);
        }
        if (newCapacity < required)
        {
            newCapacity = required;
        }

        auto replacement = std::make_shared<std::vector<std::uint8_t>>(
            static_cast<std::size_t>(newCapacity), std::uint8_t{0});
        if (_dataCount != 0)
        {
            std::copy_n(
                _dataStorage->begin(),
                static_cast<std::size_t>(_dataCount),
                replacement->begin());
        }
        _dataStorage = std::move(replacement);
    }

    void SSEQ::SetDataCount(std::int32_t count)
    {
        if (count < 0)
        {
            ThrowArgumentOutOfRange();
        }
        if (count > DataCapacity())
        {
            GrowData(count);
        }
        _dataCount = count;
    }

    void SSEQ::ClearData() noexcept
    {
        _dataCount = 0;
    }

    void SSEQ::EnsureDataCapacity(std::int32_t capacity)
    {
        if (capacity < 0)
        {
            ThrowArgumentOutOfRange();
        }
        if (capacity > DataCapacity())
        {
            GrowData(capacity);
        }
    }

    void SSEQ::AddData(std::uint8_t value)
    {
        if (_dataCount == std::numeric_limits<std::int32_t>::max())
        {
            throw std::bad_alloc();
        }
        if (_dataCount == DataCapacity())
        {
            GrowData(_dataCount + 1);
        }
        (*_dataStorage)[static_cast<std::size_t>(_dataCount)] = value;
        ++_dataCount;
    }

    void SSEQ::AddDataRange(const SSEQByteEnumerable& source)
    {
        std::int32_t collectionCount = 0;
        if (source.TryGetCollectionCount(collectionCount))
        {
            if (collectionCount > 0)
            {
                const std::int32_t oldCount = _dataCount;
                if (collectionCount > std::numeric_limits<std::int32_t>::max() - oldCount)
                {
                    ThrowOverflow();
                }
                EnsureDataCapacity(oldCount + collectionCount);
                source.CopyCollectionTo(
                    std::span<std::uint8_t>(_dataStorage->data(), _dataStorage->size()),
                    oldCount);
                _dataCount = oldCount + collectionCount;
            }
            return;
        }

        std::unique_ptr<SSEQByteEnumerator> enumerator = source.GetEnumerator();
        if (enumerator == nullptr)
        {
            throw std::runtime_error("Object reference not set to an instance of an object.");
        }
        while (enumerator->MoveNext())
        {
            AddData(enumerator->Current());
        }
    }

    void SSEQ::Read(std::span<const std::uint8_t> span, bool failOnMissingFile)
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

        const std::uint32_t size = ReadUInt32LittleEndian(Slice(span, 0x14U));
        const std::uint32_t dataLengthBits = size - 0x0CU;
        const std::int32_t dataLength = ToInt32Unchecked(dataLengthBits);
        SetDataCount(dataLength);

        const std::uint32_t dataOffsetBits = ReadUInt32LittleEndian(Slice(span, 0x18U));
        const std::int32_t dataOffset = ToInt32Unchecked(dataOffsetBits);
        CopyTo(Slice(span, dataOffset, dataLength), DataSpan());
    }

    void SSEQ::Write(std::span<std::uint8_t> span)
    {
        NDSStandardHeader::Write(span);
        CopyTo(Common::DataBytes.Span(), Slice(span, 0x10U));
        WriteUInt32LittleEndian(Slice(span, 0x14U), DataSize());
        WriteUInt32LittleEndian(Slice(span, 0x18U), 0x1CU);
        CopyTo(DataSpan(), Slice(span, 0x1CU));
    }

    void SSEQ::ReplaceData(const SSEQByteEnumerable* newData)
    {
        ClearData();
        if (newData == nullptr)
        {
            ThrowArgumentNull();
        }

        std::int32_t count = 0;
        if (!newData->TryGetNonEnumeratedCount(count))
        {
            count = newData->Count();
        }
        EnsureDataCapacity(count);
        AddDataRange(*newData);
    }

    bool SSEQ::Equals(const SSEQ* other) const noexcept
    {
        return other != nullptr
            && DataSize() == other->DataSize()
            && _dataCount == other->_dataCount
            && std::equal(DataSpan().begin(), DataSpan().end(), other->DataSpan().begin(), other->DataSpan().end());
    }

    bool SSEQ::Equals(const std::any& obj) const noexcept
    {
        if (const auto* other = std::any_cast<SSEQ*>(&obj))
        {
            return Equals(*other);
        }
        if (const auto* other = std::any_cast<const SSEQ*>(&obj))
        {
            return Equals(*other);
        }
        return false;
    }

    std::int32_t SSEQ::GetHashCode() const
    {
        const std::int32_t dataSizeHash = std::bit_cast<std::int32_t>(DataSize());
        const std::int32_t listHash = IdentityHashCode(&_dataStorage);
        return CombineHashCodes(dataSizeHash, listHash);
    }

    bool SSEQ::EqualityOperator(const SSEQ* left, const SSEQ* right) noexcept
    {
        return left != nullptr && left->Equals(right);
    }

    bool SSEQ::InequalityOperator(const SSEQ* left, const SSEQ* right) noexcept
    {
        return !EqualityOperator(left, right);
    }

    bool operator==(const SSEQ& left, const SSEQ& right) noexcept
    {
        return left.Equals(&right);
    }

    bool operator!=(const SSEQ& left, const SSEQ& right) noexcept
    {
        return !(left == right);
    }
}
