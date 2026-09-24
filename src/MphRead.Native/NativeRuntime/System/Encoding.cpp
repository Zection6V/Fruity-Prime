#include "Encoding.hpp"

#include <algorithm>

namespace MphRead::NativeRuntime
{
    namespace
    {
        constexpr char32_t ReplacementChar = 0xFFFDU;

        [[nodiscard]] constexpr bool IsScalar(char32_t value) noexcept
        {
            return value <= 0x10FFFFU && (value < 0xD800U || value > 0xDFFFU);
        }

        [[nodiscard]] constexpr bool InRange(unsigned char value, unsigned char low, unsigned char high) noexcept
        {
            return value >= low && value <= high;
        }

        // Rune.DecodeFromUtf8, with Unicode's table of well-formed sequences
        // (Table 3-7): the second byte's range depends on the first, which is
        // what makes an overlong form, a surrogate or a value past U+10FFFF
        // fail at the second byte rather than after the whole sequence.
        //
        // WTF-8 differs only in letting ED A0..BF through, which is how it
        // writes a lone surrogate.
        [[nodiscard]] Utf8Scalar Decode(std::string_view bytes, bool allowSurrogates = false) noexcept
        {
            if (bytes.empty())
            {
                return {ReplacementChar, 0, OperationStatus::NeedMoreData};
            }
            const auto first = static_cast<unsigned char>(bytes[0]);
            if (first <= 0x7FU)
            {
                return {first, 1, OperationStatus::Done};
            }

            std::size_t length = 0;
            unsigned char secondLow = 0x80U;
            unsigned char secondHigh = 0xBFU;
            char32_t value = 0;
            if (InRange(first, 0xC2U, 0xDFU))
            {
                length = 2;
                value = first & 0x1FU;
            }
            else if (InRange(first, 0xE0U, 0xEFU))
            {
                length = 3;
                value = first & 0x0FU;
                if (first == 0xE0U)
                    secondLow = 0xA0U;
                else if (first == 0xEDU && !allowSurrogates)
                    secondHigh = 0x9FU;
            }
            else if (InRange(first, 0xF0U, 0xF4U))
            {
                length = 4;
                value = first & 0x07U;
                if (first == 0xF0U)
                    secondLow = 0x90U;
                else if (first == 0xF4U)
                    secondHigh = 0x8FU;
            }
            else
            {
                return {ReplacementChar, 1, OperationStatus::InvalidData};
            }

            for (std::size_t index = 1; index < length; ++index)
            {
                if (index >= bytes.size())
                {
                    return {ReplacementChar, index, OperationStatus::NeedMoreData};
                }
                const auto next = static_cast<unsigned char>(bytes[index]);
                const unsigned char low = index == 1 ? secondLow : 0x80U;
                const unsigned char high = index == 1 ? secondHigh : 0xBFU;
                if (!InRange(next, low, high))
                {
                    return {ReplacementChar, index, OperationStatus::InvalidData};
                }
                value = (value << 6) | (next & 0x3FU);
            }
            return {value, length, OperationStatus::Done};
        }

        // A scalar, or a lone surrogate, as UTF-8 bytes with nothing replaced.
        void AppendUnchecked(std::string& output, char32_t value)
        {
            if (value <= 0x7FU)
            {
                output.push_back(static_cast<char>(value));
            }
            else if (value <= 0x7FFU)
            {
                output.push_back(static_cast<char>(0xC0U | (value >> 6)));
                output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
            }
            else if (value <= 0xFFFFU)
            {
                output.push_back(static_cast<char>(0xE0U | (value >> 12)));
                output.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
                output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
            }
            else
            {
                output.push_back(static_cast<char>(0xF0U | (value >> 18)));
                output.push_back(static_cast<char>(0x80U | ((value >> 12) & 0x3FU)));
                output.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
                output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
            }
        }

        template<class String>
        void AppendUtf16Units(String& output, char32_t value)
        {
            using Unit = typename String::value_type;
            if (value <= 0xFFFFU)
            {
                output.push_back(static_cast<Unit>(value));
                return;
            }
            value -= 0x10000U;
            output.push_back(static_cast<Unit>(0xD800U + (value >> 10)));
            output.push_back(static_cast<Unit>(0xDC00U + (value & 0x3FFU)));
        }

