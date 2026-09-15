#include "INFOEntryPLAYER.hpp"

#include <bit>
#include <charconv>
#include <cstddef>
#include <random>
#include <stdexcept>

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

    [[nodiscard]] std::uint8_t ReadByte(
        std::span<const std::uint8_t> span, std::size_t index)
    {
        if (index >= span.size())
        {
            ThrowIndexOutOfRange();
        }
        return span[index];
    }

    void WriteByte(std::span<std::uint8_t> span, std::size_t index, std::uint8_t value)
    {
        if (index >= span.size())
        {
            ThrowIndexOutOfRange();
        }
        span[index] = value;
    }

    [[nodiscard]] std::uint16_t ReadUInt16LittleEndian(std::span<const std::uint8_t> span)
    {
        if (span.size() < sizeof(std::uint16_t))
        {
            ThrowArgumentOutOfRange();
        }
        return static_cast<std::uint16_t>(span[0])
            | static_cast<std::uint16_t>(static_cast<std::uint16_t>(span[1]) << 8U);
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

    template <typename T>
    void AppendUnsigned(std::u16string& destination, T value)
    {
        char buffer[32];
        const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
        if (error != std::errc{})
        {
            throw std::runtime_error("Failed to format unsigned integer.");
        }
        for (const char* current = buffer; current != end; ++current)
        {
            destination.push_back(static_cast<char16_t>(*current));
        }
    }

    void AppendHex4(std::u16string& destination, std::uint16_t value)
    {
        constexpr char16_t digits[] = u"0123456789ABCDEF";
        destination.push_back(digits[(value >> 12U) & 0xFU]);
        destination.push_back(digits[(value >> 8U) & 0xFU]);
        destination.push_back(digits[(value >> 4U) & 0xFU]);
        destination.push_back(digits[value & 0xFU]);
    }

    std::uint32_t GlobalHashSeed()
    {
        static const std::uint32_t seed = []
        {
            std::random_device device;
            std::uniform_int_distribution<std::uint32_t> distribution;
            return distribution(device);
        }();
        return seed;
    }

    std::uint32_t QueueRound(std::uint32_t hash, std::uint32_t queuedValue) noexcept
    {
        return std::rotl(hash + queuedValue * Prime3, 17) * Prime4;
    }

    std::uint32_t MixFinal(std::uint32_t hash) noexcept
    {
        hash ^= hash >> 15U;
        hash *= Prime2;
        hash ^= hash >> 13U;
        hash *= Prime3;
        hash ^= hash >> 16U;
        return hash;
    }

    std::int32_t CombineHashCodes(
        std::uint8_t value1,
        std::uint8_t value2,
        std::uint16_t value3,
        std::uint32_t value4)
    {
        std::uint32_t hash = GlobalHashSeed() + Prime5;
        hash += 16U;
        hash = QueueRound(hash, value1);
        hash = QueueRound(hash, value2);
        hash = QueueRound(hash, value3);
        hash = QueueRound(hash, value4);
        return std::bit_cast<std::int32_t>(MixFinal(hash));
    }
}

namespace NCSFCommon::NC
{
    INFOEntryPLAYER::INFOEntryPLAYER(const INFOEntryPLAYER* other)
        : INFOEntry(other)
    {
        _maxSequences = other->_maxSequences;
        _padding = other->_padding;
        _channelMask = other->_channelMask;
        _heapSize = other->_heapSize;
    }

    std::uint8_t INFOEntryPLAYER::MaxSequences() const noexcept
    {
        return _maxSequences;
    }

    void INFOEntryPLAYER::MaxSequences(std::uint8_t value) noexcept
    {
        _maxSequences = value;
    }

    std::uint8_t INFOEntryPLAYER::Padding() const noexcept
    {
        return _padding;
    }

    void INFOEntryPLAYER::Padding(std::uint8_t value) noexcept
    {
        _padding = value;
    }

