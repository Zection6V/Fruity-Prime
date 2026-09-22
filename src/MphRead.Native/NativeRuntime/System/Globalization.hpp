#pragma once

// Culture-dependent string and number operations (System.String,
// System.Globalization, Int32/Single parsing) as .NET performs them for the
// current culture. Strings are UTF-8.

#include <cstdint>
#include <string>
#include <string_view>

namespace MphRead::NativeRuntime
{
    // string.Trim().
    [[nodiscard]] std::string StringTrim(std::string value);
    // float.TryParse(text, out value): NumberStyles.Float | AllowThousands, current culture.
    [[nodiscard]] bool SingleTryParseCurrentCulture(std::string text, float& value);
    // float.ToString(): the shortest round-trippable form, current culture.
    [[nodiscard]] std::string SingleToString(float value);
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
}