        // UTF-16 code units, one at a time, to scalars: a surrogate that is
        // not half of a pair is U+FFFD.
        template<class Unit, class Append>
        void DecodeUtf16Units(std::size_t count, Unit unit, Append append)
        {
            for (std::size_t index = 0; index < count; ++index)
            {
                char32_t value = unit(index);
                if (value >= 0xD800U && value <= 0xDBFFU)
                {
                    const char32_t low = index + 1 < count ? unit(index + 1) : 0;
                    if (low >= 0xDC00U && low <= 0xDFFFU)
                    {
                        value = 0x10000U + ((value - 0xD800U) << 10) + (low - 0xDC00U);
                        ++index;
                    }
                    else
                    {
                        value = ReplacementChar;
                    }
                }
                else if (value >= 0xDC00U && value <= 0xDFFFU)
                {
                    value = ReplacementChar;
                }
                append(value);
            }
        }

        [[nodiscard]] std::uint32_t ReadUnit(std::string_view bytes, std::size_t offset, std::size_t size, bool bigEndian) noexcept
        {
            std::uint32_t value = 0;
            for (std::size_t index = 0; index < size; ++index)
            {
                const auto byte = static_cast<unsigned char>(bytes[offset + (bigEndian ? index : size - 1 - index)]);
                value = (value << 8) | byte;
            }
            return value;
        }
    }

    Utf8Scalar RuneDecodeFromUtf8(std::string_view bytes) noexcept
    {
        return Decode(bytes);
    }

    Utf8Scalar DecodeUtf8Scalar(std::string_view text, std::size_t offset) noexcept
    {
        Utf8Scalar result = Decode(text.substr(offset));
        if (result.Status == OperationStatus::NeedMoreData)
        {
            result.Status = OperationStatus::InvalidData;
        }
        return result;
    }

    Utf8Scalar DecodeLastUtf8Scalar(std::string_view text, std::size_t end) noexcept
    {
        // Rune.DecodeLastFromUtf8: back up over at most three continuation
        // bytes to a byte that could start a sequence, and decode forwards
        // from there. If that does not land exactly on `end`, the last byte
        // is a stray continuation byte and is ill-formed on its own.
        std::size_t start = end - 1;
        const std::size_t limit = end >= 4 ? end - 4 : 0;
        while (start > limit && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U)
        {
            --start;
        }
        Utf8Scalar result = DecodeUtf8Scalar(text.substr(0, end), start);
        if (start + result.Length == end)
        {
            return result;
        }
        return {ReplacementChar, 1, OperationStatus::InvalidData};
    }

    void AppendUtf8(std::string& output, char32_t scalar)
    {
        AppendUnchecked(output, IsScalar(scalar) ? scalar : ReplacementChar);
    }

    std::string Utf8GetString(std::string_view bytes)
    {
        std::string result;
        result.reserve(bytes.size());
        std::size_t index = 0;
        while (index < bytes.size())
        {
            // Copy the well-formed run as it stands; only a fault is rebuilt.
            const Utf8Scalar scalar = DecodeUtf8Scalar(bytes, index);
            if (scalar.Valid())
            {
                result.append(bytes.substr(index, scalar.Length));
            }
            else
            {
                AppendUtf8(result, ReplacementChar);
            }
            index += scalar.Length;
        }
        return result;
    }

