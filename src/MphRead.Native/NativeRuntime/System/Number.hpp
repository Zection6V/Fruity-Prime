#pragma once

// System.Globalization.NumberFormatInfo, NumberStyles, and the Parse and
// ToString of the numeric types -- what `value.ToString("0.00")`,
// `$"{value:F1}"`, `float.TryParse(text, out value)` and
// `int.Parse(text, CultureInfo.InvariantCulture)` do. Text is UTF-8.
//
// Formatting follows .NET Core 3.0 and later:
// - no format, "G" with no precision and "R" are the shortest text that reads
//   back as the same value, switching to E notation once the point is more
//   than 17 digits from the start for a double and 9 for a float ("1E+17");
// - "F", "N", "E" and "G" with a precision round the exact binary value;
// - custom formats ("0.0", "#,##0.###", "0.#######e+0") first take 15
//   significant digits of a double or 7 of a float, then round those half
//   away from zero -- which is why 0.125f.ToString("0.00") is "0.13";
// - a negative value keeps its sign even when it rounds to zero, in the
//   standard formats and custom ones alike ("-0", "-0.00").

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace MphRead::NativeRuntime
{
    enum class NumberStyles : std::uint32_t
    {
        None = 0,
        AllowLeadingWhite = 0x001,
        AllowTrailingWhite = 0x002,
        AllowLeadingSign = 0x004,
        AllowTrailingSign = 0x008,
        AllowParentheses = 0x010,
        AllowDecimalPoint = 0x020,
        AllowThousands = 0x040,
        AllowExponent = 0x080,
        AllowCurrencySymbol = 0x100,
        AllowHexSpecifier = 0x200,
        Integer = AllowLeadingWhite | AllowTrailingWhite | AllowLeadingSign,
        HexNumber = AllowLeadingWhite | AllowTrailingWhite | AllowHexSpecifier,
        Number = Integer | AllowTrailingSign | AllowDecimalPoint | AllowThousands,
        Float = Integer | AllowDecimalPoint | AllowExponent,
        Any = Number | AllowParentheses | AllowExponent | AllowCurrencySymbol,
    };

    [[nodiscard]] constexpr NumberStyles operator|(NumberStyles left, NumberStyles right) noexcept
    {
        return static_cast<NumberStyles>(
            static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr bool HasStyle(NumberStyles styles, NumberStyles flag) noexcept
    {
        return (static_cast<std::uint32_t>(styles) & static_cast<std::uint32_t>(flag)) != 0;
    }

    struct NumberFormatInfo final
    {
        std::string NumberDecimalSeparator = ".";
        std::string NumberGroupSeparator = ",";
        std::vector<std::int32_t> NumberGroupSizes{ 3 };
        std::int32_t NumberDecimalDigits = 2;
        // 0 "(n)", 1 "-n", 2 "- n", 3 "n-", 4 "n -".
        std::int32_t NumberNegativePattern = 1;
        std::string PositiveSign = "+";
        std::string NegativeSign = "-";
        std::string PositiveInfinitySymbol = "Infinity";
        std::string NegativeInfinitySymbol = "-Infinity";
        std::string NaNSymbol = "NaN";
        std::string PercentSymbol = "%";
        std::string PerMilleSymbol = "\xE2\x80\xB0";
        std::int32_t PercentDecimalDigits = 2;
        // 0 "n %", 1 "n%", 2 "%n", 3 "% n".
        std::int32_t PercentPositivePattern = 0;
        // 0 "-n %", 1 "-n%", 2 "-%n", 3 "%-n", 4 "%n-", 5 "n-%", 6 "n%-",
        // 7 "-% n", 8 "n %-", 9 "% n-", 10 "% -n", 11 "n- %".
        std::int32_t PercentNegativePattern = 0;
        // A culture whose minus sign is not U+002D (U+2212, say) still reads
        // a hyphen as one.
        bool AllowHyphenDuringParsing = false;

        // CultureInfo.InvariantCulture.NumberFormat.
        [[nodiscard]] static const NumberFormatInfo& InvariantInfo() noexcept;
        // CultureInfo.CurrentCulture.NumberFormat: read from ICU (or NLS on
        // Windows) the first time it is asked for, with the user's overrides,
        // and kept for the life of the process as .NET keeps it.
        [[nodiscard]] static const NumberFormatInfo& CurrentInfo();
    };

    // Thread.CurrentThread.CurrentCulture = CultureInfo.InvariantCulture:
    // CurrentInfo() on this thread is the invariant one from now on.
    void UseInvariantCultureOnThisThread() noexcept;
    // Whether this thread has been switched to the invariant culture.
    [[nodiscard]] bool CurrentCultureIsInvariantOnThisThread() noexcept;

    // ---- ToString ----------------------------------------------------------

    // value.ToString(format, info). An empty format is ToString().
    // FormatException for a format .NET rejects.
    [[nodiscard]] std::string NumberToString(double value, std::string_view format,
        const NumberFormatInfo& info);
    [[nodiscard]] std::string NumberToString(float value, std::string_view format,
        const NumberFormatInfo& info);
    // Integers of any width: "X" and "D" see the type's own width, as
    // (-1).ToString("X") is "FFFFFFFF" for an int and "FF" for an sbyte.
    [[nodiscard]] std::string IntegerToString(std::uint64_t magnitude, bool negative,
        std::uint64_t bits, std::int32_t byteWidth, std::string_view format,
        const NumberFormatInfo& info);

    template <class T>
        requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
    [[nodiscard]] std::string NumberToString(T value, std::string_view format,
        const NumberFormatInfo& info)
    {
        using Unsigned = std::make_unsigned_t<T>;
        const bool negative = value < 0;
        const std::uint64_t magnitude = negative
            ? static_cast<std::uint64_t>(0) - static_cast<std::uint64_t>(static_cast<std::int64_t>(value))
            : static_cast<std::uint64_t>(value);
        return IntegerToString(magnitude, negative,
            static_cast<std::uint64_t>(static_cast<Unsigned>(value)),
            static_cast<std::int32_t>(sizeof(T)), format, info);
    }

    // value.ToString(format): the current culture.
    template <class T>
    [[nodiscard]] std::string ToString(T value, std::string_view format = {})
    {
        return NumberToString(value, format, NumberFormatInfo::CurrentInfo());
    }

    // value.ToString(format, CultureInfo.InvariantCulture).
    template <class T>
    [[nodiscard]] std::string ToStringInvariant(T value, std::string_view format = {})
    {
        return NumberToString(value, format, NumberFormatInfo::InvariantInfo());
    }

    // ---- Parse -------------------------------------------------------------

    // double.TryParse(text, styles, info, out value). A failed parse leaves 0.
    [[nodiscard]] bool TryParseDouble(std::string_view text, NumberStyles styles,
        const NumberFormatInfo& info, double& value);
    [[nodiscard]] bool TryParseSingle(std::string_view text, NumberStyles styles,
        const NumberFormatInfo& info, float& value);
    // long.TryParse / int.TryParse / ... for a type `bits` wide, signed or not.
    [[nodiscard]] bool TryParseInteger(std::string_view text, NumberStyles styles,
        const NumberFormatInfo& info, bool isSigned, std::int32_t bits, std::int64_t& value);

    // long.Parse / int.Parse / ...: FormatException for text that is not a
    // number of that style, OverflowException for one out of range.
    [[nodiscard]] std::int64_t ParseInteger(std::string_view text, NumberStyles styles,
        const NumberFormatInfo& info, bool isSigned, std::int32_t bits);

    template <class T>
        requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
    [[nodiscard]] T ParseInteger(std::string_view text, NumberStyles styles, const NumberFormatInfo& info)
    {
        return static_cast<T>(ParseInteger(text, styles, info, std::is_signed_v<T>,
            static_cast<std::int32_t>(sizeof(T) * 8)));
    }

    template <class T>
        requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
    [[nodiscard]] bool TryParseInteger(std::string_view text, NumberStyles styles,
        const NumberFormatInfo& info, T& value)
    {
        std::int64_t parsed = 0;
        const bool ok = TryParseInteger(text, styles, info, std::is_signed_v<T>,
            static_cast<std::int32_t>(sizeof(T) * 8), parsed);
        value = static_cast<T>(parsed);
        return ok;
    }

    // The overloads the game calls, by the C# they stand for.

    // float.TryParse(text, out value): Float | AllowThousands, current culture.
    [[nodiscard]] bool SingleTryParseCurrentCulture(std::string_view text, float& value);
    // float.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out value).
    [[nodiscard]] bool SingleTryParseInvariant(std::string_view text, float& value);
    // double.TryParse(text, out value): Float | AllowThousands, current culture.
    [[nodiscard]] bool DoubleTryParseCurrentCulture(std::string_view text, double& value);
    // double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out value).
    [[nodiscard]] bool DoubleTryParseInvariant(std::string_view text, double& value);
    // int.TryParse(text, out value): Integer, current culture.
    [[nodiscard]] bool Int32TryParseCurrentCulture(std::string_view text, std::int32_t& value);
    // int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out value).
    [[nodiscard]] bool Int32TryParseInvariant(std::string_view text, std::int32_t& value);
    // int.TryParse(text, NumberStyles.HexNumber, null, out value).
    [[nodiscard]] bool Int32TryParseHexNumber(std::string_view text, std::int32_t& value);
    // Int32.Parse(text, NumberStyles.HexNumber).
    [[nodiscard]] std::int32_t Int32ParseHexNumber(std::string_view text);
}
