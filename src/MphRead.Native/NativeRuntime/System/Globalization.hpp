#pragma once

// System.String and System.Globalization's text operations: white space,
// trimming, the invariant culture's case mapping, OrdinalIgnoreCase, and the
// current culture's comparisons. Strings are UTF-8, compared by code point.
// Numbers are in Number.hpp, which this includes.

#include "Number.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::NativeRuntime
{
    // char.IsWhiteSpace(c): the Unicode white space the BCL knows, for a
    // UTF-16 unit or a code point alike.
    [[nodiscard]] constexpr bool CharIsWhiteSpace(char32_t value) noexcept
    {
        return (value >= U'\u0009' && value <= U'\u000D') || value == U' '
            || value == U'\u0085' || value == U' ' || value == U' '
            || (value >= U' ' && value <= U' ') || value == U' '
            || value == U' ' || value == U' ' || value == U' '
            || value == U'　';
    }
    // char.IsLetterOrDigit(c) for one UTF-16 unit: the Unicode letter and
    // decimal-digit categories, which no culture changes.
    [[nodiscard]] bool CharIsLetterOrDigit(char16_t value) noexcept;
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
    // value.Trim(): leading and trailing char.IsWhiteSpace characters off.
    [[nodiscard]] std::string StringTrim(std::string_view value);
    // value.Replace(oldValue, newValue): ordinal, every occurrence, left to
    // right; ArgumentException for an empty oldValue, as .NET throws.
    [[nodiscard]] std::string StringReplace(
        std::string value, std::string_view oldValue, std::string_view newValue);
    // value.Replace(oldValue, newValue, StringComparison.OrdinalIgnoreCase).
    [[nodiscard]] std::string StringReplaceOrdinalIgnoreCase(
        std::string_view value, std::string_view oldValue, std::string_view newValue);
    // value.Split(separator, options): RemoveEmptyEntries and TrimEntries as
    // .NET applies them -- each entry trimmed first, then empties dropped.
    [[nodiscard]] std::vector<std::string> StringSplit(
        std::string_view value, char separator, bool removeEmptyEntries = false, bool trimEntries = false);
    // value.PadLeft(totalWidth, padding) / value.PadRight(...): widths are in
    // UTF-16 units, as a C# string's Length is. `{x,6}` in a composite format
    // is PadLeft(6) and `{x,-6}` is PadRight(6).
    [[nodiscard]] std::string StringPadLeft(std::string value, std::size_t totalWidth, char padding = ' ');
    [[nodiscard]] std::string StringPadRight(std::string value, std::size_t totalWidth, char padding = ' ');
    // (int)Math.Round(value): to even on a tie, as Math.Round defaults, and
    // saturating where the value does not fit.
    [[nodiscard]] std::int32_t MathRoundToInt32(double value) noexcept;
    // Encoding.ASCII.GetString(bytes): every byte over 0x7F becomes '?'.
    [[nodiscard]] std::string AsciiGetString(std::span<const std::uint8_t> bytes);

    // ---- Case --------------------------------------------------------------

    // char.ToUpperInvariant / char.ToLowerInvariant: Unicode's simple case
    // mapping (from ICU where there is one), except that the dotless i and
    // the dotted I are left alone, as .NET's invariant culture leaves them.
    [[nodiscard]] char32_t ToUpperInvariant(char32_t value) noexcept;
    [[nodiscard]] char32_t ToLowerInvariant(char32_t value) noexcept;
    // char.IsControl: U+0000-U+001F and U+007F-U+009F.
    [[nodiscard]] constexpr bool CharIsControl(char32_t value) noexcept
    {
        return value <= 0x1FU || (value >= 0x7FU && value <= 0x9FU);
    }
    // string.ToUpperInvariant() / ToLowerInvariant().
    [[nodiscard]] std::string ToUpperInvariant(std::string_view value);
    [[nodiscard]] std::string ToLowerInvariant(std::string_view value);
    // string.ToUpper() / ToLower(): the current culture's casing -- the
    // invariant one, except that a Turkish or Azeri culture maps i to İ and
    // I to ı, and any other culture maps ı to I and İ to i.
    [[nodiscard]] std::string ToUpperCurrentCulture(std::string_view value);
    [[nodiscard]] std::string ToLowerCurrentCulture(std::string_view value);
    // What OrdinalIgnoreCase compares: the upper case of each character,
    // where the long s and the dotless i are not the S and the I.
    [[nodiscard]] char32_t OrdinalCasingToUpper(char32_t value) noexcept;

    // string.Equals / StartsWith / EndsWith / Compare / IndexOf with
    // StringComparison.OrdinalIgnoreCase.
    [[nodiscard]] bool StringEqualsOrdinalIgnoreCase(
        std::string_view left, std::string_view right) noexcept;
    [[nodiscard]] bool StringStartsWithOrdinalIgnoreCase(
        std::string_view value, std::string_view prefix) noexcept;
    [[nodiscard]] bool StringEndsWithOrdinalIgnoreCase(
        std::string_view value, std::string_view suffix) noexcept;
    [[nodiscard]] std::int32_t StringCompareOrdinalIgnoreCase(
        std::string_view left, std::string_view right) noexcept;
    // -1 when absent; otherwise the byte offset of the match.
    [[nodiscard]] std::ptrdiff_t StringIndexOfOrdinalIgnoreCase(
        std::string_view value, std::string_view search) noexcept;
    [[nodiscard]] inline bool StringContainsOrdinalIgnoreCase(
        std::string_view value, std::string_view search) noexcept
    {
        return StringIndexOfOrdinalIgnoreCase(value, search) >= 0;
    }
    // StringComparer.OrdinalIgnoreCase.GetHashCode: equal for any two strings
    // StringEqualsOrdinalIgnoreCase calls equal, seeded once per process.
    [[nodiscard]] std::int32_t StringHashOrdinalIgnoreCase(std::string_view value) noexcept;
    // StringComparer.OrdinalIgnoreCase, for ordered and hashed containers.
    struct OrdinalIgnoreCaseLess final
    {
        using is_transparent = void;
        [[nodiscard]] bool operator()(std::string_view left, std::string_view right) const noexcept
        {
            return StringCompareOrdinalIgnoreCase(left, right) < 0;
        }
    };
    struct OrdinalIgnoreCaseEqual final
    {
        using is_transparent = void;
        [[nodiscard]] bool operator()(std::string_view left, std::string_view right) const noexcept
        {
            return StringEqualsOrdinalIgnoreCase(left, right);
        }
    };
    struct OrdinalIgnoreCaseHash final
    {
        using is_transparent = void;
        [[nodiscard]] std::size_t operator()(std::string_view value) const noexcept
        {
            return static_cast<std::size_t>(static_cast<std::uint32_t>(StringHashOrdinalIgnoreCase(value)));
        }
    };

    // ---- The current culture -----------------------------------------------

    // CultureInfo.CurrentCulture.Name: "" for the invariant culture.
    [[nodiscard]] const std::string& CurrentCultureName();
    // value.StartsWith(prefix) / value.EndsWith(suffix): culture-sensitive,
    // CompareOptions.None, current culture.
    [[nodiscard]] bool StringStartsWithCurrentCulture(std::string_view value, std::string_view prefix);
    [[nodiscard]] bool StringEndsWithCurrentCulture(std::string_view value, std::string_view suffix);
    // value.StartsWith(prefix, StringComparison.InvariantCultureIgnoreCase):
    // the root collation, ignoring case -- and ignoring what it ignores, NUL
    // among it, so "ROOM" starts with "room\0\0".
    [[nodiscard]] bool StringStartsWithInvariantCultureIgnoreCase(std::string_view value, std::string_view prefix);
    // string.Compare(left, right) / left.CompareTo(right): the current
    // culture's collation, negative, zero or positive.
    [[nodiscard]] std::int32_t StringCompareCurrentCulture(std::string_view left, std::string_view right);

    // ---- bool --------------------------------------------------------------

    // bool.TryParse(value, out result): "True" or "False" ignoring case,
    // after white space and NUL characters are trimmed from both ends; result
    // is false whenever it returns false. A null value is not a boolean.
    [[nodiscard]] bool BooleanTryParse(std::string_view value, bool& result) noexcept;
    [[nodiscard]] bool BooleanTryParse(const std::optional<std::string_view>& value, bool& result) noexcept;
    [[nodiscard]] inline bool BooleanTryParse(const std::string& value, bool& result) noexcept
    {
        return BooleanTryParse(std::string_view(value), result);
    }
    [[nodiscard]] inline bool BooleanTryParse(const char* value, bool& result) noexcept
    {
        return BooleanTryParse(std::string_view(value), result);
    }
}