    std::string Utf8GetString(std::span<const std::uint8_t> bytes)
    {
        return Utf8GetString(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    }

    std::string StreamReaderDecode(std::string_view bytes)
    {
        // StreamReader.DetectEncoding, in its own order: FF FE is UTF-16
        // unless the next two bytes are zero, which makes it UTF-32.
        if (bytes.starts_with(std::string_view("\xFF\xFE\0\0", 4)))
            return Utf32GetString(bytes.substr(4), false);
        if (bytes.starts_with(std::string_view("\0\0\xFE\xFF", 4)))
            return Utf32GetString(bytes.substr(4), true);
        if (bytes.starts_with("\xEF\xBB\xBF"))
            return Utf8GetString(bytes.substr(3));
        if (bytes.starts_with("\xFF\xFE"))
            return Utf16GetString(bytes.substr(2), false);
        if (bytes.starts_with("\xFE\xFF"))
            return Utf16GetString(bytes.substr(2), true);
        return Utf8GetString(bytes);
    }

    std::string StreamReaderDecode(std::span<const std::uint8_t> bytes)
    {
        return StreamReaderDecode(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    }

    std::string Utf16GetString(std::string_view bytes, bool bigEndian)
    {
        std::string result;
        result.reserve(bytes.size());
        DecodeUtf16Units(bytes.size() / 2,
            [&](std::size_t index) { return static_cast<char32_t>(ReadUnit(bytes, index * 2, 2, bigEndian)); },
            [&](char32_t scalar) { AppendUtf8(result, scalar); });
        if (bytes.size() % 2 != 0)
        {
            // A trailing odd byte is an incomplete code unit.
            AppendUtf8(result, ReplacementChar);
        }
        return result;
    }

    std::string Utf32GetString(std::string_view bytes, bool bigEndian)
    {
        std::string result;
        result.reserve(bytes.size());
        const std::size_t count = bytes.size() / 4;
        for (std::size_t index = 0; index < count; ++index)
        {
            // AppendUtf8 already turns a non-scalar into U+FFFD.
            AppendUtf8(result, static_cast<char32_t>(ReadUnit(bytes, index * 4, 4, bigEndian)));
        }
        if (bytes.size() % 4 != 0)
        {
            AppendUtf8(result, ReplacementChar);
        }
        return result;
    }

    std::string Utf16ToUtf8(std::u16string_view value)
    {
        std::string result;
        result.reserve(value.size());
        DecodeUtf16Units(value.size(),
            [&](std::size_t index) { return static_cast<char32_t>(value[index]); },
            [&](char32_t scalar) { AppendUtf8(result, scalar); });
        return result;
    }

    std::u16string Utf8ToUtf16(std::string_view value)
    {
        std::u16string result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size();)
        {
            const Utf8Scalar scalar = DecodeUtf8Scalar(value, index);
            AppendUtf16Units(result, scalar.Value);
            index += scalar.Length;
        }
        return result;
    }

    std::u32string Utf8ToUtf32(std::string_view value)
    {
        std::u32string result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size();)
        {
            const Utf8Scalar scalar = DecodeUtf8Scalar(value, index);
            result.push_back(scalar.Value);
            index += scalar.Length;
        }
        return result;
    }

    std::string Utf32ToUtf8(std::u32string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (const char32_t scalar : value)
        {
            AppendUtf8(result, scalar);
        }
        return result;
    }

    std::size_t Utf16Length(std::string_view value) noexcept
    {
        std::size_t length = 0;
        for (std::size_t index = 0; index < value.size();)
        {
            const Utf8Scalar scalar = DecodeUtf8Scalar(value, index);
            length += scalar.Value > 0xFFFFU ? 2 : 1;
            index += scalar.Length;
        }
        return length;
    }

    std::string WideToUtf8(std::wstring_view value)
    {
        if constexpr (sizeof(wchar_t) == 2)
        {
            std::string result;
            result.reserve(value.size());
            DecodeUtf16Units(value.size(),
                [&](std::size_t index) { return static_cast<char32_t>(static_cast<std::uint16_t>(value[index])); },
                [&](char32_t scalar) { AppendUtf8(result, scalar); });
            return result;
        }
        else
        {
            std::string result;
            result.reserve(value.size());
            for (const wchar_t unit : value)
            {
                AppendUtf8(result, static_cast<char32_t>(unit));
            }
            return result;
        }
    }

    std::wstring Utf8ToWide(std::string_view value)
    {
        std::wstring result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size();)
        {
            const Utf8Scalar scalar = DecodeUtf8Scalar(value, index);
            if constexpr (sizeof(wchar_t) == 2)
                AppendUtf16Units(result, scalar.Value);
            else
                result.push_back(static_cast<wchar_t>(scalar.Value));
            index += scalar.Length;
        }
        return result;
    }

    std::string WideToWtf8(std::wstring_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            char32_t unit = static_cast<char32_t>(value[index]);
            if constexpr (sizeof(wchar_t) == 2)
            {
                unit &= 0xFFFFU;
                if (unit >= 0xD800U && unit <= 0xDBFFU && index + 1 < value.size())
                {
                    const char32_t low = static_cast<char32_t>(value[index + 1]) & 0xFFFFU;
                    if (low >= 0xDC00U && low <= 0xDFFFU)
                    {
                        unit = 0x10000U + ((unit - 0xD800U) << 10) + (low - 0xDC00U);
                        ++index;
                    }
                }
            }
            AppendUnchecked(result, unit <= 0x10FFFFU ? unit : ReplacementChar);
        }
        return result;
    }

    std::wstring Wtf8ToWide(std::string_view value)
    {
        std::wstring result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size();)
        {
            Utf8Scalar scalar = Decode(value.substr(index), true);
            if (!scalar.Valid())
            {
                scalar.Value = ReplacementChar;
            }
            if constexpr (sizeof(wchar_t) == 2)
                AppendUtf16Units(result, scalar.Value);
            else
                result.push_back(static_cast<wchar_t>(scalar.Value));
            index += scalar.Length;
        }
        return result;
    }
}
