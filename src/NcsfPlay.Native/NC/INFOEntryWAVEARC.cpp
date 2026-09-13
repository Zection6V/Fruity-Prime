#include "INFOEntryWAVEARC.hpp"

#include <bit>
#include <charconv>
#include <functional>
#include <stdexcept>
#include <utility>

namespace
{
    [[noreturn]] void ThrowArgumentOutOfRange()
    {
        throw std::out_of_range("Specified argument was out of the range of valid values.");
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

    void AppendUnsigned(std::u16string& destination, std::uint32_t value)
    {
        char buffer[16];
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
    INFOEntryWAVEARC::INFOEntryWAVEARC(const INFOEntryWAVEARC* other)
        : INFOEntry(other)
    {
        _fileID = other->_fileID;
        _flags = other->_flags;
        _swar = other->_swar;
    }

    std::uint32_t INFOEntryWAVEARC::FileID() const noexcept
    {
        return _fileID;
    }

    void INFOEntryWAVEARC::FileID(std::uint32_t value) noexcept
    {
        _fileID = value;
    }

    std::uint8_t INFOEntryWAVEARC::Flags() const noexcept
    {
        return _flags;
    }

    void INFOEntryWAVEARC::Flags(std::uint8_t value) noexcept
    {
        _flags = value;
    }

    const std::shared_ptr<NCSFCommon::NC::SWAR>& INFOEntryWAVEARC::SWAR() const noexcept
    {
        return _swar;
    }

    void INFOEntryWAVEARC::SWAR(std::shared_ptr<NCSFCommon::NC::SWAR> value) noexcept
    {
        _swar = std::move(value);
    }

    std::uint32_t INFOEntryWAVEARC::Size() const noexcept
    {
        return 0x04U;
    }

    INFOEntryWAVEARC* INFOEntryWAVEARC::Read(std::span<const std::uint8_t> span)
    {
        const std::uint32_t value = ReadUInt32LittleEndian(span);
        _fileID = value & 0x00FFFFFFU;
        _flags = span[0x03U];
        return this;
    }

    void INFOEntryWAVEARC::Write(std::span<std::uint8_t> span)
    {
        WriteUInt32LittleEndian(span, _fileID | (static_cast<std::uint32_t>(_flags) << 24U));
    }

    bool INFOEntryWAVEARC::Equals(const INFOEntryWAVEARC* other) const noexcept
    {
        return other != nullptr && _swar != nullptr && _swar.get() == other->_swar.get();
    }

    bool INFOEntryWAVEARC::Equals(const std::any& obj) const noexcept
    {
        if (const auto* other = std::any_cast<INFOEntryWAVEARC*>(&obj))
        {
            return Equals(*other);
        }
        if (const auto* other = std::any_cast<const INFOEntryWAVEARC*>(&obj))
        {
            return Equals(*other);
        }
        if (const auto* other = std::any_cast<INFOEntry*>(&obj))
        {
            return Equals(dynamic_cast<INFOEntryWAVEARC*>(*other));
        }
        if (const auto* other = std::any_cast<const INFOEntry*>(&obj))
        {
            return Equals(dynamic_cast<const INFOEntryWAVEARC*>(*other));
        }
        return false;
    }

    std::int32_t INFOEntryWAVEARC::GetHashCode() const
    {
        if (_swar == nullptr)
        {
            return 0;
        }
        const auto hash = static_cast<std::uint32_t>(std::hash<const NCSFCommon::NC::SWAR*>{}(_swar.get()));
        return std::bit_cast<std::int32_t>(hash);
    }

    bool INFOEntryWAVEARC::EqualityOperator(
        const INFOEntryWAVEARC* left, const INFOEntryWAVEARC* right) noexcept
    {
        return left != nullptr && left->Equals(right);
    }

    bool INFOEntryWAVEARC::InequalityOperator(
        const INFOEntryWAVEARC* left, const INFOEntryWAVEARC* right) noexcept
    {
        return !EqualityOperator(left, right);
    }

    std::u16string INFOEntryWAVEARC::DebuggerDisplay() const
    {
        std::u16string result = u"INFO Entry (WAVEARC) - ";
        result += INFOEntry::DebuggerDisplay();
        result += u"File ID: ";
        AppendUnsigned(result, _fileID);
        return result;
    }

    bool operator==(
        const INFOEntryWAVEARC& left, const INFOEntryWAVEARC& right) noexcept
    {
        return INFOEntryWAVEARC::EqualityOperator(&left, &right);
    }

    bool operator!=(
        const INFOEntryWAVEARC& left, const INFOEntryWAVEARC& right) noexcept
    {
        return INFOEntryWAVEARC::InequalityOperator(&left, &right);
    }
}
