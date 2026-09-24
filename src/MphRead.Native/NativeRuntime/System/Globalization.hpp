#pragma once

// Culture-dependent string and number operations (System.String,
// System.Globalization, Int32/Single parsing) as .NET performs them for the
// current culture. Strings are UTF-8.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace MphRead::NativeRuntime
{
    // char.IsWhiteSpace(c): the Unicode white space the BCL knows, for a
    // UTF-16 unit or a code point alike.
    [[nodiscard]] constexpr bool CharIsWhiteSpace(char32_t value) noexcept
    {
        return (value >= U'\u0009' && value <= U'\u000D') || value == U'\u0020'
            || value == U'\u0085' || value == U'\u00A0' || value == U'\u1680'
            || (value >= U'\u2000' && value <= U'\u200A') || value == U'\u2028'
            || value == U'\u2029' || value == U'\u202F' || value == U'\u205F'
            || value == U'\u3000';
    }
    // The white space int.Parse and float.Parse skip around a number
    // (NumberStyles.AllowLeadingWhite/AllowTrailingWhite): tab to carriage
    // return and the space, nothing wider.
    [[nodiscard]] constexpr bool IsNumberWhiteSpace(char32_t value) noexcept
    {
        return value == U' ' || (value >= U'\t' && value <= U'\r');
    }
    // string.IsNullOrWhiteSpace(value), character by character.
    [[nodiscard]] bool StringIsNullOrWhiteSpace(std::string_view value) noexcept;
    [[nodiscard]] bool StringIsNullOrWhiteSpace(const std::string& value) noexcept;
    [[nodiscard]] bool StringIsNullOrWhiteSpace(const char* value) noexcept;
    [[nodiscard]] bool StringIsNullOrWhiteSpace(const std::optional<std::string>& value) noexcept;
    // value.Trim() without the copy: the same characters, as a view into value.
    [[nodiscard]] std::string_view StringTrimView(std::string_view value) noexcept;
    // value.Replace(oldValue, newValue): ordinal, every occurrence, left to
    // right; ArgumentException for an empty oldValue, as .NET throws.
    [[nodiscard]] std::string StringReplace(
        std::string value, std::string_view oldValue, std::string_view newValue);
    // string.Equals(left, right, StringComparison.OrdinalIgnoreCase).
    [[nodiscard]] bool StringEqualsOrdinalIgnoreCase(
        std::string_view left, std::string_view right) noexcept;
    // (int)Math.Round(value): to even on a tie, as Math.Round defaults.
    [[nodiscard]] std::int32_t MathRoundToInt32(double value) noexcept;
    // Encoding.ASCII.GetString(bytes): every byte over 0x7F becomes '?'.
    [[nodiscard]] std::string AsciiGetString(std::span<const std::uint8_t> bytes);
    // value.Trim(): leading and trailing char.IsWhiteSpace characters off.
    [[nodiscard]] std::string StringTrim(std::string_view value);
    // float.TryParse(text, out value): NumberStyles.Float | AllowThousands, current culture.
    [[nodiscard]] bool SingleTryParseCurrentCulture(std::string text, float& value);
    // float.ToString(): the shortest round-trippable form, current culture.
    [[nodiscard]] std::string SingleToString(float value);
    // float.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out value).
    [[nodiscard]] bool SingleTryParseInvariantFloat(std::string text, float& value);
    // double.TryParse(text, NumberStyles.Float | AllowThousands,
    // CultureInfo.InvariantCulture, out value).
    [[nodiscard]] bool DoubleTryParseInvariant(std::string text, double& value);
    // double.ToString("0.0"), "0.00" and so on: exactly `decimals` places,
    // rounded half away from zero, current culture.
    [[nodiscard]] std::string DoubleToStringFixed(double value, std::int32_t decimals);
    // double.ToString("F2"), current culture.
    [[nodiscard]] std::string DoubleToStringFixed2(double value);
    // float.ToString("0.#"), current culture.
    [[nodiscard]] std::string SingleToStringZeroPointHash(float value);
    // int.ToString() / long.ToString(), current culture.
    [[nodiscard]] std::string Int32ToString(std::int32_t value);
    [[nodiscard]] std::string Int64ToString(std::int64_t value);
    // double.ToString("0") / ToString("F0"): rounded to no decimals,
    // current culture.
    [[nodiscard]] std::string DoubleToStringNoDecimals(double value);
    // NumberFormatInfo.NumberDecimalSeparator for the current culture.
    [[nodiscard]] std::string CurrentDecimalSeparator();
    // int.TryParse(text, NumberStyles.HexNumber, null, out value).
    [[nodiscard]] bool Int32TryParseHexNumber(std::string text, std::int32_t& value);

    // value.EndsWith(suffix): culture-sensitive, current culture.
    [[nodiscard]] bool StringEndsWithCurrentCulture(
        const std::string& value, const std::string& suffix);
    // value.StartsWith(prefix): culture-sensitive, current culture.
    [[nodiscard]] bool StringStartsWithCurrentCulture(
        std::u32string_view value,
        std::u32string_view prefix);
    // int.TryParse(value, out result): NumberStyles.Integer, current culture.
    [[nodiscard]] bool Int32TryParseCurrentCulture(
        std::u32string_view value,
        std::int32_t& result);
    [[nodiscard]] bool Int32TryParseCurrentCulture(
        std::string_view value,
        std::int32_t& result);
    // int.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture,
    // out result).
    [[nodiscard]] bool Int32TryParseInvariant(std::string_view value, std::int32_t& result);
}
