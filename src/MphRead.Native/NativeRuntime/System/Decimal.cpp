#include "Decimal.hpp"

#include "Exceptions.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace System
{
    namespace
    {
        struct BigUInt final
        {
            std::vector<std::uint32_t> Limbs;

            BigUInt() = default;
            explicit BigUInt(std::uint64_t value)
            {
                if (value != 0)
                {
                    Limbs.push_back(static_cast<std::uint32_t>(value));
                    const std::uint32_t high = static_cast<std::uint32_t>(value >> 32);
                    if (high != 0)
                    {
                        Limbs.push_back(high);
                    }
                }
            }
            BigUInt(std::uint32_t lo, std::uint32_t mid, std::uint32_t hi)
            {
                Limbs = {lo, mid, hi};
                Normalize();
            }

            void Normalize() noexcept
            {
                while (!Limbs.empty() && Limbs.back() == 0)
                {
                    Limbs.pop_back();
                }
            }

            [[nodiscard]] bool IsZero() const noexcept { return Limbs.empty(); }
            [[nodiscard]] bool IsOdd() const noexcept
            {
                return !Limbs.empty() && (Limbs[0] & 1U) != 0;
            }

            [[nodiscard]] int Compare(const BigUInt& other) const noexcept
            {
                if (Limbs.size() != other.Limbs.size())
                {
                    return Limbs.size() < other.Limbs.size() ? -1 : 1;
                }
                for (std::size_t i = Limbs.size(); i > 0; --i)
                {
                    const std::uint32_t left = Limbs[i - 1];
                    const std::uint32_t right = other.Limbs[i - 1];
                    if (left != right)
                    {
                        return left < right ? -1 : 1;
                    }
                }
                return 0;
            }

            void AddSmall(std::uint32_t value)
            {
                std::uint64_t carry = value;
                std::size_t i = 0;
                while (carry != 0)
                {
                    if (i == Limbs.size())
                    {
                        Limbs.push_back(0);
                    }
                    const std::uint64_t sum = static_cast<std::uint64_t>(Limbs[i]) + carry;
                    Limbs[i] = static_cast<std::uint32_t>(sum);
                    carry = sum >> 32;
                    ++i;
                }
            }

            void MultiplySmall(std::uint32_t value)
            {
                if (value == 0 || IsZero())
                {
                    Limbs.clear();
                    return;
                }
                std::uint64_t carry = 0;
                for (std::uint32_t& limb : Limbs)
                {
                    const std::uint64_t product
                        = static_cast<std::uint64_t>(limb) * value + carry;
                    limb = static_cast<std::uint32_t>(product);
                    carry = product >> 32;
                }
                if (carry != 0)
                {
                    Limbs.push_back(static_cast<std::uint32_t>(carry));
                }
            }

            [[nodiscard]] std::uint32_t DivideSmall(std::uint32_t divisor)
            {
                std::uint64_t remainder = 0;
                for (std::size_t i = Limbs.size(); i > 0; --i)
                {
                    const std::uint64_t current = (remainder << 32) | Limbs[i - 1];
                    Limbs[i - 1] = static_cast<std::uint32_t>(current / divisor);
                    remainder = current % divisor;
                }
                Normalize();
                return static_cast<std::uint32_t>(remainder);
            }

            void MultiplyPower10(std::int32_t power)
            {
                for (std::int32_t i = 0; i < power; ++i)
                {
                    MultiplySmall(10);
                }
            }

            void ShiftLeftOne()
            {
                std::uint64_t carry = 0;
                for (std::uint32_t& limb : Limbs)
                {
                    const std::uint64_t shifted
                        = (static_cast<std::uint64_t>(limb) << 1) | carry;
                    limb = static_cast<std::uint32_t>(shifted);
                    carry = shifted >> 32;
                }
                if (carry != 0)
                {
                    Limbs.push_back(static_cast<std::uint32_t>(carry));
                }
            }

            [[nodiscard]] std::size_t BitLength() const noexcept
            {
                if (Limbs.empty())
                {
                    return 0;
                }
                return (Limbs.size() - 1) * 32U
                    + static_cast<std::size_t>(32 - std::countl_zero(Limbs.back()));
            }

            [[nodiscard]] bool GetBit(std::size_t bit) const noexcept
            {
                const std::size_t limb = bit / 32U;
                if (limb >= Limbs.size())
                {
                    return false;
                }
                return ((Limbs[limb] >> (bit % 32U)) & 1U) != 0;
            }

            void SetBit(std::size_t bit)
            {
                const std::size_t limb = bit / 32U;
                if (Limbs.size() <= limb)
                {
                    Limbs.resize(limb + 1, 0);
                }
                Limbs[limb] |= std::uint32_t{1} << (bit % 32U);
            }
        };

        [[nodiscard]] BigUInt AddBig(const BigUInt& left, const BigUInt& right)
        {
            BigUInt result;
            result.Limbs.resize(std::max(left.Limbs.size(), right.Limbs.size()), 0);
            std::uint64_t carry = 0;
            for (std::size_t i = 0; i < result.Limbs.size(); ++i)
            {
                const std::uint64_t a = i < left.Limbs.size() ? left.Limbs[i] : 0;
                const std::uint64_t b = i < right.Limbs.size() ? right.Limbs[i] : 0;
                const std::uint64_t sum = a + b + carry;
                result.Limbs[i] = static_cast<std::uint32_t>(sum);
                carry = sum >> 32;
            }
            if (carry != 0)
            {
                result.Limbs.push_back(static_cast<std::uint32_t>(carry));
            }
            return result;
        }

        [[nodiscard]] BigUInt SubtractBig(const BigUInt& left, const BigUInt& right)
        {
            BigUInt result = left;
            std::uint64_t borrow = 0;
            for (std::size_t i = 0; i < result.Limbs.size(); ++i)
            {
                const std::uint64_t a = result.Limbs[i];
                const std::uint64_t b = (i < right.Limbs.size() ? right.Limbs[i] : 0) + borrow;
                result.Limbs[i] = static_cast<std::uint32_t>(a - b);
                borrow = a < b ? 1 : 0;
            }
            result.Normalize();
            return result;
        }

        [[nodiscard]] BigUInt MultiplyBig(const BigUInt& left, const BigUInt& right)
        {
            if (left.IsZero() || right.IsZero())
            {
                return {};
            }
            BigUInt result;
            result.Limbs.assign(left.Limbs.size() + right.Limbs.size(), 0);
            for (std::size_t i = 0; i < left.Limbs.size(); ++i)
            {
                std::uint64_t carry = 0;
                for (std::size_t j = 0; j < right.Limbs.size(); ++j)
                {
                    const std::size_t index = i + j;
                    const std::uint64_t current
                        = static_cast<std::uint64_t>(left.Limbs[i]) * right.Limbs[j]
                        + result.Limbs[index] + carry;
                    result.Limbs[index] = static_cast<std::uint32_t>(current);
                    carry = current >> 32;
                }
                std::size_t index = i + right.Limbs.size();
                while (carry != 0)
                {
                    const std::uint64_t current
                        = static_cast<std::uint64_t>(result.Limbs[index]) + carry;
                    result.Limbs[index] = static_cast<std::uint32_t>(current);
                    carry = current >> 32;
                    ++index;
                    if (carry != 0 && index == result.Limbs.size())
                    {
                        result.Limbs.push_back(0);
                    }
                }
            }
            result.Normalize();
            return result;
        }

        [[nodiscard]] std::pair<BigUInt, BigUInt> DivideBig(
            const BigUInt& numerator, const BigUInt& denominator)
        {
            if (denominator.IsZero())
            {
                throw DivideByZeroException();
            }
            BigUInt quotient;
            BigUInt remainder;
            const std::size_t bits = numerator.BitLength();
            for (std::size_t i = bits; i > 0; --i)
            {
                remainder.ShiftLeftOne();
                if (numerator.GetBit(i - 1))
                {
                    remainder.AddSmall(1);
                }
                if (remainder.Compare(denominator) >= 0)
                {
                    remainder = SubtractBig(remainder, denominator);
                    quotient.SetBit(i - 1);
                }
            }
            quotient.Normalize();
            remainder.Normalize();
            return {std::move(quotient), std::move(remainder)};
        }

        [[nodiscard]] BigUInt DecimalMaxCoefficient()
        {
            return BigUInt(
                std::numeric_limits<std::uint32_t>::max(),
                std::numeric_limits<std::uint32_t>::max(),
                std::numeric_limits<std::uint32_t>::max());
        }

        [[nodiscard]] BigUInt RoundDividePower10(const BigUInt& value, std::int32_t power)
        {
            if (power <= 0)
            {
                return value;
            }
            BigUInt divisor(1);
            divisor.MultiplyPower10(power);
            auto [quotient, remainder] = DivideBig(value, divisor);
            BigUInt twice = remainder;
            twice.MultiplySmall(2);
            const int comparison = twice.Compare(divisor);
            if (comparison > 0 || (comparison == 0 && quotient.IsOdd()))
            {
                quotient.AddSmall(1);
            }
            return quotient;
        }

        [[nodiscard]] Decimal MakeDecimal(BigUInt coefficient, std::int32_t scale, bool negative)
        {
            if (scale < 0)
            {
                coefficient.MultiplyPower10(-scale);
                scale = 0;
            }
            if (coefficient.IsZero())
            {
                negative = false;
                scale = std::clamp(scale, 0, 28);
            }

            const BigUInt maximum = DecimalMaxCoefficient();
            const std::int32_t minimumDrop = std::max(0, scale - 28);
            for (std::int32_t drop = minimumDrop; drop <= scale; ++drop)
            {
                BigUInt rounded = RoundDividePower10(coefficient, drop);
                if (rounded.Compare(maximum) <= 0)
                {
                    const std::int32_t resultScale = scale - drop;
                    const std::uint32_t lo = rounded.Limbs.size() > 0 ? rounded.Limbs[0] : 0;
                    const std::uint32_t mid = rounded.Limbs.size() > 1 ? rounded.Limbs[1] : 0;
                    const std::uint32_t hi = rounded.Limbs.size() > 2 ? rounded.Limbs[2] : 0;
                    return Decimal(
                        std::bit_cast<std::int32_t>(lo),
                        std::bit_cast<std::int32_t>(mid),
                        std::bit_cast<std::int32_t>(hi),
                        negative && !rounded.IsZero(),
                        static_cast<std::uint8_t>(resultScale));
                }
            }
            throw OverflowException("Value was either too large or too small for a Decimal.");
        }

        [[nodiscard]] BigUInt ScaledCoefficient(
            std::uint32_t lo, std::uint32_t mid, std::uint32_t hi,
            std::uint8_t sourceScale, std::uint8_t targetScale)
        {
            BigUInt value(lo, mid, hi);
            value.MultiplyPower10(static_cast<std::int32_t>(targetScale - sourceScale));
            return value;
        }
    }

    Decimal::Decimal(std::int32_t value) noexcept
    {
        const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
        _negative = value < 0;
        _lo = _negative ? 0U - bits : bits;
    }

    Decimal::Decimal(std::uint32_t value) noexcept
        : _lo(value)
    {
    }

    Decimal::Decimal(std::int64_t value) noexcept
    {
        const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
        const std::uint64_t magnitude = value < 0 ? 0ULL - bits : bits;
        _negative = value < 0;
        _lo = static_cast<std::uint32_t>(magnitude);
        _mid = static_cast<std::uint32_t>(magnitude >> 32);
    }

    Decimal::Decimal(std::uint64_t value) noexcept
        : _lo(static_cast<std::uint32_t>(value)),
          _mid(static_cast<std::uint32_t>(value >> 32))
    {
    }

    const Decimal Decimal::Zero{};
    const Decimal Decimal::One{std::int32_t{1}};
    const Decimal Decimal::MinusOne{std::int32_t{-1}};
    const Decimal Decimal::MaxValue{
        std::int32_t{-1}, std::int32_t{-1}, std::int32_t{-1}, false, 0};
    const Decimal Decimal::MinValue{
        std::int32_t{-1}, std::int32_t{-1}, std::int32_t{-1}, true, 0};

    Decimal::Decimal(std::int32_t lo, std::int32_t mid, std::int32_t hi,
        bool isNegative, std::uint8_t scale)
        : _lo(std::bit_cast<std::uint32_t>(lo)),
          _mid(std::bit_cast<std::uint32_t>(mid)),
          _hi(std::bit_cast<std::uint32_t>(hi)),
          _scale(scale),
          _negative(isNegative)
    {
        if (scale > 28)
        {
            throw ArgumentOutOfRangeException("scale");
        }
    }

    Decimal Decimal::FromParts(
        std::uint32_t lo, std::uint32_t mid, std::uint32_t hi,
        std::uint8_t scale, bool negative) noexcept
    {
        Decimal result;
        result._lo = lo;
        result._mid = mid;
        result._hi = hi;
        result._scale = scale;
        result._negative = negative;
        return result;
    }

    Decimal Decimal::DivideInt32By65536(std::int32_t value) noexcept
    {
        Decimal result(value);
        result._scale = 16;

        // 1 / 2^16 == 5^16 / 10^16. Multiply the 96-bit coefficient by 5^16.
        for (int factor = 0; factor < 16; ++factor)
        {
            std::uint64_t carry = 0;
            std::uint64_t product = static_cast<std::uint64_t>(result._lo) * 5U + carry;
            result._lo = static_cast<std::uint32_t>(product);
            carry = product >> 32;
            product = static_cast<std::uint64_t>(result._mid) * 5U + carry;
            result._mid = static_cast<std::uint32_t>(product);
            carry = product >> 32;
            product = static_cast<std::uint64_t>(result._hi) * 5U + carry;
            result._hi = static_cast<std::uint32_t>(product);
        }

        while (result._scale > 0)
        {
            std::array<std::uint32_t, 3> quotient{};
            std::uint64_t remainder = 0;
            const std::array<std::uint32_t, 3> limbs{result._lo, result._mid, result._hi};
            for (int i = 2; i >= 0; --i)
            {
                const std::uint64_t current = (remainder << 32)
                    | limbs[static_cast<std::size_t>(i)];
                quotient[static_cast<std::size_t>(i)] = static_cast<std::uint32_t>(current / 10U);
                remainder = current % 10U;
            }
            if (remainder != 0)
            {
                break;
            }
            result._lo = quotient[0];
            result._mid = quotient[1];
            result._hi = quotient[2];
            --result._scale;
        }
        if (result._lo == 0 && result._mid == 0 && result._hi == 0)
        {
            result._negative = false;
        }
        return result;
    }

    Decimal Decimal::Add(const Decimal& left, const Decimal& right)
    {
        return left + right;
    }

    Decimal Decimal::Subtract(const Decimal& left, const Decimal& right)
    {
        return left - right;
    }

    Decimal Decimal::Multiply(const Decimal& left, const Decimal& right)
    {
        return left * right;
    }

    Decimal Decimal::Divide(const Decimal& left, const Decimal& right)
    {
        return left / right;
    }

    Decimal Decimal::Remainder(const Decimal& left, const Decimal& right)
    {
        return left % right;
    }

    Decimal Decimal::Negate(const Decimal& value)
    {
        return -value;
    }

    Decimal Decimal::Abs(const Decimal& value)
    {
        return value._negative ? -value : value;
    }

    std::int32_t Decimal::Compare(const Decimal& left, const Decimal& right)
    {
        return left.CompareTo(right);
    }

    bool Decimal::Equals(const Decimal& left, const Decimal& right)
    {
        return left.Equals(right);
    }

    std::array<std::int32_t, 4> Decimal::GetBits(const Decimal& value) noexcept
    {
        const std::uint32_t flags = (static_cast<std::uint32_t>(value._scale) << 16)
            | (value._negative ? 0x80000000U : 0U);
        return {
            std::bit_cast<std::int32_t>(value._lo),
            std::bit_cast<std::int32_t>(value._mid),
            std::bit_cast<std::int32_t>(value._hi),
            std::bit_cast<std::int32_t>(flags)};
    }

    std::int32_t Decimal::CompareTo(const Decimal& other) const
    {
        const BigUInt leftRaw(_lo, _mid, _hi);
        const BigUInt rightRaw(other._lo, other._mid, other._hi);
        if (leftRaw.IsZero() && rightRaw.IsZero())
        {
            return 0;
        }
        if (_negative != other._negative)
        {
            return _negative ? -1 : 1;
        }
        const std::uint8_t scale = std::max(_scale, other._scale);
        const BigUInt left = ScaledCoefficient(_lo, _mid, _hi, _scale, scale);
        const BigUInt right = ScaledCoefficient(
            other._lo, other._mid, other._hi, other._scale, scale);
        const int comparison = left.Compare(right);
        if (comparison == 0)
        {
            return 0;
        }
        return _negative ? -comparison : comparison;
    }

    bool Decimal::Equals(const Decimal& other) const
    {
        return CompareTo(other) == 0;
    }

    float Decimal::ToSingle() const noexcept
    {
        return static_cast<float>(ToDouble());
    }

    double Decimal::ToDouble() const noexcept
    {
        // DecCalc.VarR8FromDec.
        static constexpr double Powers10[] = {
            1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14,
            1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22, 1e23, 1e24, 1e25, 1e26, 1e27, 1e28,
        };
        const std::uint64_t low64 = (static_cast<std::uint64_t>(_mid) << 32) | _lo;
        double value = (static_cast<double>(low64) + static_cast<double>(_hi) * 18446744073709551616.0)
            / Powers10[_scale];
        if (_negative)
        {
            value = -value;
        }
        return value;
    }

    ::MphRead::NativeRuntime::DecimalBits Decimal::Bits() const noexcept
    {
        return ::MphRead::NativeRuntime::DecimalBits{_lo, _mid, _hi, _scale, _negative};
    }

    Decimal Decimal::FromBits(const ::MphRead::NativeRuntime::DecimalBits& bits) noexcept
    {
        return FromParts(bits.Lo, bits.Mid, bits.Hi, bits.Scale, bits.Negative);
    }

    std::string Decimal::ToString(std::string_view format) const
    {
        return ToString(format, ::MphRead::NativeRuntime::NumberFormatInfo::CurrentInfo());
    }

    std::string Decimal::ToString(std::string_view format,
        const ::MphRead::NativeRuntime::NumberFormatInfo& info) const
    {
        return ::MphRead::NativeRuntime::DecimalToString(Bits(), format, info);
    }

    bool Decimal::TryParse(std::string_view text, Decimal& result)
    {
        return TryParse(text, ::MphRead::NativeRuntime::NumberStyles::Number,
            ::MphRead::NativeRuntime::NumberFormatInfo::CurrentInfo(), result);
    }

    bool Decimal::TryParse(std::string_view text, ::MphRead::NativeRuntime::NumberStyles styles,
        const ::MphRead::NativeRuntime::NumberFormatInfo& info, Decimal& result)
    {
        ::MphRead::NativeRuntime::DecimalBits bits;
        const bool parsed = ::MphRead::NativeRuntime::TryParseDecimal(text, styles, info, bits);
        result = parsed ? FromBits(bits) : Decimal{};
        return parsed;
    }

    Decimal Decimal::ParseInvariant(std::string_view text)
    {
        Decimal result;
        if (!TryParse(text, ::MphRead::NativeRuntime::NumberStyles::Number,
                ::MphRead::NativeRuntime::NumberFormatInfo::InvariantInfo(), result))
        {
            throw FormatException();
        }
        return result;
    }

    std::int32_t Decimal::ToInt32() const
    {
        // decimal.ToInt32: truncate toward zero, then range-check.
        BigUInt value(_lo, _mid, _hi);
        for (std::uint8_t i = 0; i < _scale; ++i)
        {
            (void)value.DivideSmall(10);
        }
        value.Normalize();
        const std::uint64_t limit = _negative ? 2147483648ULL : 2147483647ULL;
        if (value.Limbs.size() > 1 || (!value.Limbs.empty() && value.Limbs[0] > limit))
        {
            throw OverflowException("Value was either too large or too small for an Int32.");
        }
        const std::int64_t magnitude = value.Limbs.empty() ? 0 : value.Limbs[0];
        return static_cast<std::int32_t>(_negative ? -magnitude : magnitude);
    }

    bool operator==(const Decimal& left, const Decimal& right)
    {
        return left.CompareTo(right) == 0;
    }

    bool operator!=(const Decimal& left, const Decimal& right)
    {
        return !(left == right);
    }

    bool operator<(const Decimal& left, const Decimal& right)
    {
        return left.CompareTo(right) < 0;
    }

    bool operator<=(const Decimal& left, const Decimal& right)
    {
        return left.CompareTo(right) <= 0;
    }

    bool operator>(const Decimal& left, const Decimal& right)
    {
        return left.CompareTo(right) > 0;
    }

    bool operator>=(const Decimal& left, const Decimal& right)
    {
        return left.CompareTo(right) >= 0;
    }

    Decimal operator-(const Decimal& value)
    {
        return Decimal::FromParts(
            value._lo, value._mid, value._hi, value._scale,
            (value._lo != 0 || value._mid != 0 || value._hi != 0) && !value._negative);
    }

    Decimal operator+(const Decimal& left, const Decimal& right)
    {
        const std::uint8_t scale = std::max(left._scale, right._scale);
        const BigUInt leftValue = ScaledCoefficient(
            left._lo, left._mid, left._hi, left._scale, scale);
        const BigUInt rightValue = ScaledCoefficient(
            right._lo, right._mid, right._hi, right._scale, scale);
        if (left._negative == right._negative)
        {
            return MakeDecimal(AddBig(leftValue, rightValue), scale, left._negative);
        }
        const int comparison = leftValue.Compare(rightValue);
        if (comparison == 0)
        {
            return MakeDecimal({}, scale, false);
        }
        if (comparison > 0)
        {
            return MakeDecimal(SubtractBig(leftValue, rightValue), scale, left._negative);
        }
        return MakeDecimal(SubtractBig(rightValue, leftValue), scale, right._negative);
    }

    Decimal operator-(const Decimal& left, const Decimal& right)
    {
        return left + (-right);
    }

    Decimal operator*(const Decimal& left, const Decimal& right)
    {
        const BigUInt leftValue(left._lo, left._mid, left._hi);
        const BigUInt rightValue(right._lo, right._mid, right._hi);
        return MakeDecimal(
            MultiplyBig(leftValue, rightValue),
            static_cast<std::int32_t>(left._scale) + right._scale,
            left._negative != right._negative);
    }

    Decimal operator/(const Decimal& left, const Decimal& right)
    {
        const BigUInt numeratorBase(left._lo, left._mid, left._hi);
        const BigUInt denominatorBase(right._lo, right._mid, right._hi);
        if (denominatorBase.IsZero())
        {
            throw DivideByZeroException();
        }
        if (numeratorBase.IsZero())
        {
            return Decimal();
        }

        const BigUInt maximum = DecimalMaxCoefficient();
        for (std::int32_t targetScale = 28; targetScale >= 0; --targetScale)
        {
            BigUInt numerator = numeratorBase;
            BigUInt denominator = denominatorBase;
            const std::int32_t exponent = targetScale
                + static_cast<std::int32_t>(right._scale)
                - static_cast<std::int32_t>(left._scale);
            if (exponent >= 0)
            {
                numerator.MultiplyPower10(exponent);
            }
            else
            {
                denominator.MultiplyPower10(-exponent);
            }

            auto [quotient, remainder] = DivideBig(numerator, denominator);
            BigUInt twice = remainder;
            twice.MultiplySmall(2);
            const int comparison = twice.Compare(denominator);
            if (comparison > 0 || (comparison == 0 && quotient.IsOdd()))
            {
                quotient.AddSmall(1);
            }
            if (quotient.Compare(maximum) > 0)
            {
                continue;
            }

            std::int32_t scale = targetScale;
            while (scale > 0)
            {
                BigUInt reduced = quotient;
                if (reduced.DivideSmall(10) != 0)
                {
                    break;
                }
                quotient = std::move(reduced);
                --scale;
            }
            const std::uint32_t lo = quotient.Limbs.size() > 0 ? quotient.Limbs[0] : 0;
            const std::uint32_t mid = quotient.Limbs.size() > 1 ? quotient.Limbs[1] : 0;
            const std::uint32_t hi = quotient.Limbs.size() > 2 ? quotient.Limbs[2] : 0;
            return Decimal::FromParts(
                lo, mid, hi, static_cast<std::uint8_t>(scale),
                left._negative != right._negative);
        }
        throw OverflowException("Value was either too large or too small for a Decimal.");
    }

    Decimal operator%(const Decimal& left, const Decimal& right)
    {
        const std::uint8_t scale = std::max(left._scale, right._scale);
        const BigUInt leftValue = ScaledCoefficient(
            left._lo, left._mid, left._hi, left._scale, scale);
        const BigUInt rightValue = ScaledCoefficient(
            right._lo, right._mid, right._hi, right._scale, scale);
        if (rightValue.IsZero())
        {
            throw DivideByZeroException();
        }
        auto [quotient, remainder] = DivideBig(leftValue, rightValue);
        (void)quotient;
        return MakeDecimal(std::move(remainder), scale, left._negative);
    }
}
