#include "INFOEntrySEQ.hpp"

#include "SSEQ.hpp"

#include <charconv>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace
{
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
}

namespace NCSFCommon::NC
{
    INFOEntrySEQ::INFOEntrySEQ(const INFOEntrySEQ* other)
        : INFOEntry(other)
    {
        _fileID = other->_fileID;
        _bank = other->_bank;
        _volume = other->_volume;
        _channelPriority = other->_channelPriority;
        _playerPriority = other->_playerPriority;
        _player = other->_player;
        _reserved = other->_reserved;
        _sseq = other->_sseq;
    }

    std::uint32_t INFOEntrySEQ::FileID() const noexcept
    {
        return _fileID;
    }

    void INFOEntrySEQ::FileID(std::uint32_t value) noexcept
    {
        _fileID = value;
    }

    std::uint16_t INFOEntrySEQ::Bank() const noexcept
    {
        return _bank;
    }

    void INFOEntrySEQ::Bank(std::uint16_t value) noexcept
    {
        _bank = value;
    }

    std::uint8_t INFOEntrySEQ::Volume() const noexcept
    {
        return _volume;
    }

    void INFOEntrySEQ::Volume(std::uint8_t value) noexcept
    {
        _volume = value;
    }

    std::uint8_t INFOEntrySEQ::ChannelPriority() const noexcept
    {
        return _channelPriority;
    }

    void INFOEntrySEQ::ChannelPriority(std::uint8_t value) noexcept
    {
        _channelPriority = value;
    }

    std::uint8_t INFOEntrySEQ::PlayerPriority() const noexcept
    {
        return _playerPriority;
    }

    void INFOEntrySEQ::PlayerPriority(std::uint8_t value) noexcept
    {
        _playerPriority = value;
    }

    std::uint8_t INFOEntrySEQ::Player() const noexcept
    {
        return _player;
    }

    void INFOEntrySEQ::Player(std::uint8_t value) noexcept
    {
        _player = value;
    }

    std::uint16_t INFOEntrySEQ::Reserved() const noexcept
    {
        return _reserved;
    }

    void INFOEntrySEQ::Reserved(std::uint16_t value) noexcept
    {
        _reserved = value;
    }

    std::shared_ptr<NCSFCommon::NC::SSEQ> INFOEntrySEQ::SSEQ() const noexcept
    {
        return _sseq;
    }

    void INFOEntrySEQ::SSEQ(std::shared_ptr<NCSFCommon::NC::SSEQ> value) noexcept
    {
        _sseq = std::move(value);
    }

    std::uint32_t INFOEntrySEQ::Size() const noexcept
    {
        return 0x0CU;
    }

    INFOEntrySEQ* INFOEntrySEQ::Read(std::span<const std::uint8_t> span)
    {
        _fileID = ReadUInt32LittleEndian(span);
        _bank = ReadUInt16LittleEndian(Slice(span, 0x04U));
        _volume = ReadByte(span, 0x06U);
        _channelPriority = ReadByte(span, 0x07U);
        _playerPriority = ReadByte(span, 0x08U);
        _player = ReadByte(span, 0x09U);
        _reserved = ReadUInt16LittleEndian(Slice(span, 0x0AU));
        return this;
    }

    void INFOEntrySEQ::Write(std::span<std::uint8_t> span)
    {
        WriteUInt32LittleEndian(span, _fileID);
        WriteUInt16LittleEndian(Slice(span, 0x04U), _bank);
        WriteByte(span, 0x06U, _volume);
        WriteByte(span, 0x07U, _channelPriority);
        WriteByte(span, 0x08U, _playerPriority);
        WriteByte(span, 0x09U, _player);
        WriteUInt16LittleEndian(Slice(span, 0x0AU), _reserved);
    }

    bool INFOEntrySEQ::FileEquals(const INFOEntrySEQ* other) const
    {
        return other != nullptr
            && NCSFCommon::NC::SSEQ::EqualityOperator(_sseq.get(), other->_sseq.get());
    }

    std::u16string INFOEntrySEQ::DebuggerDisplay() const
    {
        std::u16string result = u"INFO Entry (SEQ) - ";
        result += INFOEntry::DebuggerDisplay();
        result += u"File ID: ";
        AppendUnsigned(result, _fileID);
        result += u", Bank: ";
        AppendUnsigned(result, _bank);
        result += u", Volume: ";
        AppendUnsigned(result, _volume);
        result += u", Channel Priority: ";
        AppendUnsigned(result, _channelPriority);
        result += u", Player Priority: ";
        AppendUnsigned(result, _playerPriority);
        result += u", Player: ";
        AppendUnsigned(result, _player);
        return result;
    }
}