    std::uint16_t INFOEntryPLAYER::ChannelMask() const noexcept
    {
        return _channelMask;
    }

    void INFOEntryPLAYER::ChannelMask(std::uint16_t value) noexcept
    {
        _channelMask = value;
    }

    std::uint32_t INFOEntryPLAYER::HeapSize() const noexcept
    {
        return _heapSize;
    }

    void INFOEntryPLAYER::HeapSize(std::uint32_t value) noexcept
    {
        _heapSize = value;
    }

    std::uint32_t INFOEntryPLAYER::Size() const noexcept
    {
        return 0x08U;
    }

    INFOEntryPLAYER* INFOEntryPLAYER::Read(std::span<const std::uint8_t> span)
    {
        _maxSequences = ReadByte(span, 0x00U);
        _padding = ReadByte(span, 0x01U);
        _channelMask = ReadUInt16LittleEndian(Slice(span, 0x02U));
        _heapSize = ReadUInt32LittleEndian(Slice(span, 0x04U));
        return this;
    }

    void INFOEntryPLAYER::Write(std::span<std::uint8_t> span)
    {
        WriteByte(span, 0x00U, _maxSequences);
        WriteByte(span, 0x01U, _padding);
        WriteUInt16LittleEndian(Slice(span, 0x02U), _channelMask);
        WriteUInt32LittleEndian(Slice(span, 0x04U), _heapSize);
    }

    bool INFOEntryPLAYER::Equals(const INFOEntryPLAYER* other) const noexcept
    {
        return other != nullptr && _maxSequences == other->_maxSequences
            && _padding == other->_padding && _channelMask == other->_channelMask
            && _heapSize == other->_heapSize;
    }

    bool INFOEntryPLAYER::Equals(const std::any& obj) const
    {
        if (const auto* other = std::any_cast<INFOEntryPLAYER*>(&obj))
        {
            return Equals(*other);
        }
        if (const auto* other = std::any_cast<const INFOEntryPLAYER*>(&obj))
        {
            return Equals(*other);
        }
        if (const auto* other = std::any_cast<INFOEntry*>(&obj))
        {
            return Equals(dynamic_cast<INFOEntryPLAYER*>(*other));
        }
        if (const auto* other = std::any_cast<const INFOEntry*>(&obj))
        {
            return Equals(dynamic_cast<const INFOEntryPLAYER*>(*other));
        }
        return false;
    }

    std::int32_t INFOEntryPLAYER::GetHashCode() const
    {
        return CombineHashCodes(_maxSequences, _padding, _channelMask, _heapSize);
    }

    bool INFOEntryPLAYER::EqualityOperator(
        const INFOEntryPLAYER* left, const INFOEntryPLAYER* right) noexcept
    {
        return left != nullptr && left->Equals(right);
    }

    bool INFOEntryPLAYER::InequalityOperator(
        const INFOEntryPLAYER* left, const INFOEntryPLAYER* right) noexcept
    {
        return !EqualityOperator(left, right);
    }

    std::u16string INFOEntryPLAYER::DebuggerDisplay() const
    {
        std::u16string result = u"INFO Entry (PLAYER) - ";
        result += INFOEntry::DebuggerDisplay();
        result += u"Max Sequences: ";
        AppendUnsigned(result, _maxSequences);
        result += u", Channel Mask: 0x";
        AppendHex4(result, _channelMask);
        result += u", HeapSize: ";
        AppendUnsigned(result, _heapSize);
        return result;
    }

    bool operator==(const INFOEntryPLAYER& left, const INFOEntryPLAYER& right) noexcept
    {
        return INFOEntryPLAYER::EqualityOperator(&left, &right);
    }

    bool operator!=(const INFOEntryPLAYER& left, const INFOEntryPLAYER& right) noexcept
    {
        return INFOEntryPLAYER::InequalityOperator(&left, &right);
    }
}
