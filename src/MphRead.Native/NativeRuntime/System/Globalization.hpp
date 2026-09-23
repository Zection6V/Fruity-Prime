#pragma once

// Culture-dependent string and number operations (System.String,
// System.Globalization, Int32/Single parsing) as .NET performs them for the
// current culture. Strings are UTF-8.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace MphRead::NativeRuntime
{
    // string.IsNullOrWhiteSpace(value).
    [[nodiscard]] bool StringIsNullOrWhiteSpace(std::string_view value) noexcept;
    // string.Equals(left, right, StringComparison.OrdinalIgnoreCase).
    [[nodiscard]] bool StringEqualsOrdinalIgnoreCase(
        std::string_view left, std::string_view right) noexcept;
    // (int)Math.Round(value): to even on a tie, as Math.Round defaults.
    [[nodiscard]] std::int32_t MathRoundToInt32(double value) noexcept;
    // Encoding.ASCII.GetString(bytes): every byte over 0x7F becomes '?'.
    [[nodiscard]] std::string AsciiGetString(std::span<const std::uint8_t> bytes);
    // string.Trim().
    [[nodiscard]] std::string StringTrim(std::string value);
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
}
