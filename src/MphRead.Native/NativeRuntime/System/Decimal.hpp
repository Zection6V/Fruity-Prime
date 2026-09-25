#pragma once

// System.Decimal: the 96-bit coefficient, sign and scale, with .NET's
// arithmetic, rounding, parsing and formatting.

#include "Number.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace System
{
    // Mechanical value-type representation of System.Decimal. The 96-bit integer,
    // sign and scale fields match the CLR decimal value domain without binary-FP loss.
    class Decimal final
    {
    public:
        constexpr Decimal() noexcept = default;
        explicit Decimal(std::int32_t value) noexcept;
        explicit Decimal(std::uint32_t value) noexcept;
        explicit Decimal(std::int64_t value) noexcept;
        explicit Decimal(std::uint64_t value) noexcept;
        Decimal(std::int32_t lo, std::int32_t mid, std::int32_t hi,
            bool isNegative, std::uint8_t scale);

        static const Decimal Zero;
        static const Decimal One;
        static const Decimal MinusOne;
        static const Decimal MaxValue;
        static const Decimal MinValue;

        [[nodiscard]] static Decimal DivideInt32By65536(std::int32_t value) noexcept;
        [[nodiscard]] static Decimal Add(const Decimal& left, const Decimal& right);
        [[nodiscard]] static Decimal Subtract(const Decimal& left, const Decimal& right);
        [[nodiscard]] static Decimal Multiply(const Decimal& left, const Decimal& right);
        [[nodiscard]] static Decimal Divide(const Decimal& left, const Decimal& right);
        [[nodiscard]] static Decimal Remainder(const Decimal& left, const Decimal& right);
        [[nodiscard]] static Decimal Negate(const Decimal& value);
        [[nodiscard]] static Decimal Abs(const Decimal& value);
        [[nodiscard]] static std::int32_t Compare(const Decimal& left, const Decimal& right);
        [[nodiscard]] static bool Equals(const Decimal& left, const Decimal& right);
        [[nodiscard]] std::int32_t CompareTo(const Decimal& other) const;
        [[nodiscard]] bool Equals(const Decimal& other) const;
        [[nodiscard]] static std::array<std::int32_t, 4> GetBits(const Decimal& value) noexcept;
        [[nodiscard]] float ToSingle() const noexcept;
        [[nodiscard]] double ToDouble() const noexcept;
        // value.ToString(format) / ToString(format, info): no format keeps
        // every digit the value carries ("1.50").
        [[nodiscard]] std::string ToString(std::string_view format = {}) const;
        [[nodiscard]] std::string ToString(std::string_view format,
            const ::MphRead::NativeRuntime::NumberFormatInfo& info) const;
        // decimal.TryParse(text, out value): NumberStyles.Number, current culture.
        [[nodiscard]] static bool TryParse(std::string_view text, Decimal& result);
        [[nodiscard]] static bool TryParse(std::string_view text,
            ::MphRead::NativeRuntime::NumberStyles styles,
            const ::MphRead::NativeRuntime::NumberFormatInfo& info, Decimal& result);
        // A C# decimal literal: decimal.Parse(text, CultureInfo.InvariantCulture).
        [[nodiscard]] static Decimal ParseInvariant(std::string_view text);
        // (int)value: truncates, OverflowException out of range.
        [[nodiscard]] std::int32_t ToInt32() const;
        [[nodiscard]] ::MphRead::NativeRuntime::DecimalBits Bits() const noexcept;
        [[nodiscard]] static Decimal FromBits(const ::MphRead::NativeRuntime::DecimalBits& bits) noexcept;

        friend bool operator==(const Decimal& left, const Decimal& right);
        friend bool operator!=(const Decimal& left, const Decimal& right);
        friend bool operator<(const Decimal& left, const Decimal& right);
        friend bool operator<=(const Decimal& left, const Decimal& right);
        friend bool operator>(const Decimal& left, const Decimal& right);
        friend bool operator>=(const Decimal& left, const Decimal& right);
        friend Decimal operator-(const Decimal& value);
        friend Decimal operator+(const Decimal& left, const Decimal& right);
        friend Decimal operator-(const Decimal& left, const Decimal& right);
        friend Decimal operator*(const Decimal& left, const Decimal& right);
        friend Decimal operator/(const Decimal& left, const Decimal& right);
        friend Decimal operator%(const Decimal& left, const Decimal& right);

    private:
        std::uint32_t _lo = 0;
        std::uint32_t _mid = 0;
        std::uint32_t _hi = 0;
        std::uint8_t _scale = 0;
        bool _negative = false;

        [[nodiscard]] static Decimal FromParts(
            std::uint32_t lo, std::uint32_t mid, std::uint32_t hi,
            std::uint8_t scale, bool negative) noexcept;
    };
}
