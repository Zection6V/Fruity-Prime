#include "SWAV.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
    constexpr std::uint32_t Prime1 = 2654435761U;
    constexpr std::uint32_t Prime2 = 2246822519U;
    constexpr std::uint32_t Prime3 = 3266489917U;
    constexpr std::uint32_t Prime4 = 668265263U;
    constexpr std::int32_t ArrayMaxLength = 0x7FFFFFC7;
    constexpr std::int32_t DefaultListCapacity = 4;

    [[noreturn]] void ThrowIndexOutOfRange()
    {
        throw std::out_of_range("Index was outside the bounds of the array.");
    }

    [[noreturn]] void ThrowArgumentOutOfRange()
    {
        throw std::out_of_range("Specified argument was out of the range of valid values.");
    }

    [[noreturn]] void ThrowDestinationTooShort()
    {
        throw std::invalid_argument("Destination is too short.");
    }

    [[nodiscard]] std::int32_t ToInt32Unchecked(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::uint8_t& At(std::span<std::uint8_t> span, std::size_t index)
    {
        if (index >= span.size())
        {
            ThrowIndexOutOfRange();
        }
        return span[index];
    }

    [[nodiscard]] std::uint8_t At(std::span<const std::uint8_t> span, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= span.size())
        {
            ThrowIndexOutOfRange();
        }
        return span[static_cast<std::size_t>(index)];
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

    [[nodiscard]] std::span<const std::uint8_t> Slice(
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

    [[nodiscard]] std::uint16_t ReadUInt16LittleEndian(std::span<const std::uint8_t> span)
    {
        if (span.size() < sizeof(std::uint16_t))
        {
            ThrowArgumentOutOfRange();
        }
        return static_cast<std::uint16_t>(static_cast<std::uint16_t>(span[0])
            | static_cast<std::uint16_t>(static_cast<std::uint16_t>(span[1]) << 8U));
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

    void WriteUInt16LittleEndian(std::span<std::uint8_t> span, std::uint16_t value)
    {
        if (span.size() < sizeof(std::uint16_t))
        {
            ThrowArgumentOutOfRange();
        }
        span[0] = static_cast<std::uint8_t>(value);
        span[1] = static_cast<std::uint8_t>(value >> 8U);
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
            ThrowDestinationTooShort();
        }
        if (!source.empty())
        {
            std::memmove(destination.data(), source.data(), source.size());
        }
    }

    [[nodiscard]] std::int32_t GetNewListCapacity(
        std::int32_t currentCapacity, std::int32_t requestedCapacity) noexcept
    {
        std::int32_t newCapacity;
        if (currentCapacity == 0)
        {
            newCapacity = DefaultListCapacity;
        }
        else
        {
            const std::uint32_t doubled =
                static_cast<std::uint32_t>(currentCapacity) * 2U;
            newCapacity = ToInt32Unchecked(doubled);
        }

        if (static_cast<std::uint32_t>(newCapacity)
            > static_cast<std::uint32_t>(ArrayMaxLength))
        {
            newCapacity = ArrayMaxLength;
        }
        if (newCapacity < requestedCapacity)
        {
            newCapacity = requestedCapacity;
        }
        return newCapacity;
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

    [[nodiscard]] std::uint32_t Round(std::uint32_t hash, std::uint32_t input) noexcept
    {
        return std::rotl(hash + input * Prime2, 13) * Prime1;
    }

    [[nodiscard]] std::uint32_t QueueRound(
        std::uint32_t hash, std::uint32_t queuedValue) noexcept
    {
        return std::rotl(hash + queuedValue * Prime3, 17) * Prime4;
    }

    [[nodiscard]] std::uint32_t MixState(
        std::uint32_t v1, std::uint32_t v2, std::uint32_t v3, std::uint32_t v4) noexcept
    {
        return std::rotl(v1, 1) + std::rotl(v2, 7) + std::rotl(v3, 12) + std::rotl(v4, 18);
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

    [[nodiscard]] std::int32_t CombineHashCodes(
        std::int32_t value1,
        std::int32_t value2,
        std::int32_t value3,
        std::int32_t value4,
        std::int32_t value5,
        std::int32_t value6,
        std::int32_t value7)
    {
        const std::uint32_t seed = GlobalHashSeed();
        std::uint32_t v1 = seed + Prime1 + Prime2;
        std::uint32_t v2 = seed + Prime2;
        std::uint32_t v3 = seed;
        std::uint32_t v4 = seed - Prime1;

        v1 = Round(v1, static_cast<std::uint32_t>(value1));
        v2 = Round(v2, static_cast<std::uint32_t>(value2));
        v3 = Round(v3, static_cast<std::uint32_t>(value3));
        v4 = Round(v4, static_cast<std::uint32_t>(value4));

        std::uint32_t hash = MixState(v1, v2, v3, v4);
        hash += 28U;
        hash = QueueRound(hash, static_cast<std::uint32_t>(value5));
        hash = QueueRound(hash, static_cast<std::uint32_t>(value6));
        hash = QueueRound(hash, static_cast<std::uint32_t>(value7));
        return std::bit_cast<std::int32_t>(MixFinal(hash));
    }

    // List<T> inherits object.GetHashCode(), so HashCode.Combine receives the
    // list object's identity, not a hash of the list contents.
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
    std::uint8_t SWAV::WaveType() const noexcept
    {
        return _waveType;
    }

    void SWAV::WaveType(std::uint8_t value) noexcept
    {
        _waveType = value;
    }

    std::uint8_t SWAV::Loop() const noexcept
    {
        return _loop;
    }

    void SWAV::Loop(std::uint8_t value) noexcept
    {
        _loop = value;
    }

    std::uint16_t SWAV::SampleRate() const noexcept
    {
        return _sampleRate;
    }

    void SWAV::SampleRate(std::uint16_t value) noexcept
    {
        _sampleRate = value;
    }

    std::uint16_t SWAV::Time() const noexcept
    {
        return _time;
    }

    void SWAV::Time(std::uint16_t value) noexcept
    {
        _time = value;
    }

    std::uint16_t SWAV::OriginalLoopOffset() const noexcept
    {
        return _originalLoopOffset;
    }

    void SWAV::OriginalLoopOffset(std::uint16_t value) noexcept
    {
        _originalLoopOffset = value;
    }

    std::uint32_t SWAV::LoopOffset() const noexcept
    {
        return _loopOffset;
    }

    void SWAV::LoopOffset(std::uint32_t value) noexcept
    {
        _loopOffset = value;
    }

    std::uint32_t SWAV::OriginalLoopLength() const noexcept
    {
        return _originalLoopLength;
    }

    void SWAV::OriginalLoopLength(std::uint32_t value) noexcept
    {
        _originalLoopLength = value;
    }

    std::uint32_t SWAV::LoopLength() const noexcept
    {
        return _loopLength;
    }

    void SWAV::LoopLength(std::uint32_t value) noexcept
    {
        _loopLength = value;
    }

    std::span<const std::uint8_t> SWAV::OriginalData() const noexcept
    {
        return std::span<const std::uint8_t>(
            _originalDataStorage.data(), static_cast<std::size_t>(_originalDataCount));
    }

    std::span<const float> SWAV::Data() const noexcept
    {
        return std::span<const float>(
            _dataStorage.data(), static_cast<std::size_t>(_dataCount));
    }

    std::uint32_t SWAV::Size() const noexcept
    {
        return 0x0CU + static_cast<std::uint32_t>(_originalDataCount);
    }

    void SWAV::SetOriginalDataCount(std::int32_t count)
    {
        if (count < 0)
        {
            ThrowArgumentOutOfRange();
        }

        const std::int32_t currentCapacity =
            static_cast<std::int32_t>(_originalDataStorage.size());
        if (count > currentCapacity)
        {
            const std::int32_t newCapacity = GetNewListCapacity(currentCapacity, count);
            if (newCapacity > ArrayMaxLength)
            {
                throw std::bad_alloc();
            }
            std::vector<std::uint8_t> newStorage(
                static_cast<std::size_t>(newCapacity), std::uint8_t{0});
            if (_originalDataCount > 0)
            {
                std::copy_n(
                    _originalDataStorage.begin(),
                    static_cast<std::size_t>(_originalDataCount),
                    newStorage.begin());
            }
            _originalDataStorage.swap(newStorage);
        }

        // byte is a non-reference value type, so shrinking does not clear slots.
        _originalDataCount = count;
    }

    void SWAV::EnsureDataCapacity(std::int32_t capacity)
    {
        if (capacity < 0)
        {
            ThrowArgumentOutOfRange();
        }

        const std::int32_t currentCapacity = static_cast<std::int32_t>(_dataStorage.size());
        if (capacity > currentCapacity)
        {
            const std::int32_t newCapacity = GetNewListCapacity(currentCapacity, capacity);
            if (newCapacity > ArrayMaxLength)
            {
                throw std::bad_alloc();
            }
            std::vector<float> newStorage(static_cast<std::size_t>(newCapacity), 0.0F);
            if (_dataCount > 0)
            {
                std::copy_n(
                    _dataStorage.begin(), static_cast<std::size_t>(_dataCount), newStorage.begin());
            }
            _dataStorage.swap(newStorage);
        }
    }

    void SWAV::ClearData() noexcept
    {
        // float is a non-reference value type, so List<float>.Clear only sets Count to zero.
        _dataCount = 0;
    }

    void SWAV::AddData(float value)
    {
        const std::int32_t currentCapacity = static_cast<std::int32_t>(_dataStorage.size());
        if (_dataCount == currentCapacity)
        {
            const std::int32_t requestedCapacity = ToInt32Unchecked(
                static_cast<std::uint32_t>(_dataCount) + 1U);
            EnsureDataCapacity(requestedCapacity);
        }
        _dataStorage[static_cast<std::size_t>(_dataCount)] = value;
        ++_dataCount;
    }

    void SWAV::DecodeADPCMNibble(
        std::int32_t nibble, std::int32_t& stepIndex, std::int32_t& predictedValue)
    {
        if (stepIndex < 0 || stepIndex >= static_cast<std::int32_t>(IMAStepTable.size()))
        {
            ThrowIndexOutOfRange();
        }
        const std::int32_t step = IMAStepTable[static_cast<std::size_t>(stepIndex)];

        if (nibble < 0 || nibble >= static_cast<std::int32_t>(IMAIndexTable.size()))
        {
            ThrowIndexOutOfRange();
        }
        stepIndex += IMAIndexTable[static_cast<std::size_t>(nibble)];
        stepIndex = std::clamp(stepIndex, 0, 88);

        std::int32_t diff = step >> 3;
        if ((nibble & 1) != 0)
        {
            diff += step >> 2;
        }
        if ((nibble & 2) != 0)
        {
            diff += step >> 1;
        }
        if ((nibble & 4) != 0)
        {
            diff += step;
        }

        predictedValue = (nibble & 8) == 0
            ? std::min(predictedValue + diff, 0x7FFF)
            : std::max(predictedValue - diff, -0x7FFF);
    }

    void SWAV::DecodeADPCM(std::uint32_t len)
    {
        const std::span<const std::uint8_t> originalSpan = OriginalData();
        std::int32_t predictedValue = static_cast<std::int32_t>(ReadUInt16LittleEndian(originalSpan));
        std::int32_t stepIndex = static_cast<std::int32_t>(
            ReadUInt16LittleEndian(Slice(originalSpan, 0x02U)));

        for (std::uint32_t i = 0; i < len; ++i)
        {
            const std::int32_t dataIndex = ToInt32Unchecked(i + 4U);
            const std::uint8_t encoded = At(originalSpan, dataIndex);

            DecodeADPCMNibble(
                static_cast<std::int32_t>(encoded & 0x0FU), stepIndex, predictedValue);
            AddData(static_cast<float>(predictedValue) / static_cast<float>(std::numeric_limits<std::int16_t>::max()));

            DecodeADPCMNibble(
                static_cast<std::int32_t>((encoded >> 4U) & 0x0FU), stepIndex, predictedValue);
            AddData(static_cast<float>(predictedValue) / static_cast<float>(std::numeric_limits<std::int16_t>::max()));
        }
    }

    SWAV& SWAV::Read(std::span<const std::uint8_t> span)
    {
        _waveType = At(span, 0x00U);
        _loop = At(span, 0x01U);
        _sampleRate = ReadUInt16LittleEndian(Slice(span, 0x02U));
        _time = ReadUInt16LittleEndian(Slice(span, 0x04U));

        const std::uint16_t originalLoopOffset = ReadUInt16LittleEndian(Slice(span, 0x06U));
        _originalLoopOffset = originalLoopOffset;
        _loopOffset = originalLoopOffset;

        const std::uint32_t originalLoopLength = ReadUInt32LittleEndian(Slice(span, 0x08U));
        _originalLoopLength = originalLoopLength;
        _loopLength = originalLoopLength;

        const std::uint32_t size = (_loopOffset + _loopLength) * 4U;
        const std::int32_t signedSize = ToInt32Unchecked(size);
        SetOriginalDataCount(signedSize);

        const std::span<const std::uint8_t> source = Slice(span, 0x0C, signedSize);
        CopyTo(source, std::span<std::uint8_t>(
            _originalDataStorage.data(), static_cast<std::size_t>(_originalDataCount)));

        ClearData();
        switch (_waveType)
        {
        case 0U:
            EnsureDataCapacity(signedSize);
            for (std::int32_t i = 0; i < _originalDataCount; ++i)
            {
                const auto value = std::bit_cast<std::int8_t>(
                    _originalDataStorage[static_cast<std::size_t>(i)]);
                AddData(static_cast<float>(value)
                    / static_cast<float>(std::numeric_limits<std::int8_t>::max()));
            }
            _loopOffset *= 4U;
            _loopLength *= 4U;
            break;

        case 1U:
        {
            const std::uint32_t sampleCount = size / 2U;
            EnsureDataCapacity(ToInt32Unchecked(sampleCount));
            for (std::uint32_t pos = 0; pos < size; pos += 2U)
            {
                std::int16_t value;
                std::memcpy(
                    std::addressof(value),
                    _originalDataStorage.data() + static_cast<std::size_t>(pos),
                    sizeof(value));
                AddData(static_cast<float>(value)
                    / static_cast<float>(std::numeric_limits<std::int16_t>::max()));
            }
            _loopOffset *= 2U;
            _loopLength *= 2U;
            break;
        }

        case 2U:
        {
            const std::uint32_t decodedCapacity = (size - 4U) * 2U;
            EnsureDataCapacity(ToInt32Unchecked(decodedCapacity));
            DecodeADPCM(size - 4U);
            if (_loopOffset != 0U)
            {
                --_loopOffset;
            }
            _loopOffset *= 8U;
            _loopLength *= 8U;
            break;
        }

        default:
            break;
        }

        return *this;
    }

    void SWAV::Write(std::span<std::uint8_t> span)
    {
        At(span, 0x00U) = _waveType;
        At(span, 0x01U) = _loop;
        WriteUInt16LittleEndian(Slice(span, 0x02U), _sampleRate);
        WriteUInt16LittleEndian(Slice(span, 0x04U), _time);
        WriteUInt16LittleEndian(Slice(span, 0x06U), _originalLoopOffset);
        WriteUInt32LittleEndian(Slice(span, 0x08U), _originalLoopLength);
        CopyTo(OriginalData(), Slice(span, 0x0CU));
    }

    bool SWAV::Equals(const SWAV* other) const noexcept
    {
        if (other == nullptr
            || _waveType != other->_waveType
            || _loop != other->_loop
            || _sampleRate != other->_sampleRate
            || _time != other->_time
            || _originalLoopOffset != other->_originalLoopOffset
            || _originalLoopLength != other->_originalLoopLength
            || _originalDataCount != other->_originalDataCount)
        {
            return false;
        }

        return std::equal(
            _originalDataStorage.begin(),
            _originalDataStorage.begin() + _originalDataCount,
            other->_originalDataStorage.begin());
    }

    std::int32_t SWAV::GetHashCode() const
    {
        return CombineHashCodes(
            static_cast<std::int32_t>(_waveType),
            static_cast<std::int32_t>(_loop),
            static_cast<std::int32_t>(_sampleRate),
            static_cast<std::int32_t>(_time),
            static_cast<std::int32_t>(_originalLoopOffset),
            ToInt32Unchecked(_originalLoopLength),
            IdentityHashCode(std::addressof(_originalDataStorage)));
    }

    // Preserve left?.Equals(right) ?? false, including null == null being false.
    bool SWAV::OpEquality(const SWAV* left, const SWAV* right) noexcept
    {
        return left != nullptr && left->Equals(right);
    }

    bool SWAV::OpInequality(const SWAV* left, const SWAV* right) noexcept
    {
        return !OpEquality(left, right);
    }

    bool operator==(const SWAV& left, const SWAV& right) noexcept
    {
        return left.Equals(std::addressof(right));
    }

    bool operator!=(const SWAV& left, const SWAV& right) noexcept
    {
        return !(left == right);
    }
}
