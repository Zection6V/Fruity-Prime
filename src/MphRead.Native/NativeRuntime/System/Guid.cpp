#include "Guid.hpp"

#include "Exceptions.hpp"
#include "Random.hpp"

#include <algorithm>

namespace MphRead::NativeRuntime
{
    Guid::Guid(std::span<const std::uint8_t> bytes)
    {
        if (bytes.size() != 16)
        {
            throw System::ArgumentException(
                "Byte array for Guid must be exactly 16 bytes long. (Parameter 'b')");
        }
        std::copy(bytes.begin(), bytes.end(), _bytes.begin());
    }

    Guid Guid::NewGuid()
    {
        Guid result;
        RandomNumberGeneratorFill(result._bytes.data(), result._bytes.size());
        // Version 4 in the high nibble of field c (byte 7, little-endian), and
        // the RFC 4122 variant in byte 8, as Guid.NewGuid sets them.
        result._bytes[7] = static_cast<std::uint8_t>((result._bytes[7] & 0x0FU) | 0x40U);
        result._bytes[8] = static_cast<std::uint8_t>((result._bytes[8] & 0x3FU) | 0x80U);
        return result;
    }

    bool Guid::TryWriteBytes(std::span<std::uint8_t> destination) const noexcept
    {
        if (destination.size() < 16)
        {
            return false;
        }
        std::copy(_bytes.begin(), _bytes.end(), destination.begin());
        return true;
    }

    std::string Guid::ToString(std::string_view format) const
    {
        static constexpr char Digits[] = "0123456789abcdef";
        const auto hex = [](std::string& text, std::uint8_t value)
        {
            text.push_back(Digits[value >> 4]);
            text.push_back(Digits[value & 0x0FU]);
        };
        const bool hyphens = format.empty() || format == "D" || format == "d";
        if (!hyphens && format != "N" && format != "n")
        {
            throw System::FormatException(
                "Format string can be only \"D\", \"d\", \"N\", \"n\", \"P\", \"p\", \"B\", \"b\", \"X\" or \"x\".");
        }
        std::string text;
        // The first three fields are little-endian integers printed big-end
        // first.
        for (int i = 3; i >= 0; --i)
        {
            hex(text, _bytes[static_cast<std::size_t>(i)]);
        }
        if (hyphens) text.push_back('-');
        hex(text, _bytes[5]);
        hex(text, _bytes[4]);
        if (hyphens) text.push_back('-');
        hex(text, _bytes[7]);
        hex(text, _bytes[6]);
        if (hyphens) text.push_back('-');
        hex(text, _bytes[8]);
        hex(text, _bytes[9]);
        if (hyphens) text.push_back('-');
        for (std::size_t i = 10; i < 16; ++i)
        {
            hex(text, _bytes[i]);
        }
        return text;
    }
}
