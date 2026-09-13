#include "INFOEntryBANK.hpp"

#include <cassert>
#include <charconv>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace
{
    [[noreturn]] void ThrowArgumentOutOfRange()
    {
        throw std::out_of_range("Specified argument was out of the range of valid values.");
    }

    [[noreturn]] void ThrowArgumentDestinationTooShort()
    {
        throw std::invalid_argument("Destination is too short.");
    }

    [[noreturn]] void ThrowIndexOutOfRange()
    {
        throw std::out_of_range("Index was outside the bounds of the array.");
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

    [[nodiscard]] std::uint16_t ReadUInt16LittleEndian(const std::uint8_t* data) noexcept
    {
        return static_cast<std::uint16_t>(data[0])
            | static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8U);
    }

    void WriteUInt16LittleEndian(std::uint8_t* data, std::uint16_t value) noexcept
    {
        data[0] = static_cast<std::uint8_t>(value);
        data[1] = static_cast<std::uint8_t>(value >> 8U);
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
    INFOEntryBANK::INFOEntryBANK(const INFOEntryBANK* other)
        : INFOEntry(other)
    {
        _fileID = other->_fileID;
        _waveArchives = other->_waveArchives;
        _sbnk = other->_sbnk;
    }

    std::uint32_t INFOEntryBANK::FileID() const noexcept
    {
        return _fileID;
    }

    void INFOEntryBANK::FileID(std::uint32_t value) noexcept
    {
        _fileID = value;
    }

    std::span<const std::uint16_t> INFOEntryBANK::WaveArchives() const noexcept
    {
        return _waveArchives;
    }

    const std::shared_ptr<NCSFCommon::NC::SBNK>& INFOEntryBANK::SBNK() const noexcept
    {
        return _sbnk;
    }

    void INFOEntryBANK::SBNK(std::shared_ptr<NCSFCommon::NC::SBNK> value) noexcept
    {
        _sbnk = std::move(value);
    }

    std::uint32_t INFOEntryBANK::Size() const noexcept
    {
        return 0x0CU;
    }

    INFOEntryBANK* INFOEntryBANK::Read(std::span<const std::uint8_t> span)
    {
        _fileID = ReadUInt32LittleEndian(span);
        if (span.size() < 0x0CU)
        {
            ThrowArgumentOutOfRange();
        }
        for (std::size_t i = 0; i < _waveArchives.size(); ++i)
        {
            _waveArchives[i] = ReadUInt16LittleEndian(span.data() + 0x04U + i * 2U);
        }
        return this;
    }

    void INFOEntryBANK::Write(std::span<std::uint8_t> span)
    {
        WriteUInt32LittleEndian(span, _fileID);
        if (span.size() < 0x0CU)
        {
            ThrowArgumentDestinationTooShort();
        }
        for (std::size_t i = 0; i < _waveArchives.size(); ++i)
        {
            WriteUInt16LittleEndian(span.data() + 0x04U + i * 2U, _waveArchives[i]);
        }
    }

    void INFOEntryBANK::ReplaceWaveArchive(int i, std::uint16_t newWaveArchive)
    {
        assert(i >= 0 && i <= 3);
        if (i < 0 || i >= static_cast<int>(_waveArchives.size()))
        {
            ThrowIndexOutOfRange();
        }
        _waveArchives[static_cast<std::size_t>(i)] = newWaveArchive;
    }

    bool INFOEntryBANK::FileEquals(const INFOEntryBANK* other) const
    {
        if (other == nullptr || _sbnk == nullptr || other->_sbnk == nullptr)
        {
            return false;
        }
        return *_sbnk == *other->_sbnk;
    }

    std::u16string INFOEntryBANK::DebuggerDisplay() const
    {
        std::u16string result = INFOEntry::DebuggerDisplay();
        result += u"File ID: ";
        AppendUnsigned(result, _fileID);
        result += u", WaveArchives: {";
        for (std::size_t i = 0; i < _waveArchives.size(); ++i)
        {
            if (i != 0U)
            {
                result += u", ";
            }
            AppendUnsigned(result, _waveArchives[i]);
        }
        result.push_back(u'}');
        return result;
    }
}
