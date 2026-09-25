#include "Movie.hpp"
#include "../NativeRuntime/System/HashCode.hpp"
#include "../NativeRuntime/System/ThreadStatic.hpp"

#include "../GameState.hpp"
#include "../Scene.hpp"
#include "../Shaders.hpp"
#include "../Entities/RoomEntity.hpp"
#include "../Entities/Players/PlayerEntity.hpp"
#include "../Metadata/FrontendMeta.hpp"
#include "../NativeRuntime/OpenTK/AL.hpp"
#include "../NativeRuntime/OpenTK/GL.hpp"
#include "../NativeRuntime/System/Tasks.hpp"
#include "../Sound/Music.hpp"
#include "../Sound/Sfx.hpp"
#include "../NativeRuntime/System/IO.hpp"
#include "../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <climits>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <new>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

using ::MphRead::NativeRuntime::FileExists;
using ::MphRead::NativeRuntime::MathClamp;
using ::MphRead::NativeRuntime::PathFromUtf8;
using ::MphRead::NativeRuntime::ShiftLeft;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedMultiply;
using ::MphRead::NativeRuntime::UncheckedNegate;
using ::MphRead::NativeRuntime::UncheckedSubtract;

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

        [[nodiscard]] std::string BigUIntToString(BigUInt value)
        {
            if (value.IsZero())
            {
                return "0";
            }
            std::vector<std::uint32_t> groups;
            while (!value.IsZero())
            {
                groups.push_back(value.DivideSmall(1'000'000'000U));
            }
            std::ostringstream stream;
            stream << groups.back();
            for (std::size_t i = groups.size() - 1; i > 0; --i)
            {
                stream << std::setw(9) << std::setfill('0') << groups[i - 1];
            }
            return stream.str();
        }
    }

    namespace
    {
        std::locale MakeCurrentCultureLocale()
        {
            return std::locale();
        }

        ::MphRead::NativeRuntime::ThreadStatic<std::locale> CurrentCultureLocaleSlot(
            &MakeCurrentCultureLocale);
    }

    Globalization::CultureInfo::CultureInfo()
        : _locale(CurrentCultureLocaleSlot.Value())
    {
    }

    Globalization::CultureInfo::CultureInfo(std::locale locale)
        : _locale(std::move(locale))
    {
    }

    Globalization::CultureInfo Globalization::CultureInfo::CurrentCulture()
    {
        return CultureInfo(CurrentCultureLocaleSlot.Value());
    }

    void Globalization::CultureInfo::SetCurrentCulture(CultureInfo culture)
    {
        CurrentCultureLocaleSlot.Value() = std::move(culture._locale);
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
        long double value = static_cast<long double>(_hi) * 18446744073709551616.0L
            + static_cast<long double>(_mid) * 4294967296.0L
            + static_cast<long double>(_lo);
        for (std::uint8_t i = 0; i < _scale; ++i)
        {
            value /= 10.0L;
        }
        if (_negative)
        {
            value = -value;
        }
        return static_cast<double>(value);
    }

    std::string Decimal::ToString() const
    {
        return ToString(Globalization::CultureInfo::CurrentCulture());
    }

    std::string Decimal::ToString(const Globalization::CultureInfo& culture) const
    {
        std::string digits = BigUIntToString(BigUInt(_lo, _mid, _hi));
        const std::locale& locale = culture.NativeLocale();
        const char decimalPoint = std::use_facet<std::numpunct<char>>(locale).decimal_point();
        std::string negativeSign = std::use_facet<std::moneypunct<char>>(locale).negative_sign();
        if (negativeSign.empty())
        {
            negativeSign = "-";
        }
        if (_scale != 0)
        {
            const std::size_t scale = _scale;
            if (digits.size() <= scale)
            {
                digits.insert(0, scale + 1 - digits.size(), '0');
            }
            digits.insert(digits.size() - scale, 1, decimalPoint);
        }
        if (_negative && (_lo != 0 || _mid != 0 || _hi != 0))
        {
            digits.insert(0, negativeSign);
        }
        return digits;
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
namespace MphRead::Formats::MovieNativeRuntime
{
    struct DecoderLifetime
    {
        std::mutex Mutex;
        std::condition_variable Condition;
        std::size_t Active = 0;
    };

    struct ContinuationRegistration final
    {
        std::coroutine_handle<> Handle{};
        std::shared_ptr<SynchronizationContext> Context{};
        std::shared_ptr<TaskScheduler> Scheduler{};
        System::Globalization::CultureInfo Culture{};
    };

    struct TaskState : std::enable_shared_from_this<TaskState>
    {
        std::mutex Mutex;
        std::condition_variable Condition;
        bool Done = false;
        std::exception_ptr Exception{};
        std::coroutine_handle<> Handle{};
        std::vector<ContinuationRegistration> Continuations{};
        std::shared_ptr<TaskState> SelfKeepAlive{};

        ~TaskState()
        {
            if (Handle)
            {
                Handle.destroy();
            }
        }

        static void ScheduleOn(
            const std::shared_ptr<TaskScheduler>& scheduler,
            std::function<void()> continuation)
        {
            scheduler->Schedule(std::move(continuation));
        }
    };

    namespace
    {
        // [ThreadStatic] fields. See NativeRuntime/System/ThreadStatic.hpp.
        ::MphRead::NativeRuntime::ThreadStatic<std::shared_ptr<SynchronizationContext>>
            CurrentSynchronizationContextSlot;
        ::MphRead::NativeRuntime::ThreadStatic<std::shared_ptr<TaskScheduler>>
            CurrentTaskSchedulerSlot;

        std::shared_ptr<SynchronizationContext>& CurrentSynchronizationContextRef()
        {
            return CurrentSynchronizationContextSlot.Value();
        }

        std::shared_ptr<TaskScheduler>& CurrentTaskSchedulerRef()
        {
            return CurrentTaskSchedulerSlot.Value();
        }

        class DefaultTaskScheduler final : public TaskScheduler
        {
        protected:
            void Queue(std::function<void()> continuation) override
            {
                continuation();
            }
        };

        [[nodiscard]] std::shared_ptr<TaskScheduler> DefaultSchedulerInstance()
        {
            static std::shared_ptr<TaskScheduler> scheduler
                = std::make_shared<DefaultTaskScheduler>();
            return scheduler;
        }

        [[nodiscard]] ContinuationRegistration CaptureContinuation(
            std::coroutine_handle<> handle)
        {
            std::shared_ptr<SynchronizationContext> context = SynchronizationContext::Current();
            std::shared_ptr<TaskScheduler> scheduler;
            if (!context)
            {
                scheduler = TaskScheduler::Current();
                if (scheduler == TaskScheduler::Default())
                {
                    scheduler.reset();
                }
            }
            return ContinuationRegistration{
                handle, std::move(context), std::move(scheduler),
                System::Globalization::CultureInfo::CurrentCulture()};
        }

        void ResumeWithExecutionContext(const ContinuationRegistration& registration)
        {
            if (!registration.Handle || registration.Handle.done())
            {
                return;
            }
            System::Globalization::CultureInfo previous
                = System::Globalization::CultureInfo::CurrentCulture();
            System::Globalization::CultureInfo::SetCurrentCulture(registration.Culture);
            try
            {
                registration.Handle.resume();
            }
            catch (...)
            {
                System::Globalization::CultureInfo::SetCurrentCulture(std::move(previous));
                throw;
            }
            System::Globalization::CultureInfo::SetCurrentCulture(std::move(previous));
        }

        void ResumeContinuation(ContinuationRegistration registration)
        {
            if (!registration.Handle)
            {
                return;
            }
            if (registration.Context)
            {
                std::shared_ptr<SynchronizationContext> context = registration.Context;
                context->Post(
                    [context = std::move(context), registration = std::move(registration)]() mutable
                    {
                        if (!registration.Handle || registration.Handle.done())
                        {
                            return;
                        }
                        std::shared_ptr<SynchronizationContext> previous
                            = std::move(CurrentSynchronizationContextRef());
                        CurrentSynchronizationContextRef() = context;
                        try
                        {
                            ResumeWithExecutionContext(registration);
                        }
                        catch (...)
                        {
                            CurrentSynchronizationContextRef() = std::move(previous);
                            throw;
                        }
                        CurrentSynchronizationContextRef() = std::move(previous);
                    });
            }
            else if (registration.Scheduler)
            {
                std::shared_ptr<TaskScheduler> scheduler = registration.Scheduler;
                TaskState::ScheduleOn(scheduler,
                    [registration = std::move(registration)]() mutable
                    {
                        ResumeWithExecutionContext(registration);
                    });
            }
            else
            {
                ResumeWithExecutionContext(registration);
            }
        }

        class DelayScheduler final
        {
        public:
            DelayScheduler()
                : _worker([this](std::stop_token token) { Run(token); })
            {
            }

            void Schedule(
                std::shared_ptr<TaskState> state,
                std::coroutine_handle<MovieTask::promise_type> handle)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _items.emplace(
                    std::chrono::steady_clock::now() + std::chrono::milliseconds(1),
                    Item{std::move(state), CaptureContinuation(handle)});
                _condition.notify_all();
            }

            void DeferRelease(std::shared_ptr<TaskState> state)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _releases.emplace(
                    std::chrono::steady_clock::now() + std::chrono::milliseconds(1),
                    std::move(state));
                _condition.notify_all();
            }

        private:
            struct Item
            {
                std::shared_ptr<TaskState> State;
                ContinuationRegistration Registration;
            };

            std::mutex _mutex;
            std::condition_variable_any _condition;
            std::multimap<std::chrono::steady_clock::time_point, Item> _items;
            std::multimap<std::chrono::steady_clock::time_point, std::shared_ptr<TaskState>> _releases;
            std::jthread _worker;

            void Run(std::stop_token token)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                while (!token.stop_requested())
                {
                    const auto now = std::chrono::steady_clock::now();
                    while (!_releases.empty() && _releases.begin()->first <= now)
                    {
                        _releases.erase(_releases.begin());
                    }

                    std::optional<std::chrono::steady_clock::time_point> due;
                    if (!_items.empty())
                    {
                        due = _items.begin()->first;
                    }
                    if (!_releases.empty() && (!due.has_value() || _releases.begin()->first < *due))
                    {
                        due = _releases.begin()->first;
                    }
                    if (!due.has_value())
                    {
                        _condition.wait(lock, token, [this]()
                            { return !_items.empty() || !_releases.empty(); });
                        continue;
                    }
                    if (*due > now)
                    {
                        _condition.wait_until(lock, token, *due, []() noexcept { return false; });
                        continue;
                    }
                    if (_items.empty() || _items.begin()->first > now)
                    {
                        continue;
                    }

                    auto first = _items.begin();
                    Item item = std::move(first->second);
                    _items.erase(first);
                    lock.unlock();

                    bool done = false;
                    {
                        std::lock_guard<std::mutex> stateLock(item.State->Mutex);
                        done = item.State->Done;
                    }
                    if (!done && item.Registration.Handle
                        && !item.Registration.Handle.done())
                    {
                        ResumeContinuation(std::move(item.Registration));
                    }
                    lock.lock();
                }
            }
        };

        DelayScheduler& Scheduler()
        {
            static DelayScheduler scheduler;
            return scheduler;
        }


    }

    std::shared_ptr<SynchronizationContext> SynchronizationContext::Current() noexcept
    {
        return CurrentSynchronizationContextRef();
    }

    void SynchronizationContext::SetSynchronizationContext(
        std::shared_ptr<SynchronizationContext> context) noexcept
    {
        CurrentSynchronizationContextRef() = std::move(context);
    }

    std::shared_ptr<TaskScheduler> TaskScheduler::Current() noexcept
    {
        return CurrentTaskSchedulerRef() ? CurrentTaskSchedulerRef() : DefaultSchedulerInstance();
    }

    std::shared_ptr<TaskScheduler> TaskScheduler::Default() noexcept
    {
        return DefaultSchedulerInstance();
    }

    void TaskScheduler::Schedule(std::function<void()> continuation)
    {
        std::shared_ptr<TaskScheduler> self = shared_from_this();
        Queue([self = std::move(self), continuation = std::move(continuation)]() mutable
        {
            std::shared_ptr<TaskScheduler> previous = std::move(CurrentTaskSchedulerRef());
            CurrentTaskSchedulerRef() = self;
            try
            {
                continuation();
            }
            catch (...)
            {
                CurrentTaskSchedulerRef() = std::move(previous);
                throw;
            }
            CurrentTaskSchedulerRef() = std::move(previous);
        });
    }

    BinaryReader::BinaryReader(std::shared_ptr<Stream> stream)
        : _stream(std::move(stream))
    {
        if (!_stream)
        {
            throw System::ArgumentNullException("input");
        }
        if (!_stream->CanRead())
        {
            throw System::ArgumentException("Stream was not readable.");
        }
    }

    BinaryReader::~BinaryReader() noexcept
    {
        try
        {
            Dispose();
        }
        catch (...)
        {
            // C++ destructors cannot reproduce a C# finally throwing during an
            // already-active unwind. DecodeCore calls Dispose explicitly so the
            // observable using/finally path still propagates the disposal exception.
        }
    }

    void BinaryReader::ThrowIfDisposed() const
    {
        if (_disposed || !_stream)
        {
            throw System::ObjectDisposedException("BinaryReader");
        }
    }

    void BinaryReader::Dispose()
    {
        if (_disposed)
        {
            return;
        }
        _disposed = true;
        std::shared_ptr<Stream> stream = std::move(_stream);
        if (stream)
        {
            stream->Dispose();
        }
    }

    void BinaryReader::ReadExact(void* destination, std::size_t size)
    {
        ThrowIfDisposed();
        auto* bytes = static_cast<std::uint8_t*>(destination);
        std::size_t total = 0;
        while (total < size)
        {
            const std::size_t read = _stream->Read(
                std::span<std::uint8_t>(bytes + total, size - total));
            if (read == 0)
            {
                throw System::IO::EndOfStreamException();
            }
            if (read > size - total)
            {
                throw std::runtime_error("Stream returned more bytes than requested.");
            }
            total += read;
        }
    }

    std::uint8_t BinaryReader::ReadByte()
    {
        std::uint8_t value = 0;
        ReadExact(&value, sizeof(value));
        return value;
    }

    char16_t BinaryReader::ReadChar()
    {
        ThrowIfDisposed();
        const bool canSeek = _stream->CanSeek();
        std::optional<std::int64_t> initialPosition;
        if (canSeek)
        {
            initialPosition = _stream->Position();
        }

        const std::uint8_t first = ReadByte();
        if (first < 0x80U)
        {
            return static_cast<char16_t>(first);
        }

        std::int32_t count = 0;
        std::uint32_t codePoint = 0;
        std::uint32_t minimum = 0;
        if ((first & 0xE0U) == 0xC0U)
        {
            count = 2;
            codePoint = first & 0x1FU;
            minimum = 0x80U;
        }
        else if ((first & 0xF0U) == 0xE0U)
        {
            count = 3;
            codePoint = first & 0x0FU;
            minimum = 0x800U;
        }
        else if ((first & 0xF8U) == 0xF0U)
        {
            count = 4;
            codePoint = first & 0x07U;
            minimum = 0x10000U;
        }
        else
        {
            return static_cast<char16_t>(0xFFFDU);
        }

        for (std::int32_t i = 1; i < count; ++i)
        {
            std::uint8_t next = 0;
            try
            {
                next = ReadByte();
            }
            catch (const System::IO::EndOfStreamException&)
            {
                return static_cast<char16_t>(0xFFFDU);
            }
            if ((next & 0xC0U) != 0x80U)
            {
                return static_cast<char16_t>(0xFFFDU);
            }
            codePoint = (codePoint << 6) | (next & 0x3FU);
        }

        if (codePoint < minimum || codePoint > 0x10FFFFU
            || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
        {
            return static_cast<char16_t>(0xFFFDU);
        }
        if (codePoint <= 0xFFFFU)
        {
            return static_cast<char16_t>(codePoint);
        }

        if (initialPosition.has_value())
        {
            _stream->Position(*initialPosition);
        }
        throw System::ArgumentException("The output char buffer is too small to contain the decoded characters.");
    }

    std::int16_t BinaryReader::ReadInt16()
    {
        const std::uint16_t value = ReadUInt16();
        return std::bit_cast<std::int16_t>(value);
    }

    std::uint16_t BinaryReader::ReadUInt16()
    {
        const std::uint16_t b0 = ReadByte();
        const std::uint16_t b1 = ReadByte();
        return static_cast<std::uint16_t>(b0 | (b1 << 8));
    }

    std::int32_t BinaryReader::ReadInt32()
    {
        std::uint32_t value = ReadByte();
        value |= static_cast<std::uint32_t>(ReadByte()) << 8;
        value |= static_cast<std::uint32_t>(ReadByte()) << 16;
        value |= static_cast<std::uint32_t>(ReadByte()) << 24;
        return std::bit_cast<std::int32_t>(value);
    }

    std::int64_t BinaryReader::Position()
    {
        ThrowIfDisposed();
        return _stream->Position();
    }

    void BinaryReader::Position(std::int64_t value)
    {
        ThrowIfDisposed();
        _stream->Position(value);
    }

    MovieTask::MovieTask(std::shared_ptr<TaskState> state) noexcept
        : _state(std::move(state))
    {
    }

    MovieTask MovieTask::promise_type::get_return_object()
    {
        auto state = std::make_shared<TaskState>();
        State = state.get();
        state->Handle = std::coroutine_handle<promise_type>::from_promise(*this);
        // A Task owns its async state machine independently of the caller's Task reference.
        state->SelfKeepAlive = state;
        return MovieTask(std::move(state));
    }

    void MovieTask::promise_type::unhandled_exception() noexcept
    {
        Exception = std::current_exception();
    }

    void MovieTask::promise_type::FinalAwaiter::await_suspend(
        std::coroutine_handle<promise_type> handle) const noexcept
    {
        promise_type& promise = handle.promise();
        TaskState* state = promise.State;
        if (promise.Lifetime)
        {
            std::lock_guard<std::mutex> lifetimeLock(promise.Lifetime->Mutex);
            if (promise.Lifetime->Active > 0)
            {
                --promise.Lifetime->Active;
            }
            promise.Lifetime->Condition.notify_all();
            promise.Lifetime.reset();
        }

        std::vector<ContinuationRegistration> continuations;
        std::shared_ptr<TaskState> deferredRelease;
        {
            std::lock_guard<std::mutex> lock(state->Mutex);
            state->Exception = promise.Exception;
            state->Done = true;
            continuations = std::move(state->Continuations);
            deferredRelease = std::move(state->SelfKeepAlive);
        }
        state->Condition.notify_all();

        for (ContinuationRegistration& continuation : continuations)
        {
            try
            {
                ResumeContinuation(std::move(continuation));
            }
            catch (...)
            {
                std::terminate();
            }
        }
        if (deferredRelease)
        {
            Scheduler().DeferRelease(std::move(deferredRelease));
        }
    }

    MovieTask::Awaiter::Awaiter(std::shared_ptr<TaskState> state) noexcept
        : _state(std::move(state))
    {
    }

    bool MovieTask::Awaiter::await_ready() const noexcept
    {
        if (!_state)
        {
            return true;
        }
        std::lock_guard<std::mutex> lock(_state->Mutex);
        return _state->Done;
    }

    bool MovieTask::Awaiter::await_suspend(std::coroutine_handle<> continuation)
    {
        if (!_state)
        {
            return false;
        }
        ContinuationRegistration registration = CaptureContinuation(continuation);
        std::lock_guard<std::mutex> lock(_state->Mutex);
        if (_state->Done)
        {
            return false;
        }
        _state->Continuations.push_back(std::move(registration));
        return true;
    }

    void MovieTask::Awaiter::await_resume()
    {
        if (!_state)
        {
            return;
        }
        std::exception_ptr exception;
        {
            std::lock_guard<std::mutex> lock(_state->Mutex);
            exception = _state->Exception;
        }
        if (exception)
        {
            std::rethrow_exception(exception);
        }
    }

    void MovieTask::Awaiter::GetResult()
    {
        if (!_state)
        {
            return;
        }
        std::unique_lock<std::mutex> lock(_state->Mutex);
        _state->Condition.wait(lock, [this]() { return _state->Done; });
        std::exception_ptr exception = _state->Exception;
        lock.unlock();
        if (exception)
        {
            std::rethrow_exception(exception);
        }
    }

    MovieTask::Awaiter MovieTask::GetAwaiter() const noexcept
    {
        return Awaiter(_state);
    }

    void MovieTask::DelayAwaitable::await_suspend(
        std::coroutine_handle<promise_type> handle) const
    {
        TaskState* raw = handle.promise().State;
        Scheduler().Schedule(raw->shared_from_this(), handle);
    }

    bool MovieTask::LifetimeAwaitable::await_suspend(
        std::coroutine_handle<promise_type> handle) const noexcept
    {
        if (Lifetime)
        {
            {
                std::lock_guard<std::mutex> lock(Lifetime->Mutex);
                ++Lifetime->Active;
            }
            handle.promise().Lifetime = Lifetime;
        }
        return false;
    }

    MovieTask::LifetimeAwaitable MovieTask::TrackLifetime(
        std::shared_ptr<DecoderLifetime> lifetime) noexcept
    {
        return LifetimeAwaitable{std::move(lifetime)};
    }
}
namespace MphRead::Formats
{

    namespace
    {
        [[nodiscard]] std::int32_t ArithmeticInt32ShiftRight(
            std::int32_t value, std::int32_t count) noexcept
        {
            const std::uint32_t shift = static_cast<std::uint32_t>(count) & 31U;
            if (shift == 0)
            {
                return value;
            }
            std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
            bits >>= shift;
            if (value < 0)
            {
                bits |= (~std::uint32_t{0}) << (32U - shift);
            }
            return std::bit_cast<std::int32_t>(bits);
        }

        [[nodiscard]] std::int32_t WrapInt32Add3(
            std::int32_t a, std::int32_t b, std::int32_t c) noexcept
        {
            return UncheckedAdd(UncheckedAdd(a, b), c);
        }


        [[nodiscard]] std::string Extension(const std::filesystem::path& path)
        {
#if defined(__cpp_char8_t)
            const std::u8string value = path.extension().u8string();
            return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
            return path.extension().u8string();
#endif
        }

        [[nodiscard]] std::string FileName(const std::filesystem::path& path)
        {
#if defined(__cpp_char8_t)
            const std::u8string value = path.filename().u8string();
            return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
            return path.filename().u8string();
#endif
        }

        [[nodiscard]] std::string PathUtf8(const std::filesystem::path& path)
        {
#if defined(__cpp_char8_t)
            const std::u8string value = path.u8string();
            return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
            return path.u8string();
#endif
        }

        [[nodiscard]] std::string Stem(const std::filesystem::path& path)
        {
#if defined(__cpp_char8_t)
            const std::u8string value = path.stem().u8string();
            return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
            return path.stem().u8string();
#endif
        }

        extern "C"
        {
            using StbiWriteFunc = void (*)(void* context, void* data, int size);
            void stbi_flip_vertically_on_write(int flag);
            int stbi_write_png_to_func(StbiWriteFunc func, void* context, int w, int h,
                int comp, const void* data, int stride_in_bytes);
        }

        void StbiStreamWrite(void* context, void* data, int size)
        {
            auto* stream = static_cast<std::ostream*>(context);
            stream->write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        }

        void WriteRgbPng(const std::string& path, std::span<const std::uint8_t> pixels,
            std::int32_t width, std::int32_t height)
        {
            std::ofstream stream(PathFromUtf8(path), std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                throw std::runtime_error("Could not create PNG output file.");
            }
            stbi_flip_vertically_on_write(0);
            const int result = stbi_write_png_to_func(
                &StbiStreamWrite, &stream, width, height, 3, pixels.data(),
                UncheckedMultiply(width, 3));
            if (result == 0 || !stream)
            {
                throw std::runtime_error("Failed to write PNG output file.");
            }
        }

        template <typename T>
        [[nodiscard]] T WrapFromUnsigned(std::make_unsigned_t<T> value) noexcept
        {
            return std::bit_cast<T>(value);
        }

        template <typename T>
        [[nodiscard]] T& RequireManagedReference(const std::shared_ptr<T>& value)
        {
            if (!value)
            {
                throw System::NullReferenceException();
            }
            return *value;
        }

        template <typename T>
        [[nodiscard]] const T& RequireManagedReference(
            const std::shared_ptr<const T>& value)
        {
            if (!value)
            {
                throw System::NullReferenceException();
            }
            return *value;
        }

        template <typename T>
        void ClearClrArray(ClrArray<T>& array)
        {
            for (std::int32_t i = 0; i < array.Length(); i = UncheckedAdd(i, 1))
            {
                array[i] = T{};
            }
        }

        template <typename T>
        void FillClrArray(ClrArray<T>& array, const T& value)
        {
            for (std::int32_t i = 0; i < array.Length(); i = UncheckedAdd(i, 1))
            {
                array[i] = value;
            }
        }

        template <typename T>
        void ClearRectArray(MovieNativeRuntime::RectArray2D<T>& array)
        {
            for (std::int32_t row = 0; row < array.GetLength(0); row = UncheckedAdd(row, 1))
            {
                for (std::int32_t column = 0; column < array.GetLength(1);
                    column = UncheckedAdd(column, 1))
                {
                    array(row, column) = T{};
                }
            }
        }
    }

    SeekTableEntry& SeekTableEntry::operator=(const SeekTableEntry& other) noexcept
    {
        if (this != std::addressof(other))
        {
            this->~SeekTableEntry();
            ::new (static_cast<void*>(this)) SeekTableEntry(other);
        }
        return *this;
    }

    Vector2ir& Vector2ir::operator=(const Vector2ir& other) noexcept
    {
        if (this != std::addressof(other))
        {
            this->~Vector2ir();
            ::new (static_cast<void*>(this)) Vector2ir(other);
        }
        return *this;
    }

    VxBuffers::VxBuffers(
        std::shared_ptr<ClrArray<std::shared_ptr<VideoFrame>>> prevVideoFrames,
        std::shared_ptr<ClrArray<std::int32_t>> quantizerTable,
        std::shared_ptr<ByteArray2D> planeBufferY,
        std::shared_ptr<ByteArray2D> planeBufferU,
        std::shared_ptr<ByteArray2D> planeBufferV,
        std::shared_ptr<ByteArray2D> coeffBufferY,
        std::shared_ptr<ByteArray2D> coeffBufferUV,
        std::shared_ptr<Vector2irArray2D> vectors,
        std::shared_ptr<ClrArray<std::int16_t>> prevSampleBuffer,
        std::shared_ptr<ClrArray<std::int32_t>> prevPulseBuffer,
        std::shared_ptr<ClrArray<std::int32_t>> lpcFilterBuffer,
        std::shared_ptr<ClrArray<std::int32_t>> influenceBuffer,
        std::shared_ptr<ClrArray<std::int16_t>> sampleBuffer)
        : PrevVideoFrames(std::move(prevVideoFrames)),
          QuantizerTable(std::move(quantizerTable)),
          PlaneBufferY(std::move(planeBufferY)),
          PlaneBufferU(std::move(planeBufferU)),
          PlaneBufferV(std::move(planeBufferV)),
          CoeffBufferY(std::move(coeffBufferY)),
          CoeffBufferUV(std::move(coeffBufferUV)),
          Vectors(std::move(vectors)),
          PrevSampleBuffer(std::move(prevSampleBuffer)),
          PrevPulseBuffer(std::move(prevPulseBuffer)),
          LpcFilterBuffer(std::move(lpcFilterBuffer)),
          InfluenceBuffer(std::move(influenceBuffer)),
          SampleBuffer(std::move(sampleBuffer))
    {
    }

    VxBuffers& VxBuffers::operator=(const VxBuffers& other) noexcept
    {
        if (this != std::addressof(other))
        {
            this->~VxBuffers();
            ::new (static_cast<void*>(this)) VxBuffers(other);
        }
        return *this;
    }

    Block& Block::operator=(const Block& other) noexcept
    {
        if (this != std::addressof(other))
        {
            this->~Block();
            ::new (static_cast<void*>(this)) Block(other);
        }
        return *this;
    }

    Block Block::HalfLeft() const noexcept
    {
        return Block(X, Y, W / 2, H);
    }

    Block Block::HalfRight() const noexcept
    {
        return Block(UncheckedAdd(X, W / 2), Y, W / 2, H);
    }

    Block Block::HalfUp() const noexcept
    {
        return Block(X, Y, W, H / 2);
    }

    Block Block::HalfDown() const noexcept
    {
        return Block(X, UncheckedAdd(Y, H / 2), W, H / 2);
    }

    const std::array<std::array<std::int32_t, 3>, 6> VxDecoder::_quantizer4x4Table{{
        {{0x0A, 0x0D, 0x10}},
        {{0x0B, 0x0E, 0x12}},
        {{0x0D, 0x10, 0x14}},
        {{0x0E, 0x12, 0x17}},
        {{0x10, 0x14, 0x19}},
        {{0x12, 0x17, 0x1D}}
    }};

    bool VxDecoder::UseStaticBuffers = true;

    VxDecoder::VxDecoder()
        : _lifetime(std::make_shared<MovieNativeRuntime::DecoderLifetime>())
    {
        for (std::int32_t frame = 0; frame < 4; frame = UncheckedAdd(frame, 1))
        {
            const std::size_t base = static_cast<std::size_t>(frame) * 3U;
            _planeBuffers[base] = std::make_shared<ByteArray2D>(_mphFrameH, _mphFrameW);
            _planeBuffers[base + 1U] = std::make_shared<ByteArray2D>(_mphFrameH / 2, _mphFrameW / 2);
            _planeBuffers[base + 2U] = std::make_shared<ByteArray2D>(_mphFrameH / 2, _mphFrameW / 2);
        }
    }

    VxDecoder::~VxDecoder()
    {
        if (_lifetime)
        {
            std::unique_lock<std::mutex> lock(_lifetime->Mutex);
            _lifetime->Condition.wait(lock, [this]() { return _lifetime->Active == 0; });
        }
    }

    namespace
    {
        struct DecoderInstances
        {
            VxDecoder One;
            VxDecoder Two;
        };

        DecoderInstances& GetDecoderInstances()
        {
            static DecoderInstances instances;
            return instances;
        }
    }

    VxDecoder& VxDecoder::Instance1()
    {
        return GetDecoderInstances().One;
    }

    VxDecoder& VxDecoder::Instance2()
    {
        return GetDecoderInstances().Two;
    }

    void VxDecoder::Reset()
    {
        {
            std::lock_guard<std::mutex> lock(_vxFramesMutex);
            if (_vxFrames)
            {
                _vxFrames->clear();
            }
        }
        _framesQueued.store(0, std::memory_order_relaxed);
        UseStaticBuffers = true;
        ClearClrArray(*_sampleBuffer);
    }

    MovieNativeRuntime::MovieTask VxDecoder::ExportAll()
    {
        co_await MovieNativeRuntime::MovieTask::TrackLifetime(_lifetime);
        Reset();
        UseStaticBuffers = false;
        std::int32_t i = 0;
        const std::string movieFolder = Paths::Combine(Paths::FileSystem(), "movies");
        std::vector<std::filesystem::path> files;
        for (const std::filesystem::directory_entry& entry
            : std::filesystem::directory_iterator(PathFromUtf8(movieFolder)))
        {
            if (entry.is_regular_file())
            {
                files.push_back(entry.path());
            }
        }
        for (const std::filesystem::path& path : files)
        {
            if (Extension(path) == ".vx")
            {
                i = UncheckedAdd(i, 1);
                std::cout << "Exporting " << i << " of " << files.size()
                    << ": " << FileName(path) << '\n';
                co_await Decode(PathUtf8(path), true);
            }
        }
        std::cout << "Done." << '\n';
        co_return;
    }

    MovieNativeRuntime::MovieTask VxDecoder::Export(const std::string& filePath)
    {
        co_await MovieNativeRuntime::MovieTask::TrackLifetime(_lifetime);
        Reset();
        std::string path = Paths::Combine(Paths::FileSystem(), "movies", filePath);
        if (!FileExists(path))
        {
            path = Paths::Combine(Paths::FileSystem(), filePath);
            if (!FileExists(path))
            {
                path = filePath;
            }
        }
        std::cout << "Exporting..." << '\n';
        UseStaticBuffers = false;
        co_await Decode(path, true);
        std::cout << "Done." << '\n';
        co_return;
    }

    MovieNativeRuntime::MovieTask VxDecoder::Decode(
        const std::string& filePath, bool writeFiles, std::stop_token token)
    {
        co_await MovieNativeRuntime::MovieTask::TrackLifetime(_lifetime);
        auto stream = std::make_shared<::MphRead::NativeRuntime::FileStream>(filePath,
            ::MphRead::NativeRuntime::FileMode::Open, ::MphRead::NativeRuntime::FileAccess::Read,
            ::MphRead::NativeRuntime::FileShare::Read);
        co_await Decode(std::static_pointer_cast<MovieNativeRuntime::Stream>(stream),
            FileName(PathFromUtf8(filePath)), writeFiles, token);
        co_return;
    }

    MovieNativeRuntime::MovieTask VxDecoder::Decode(
        std::shared_ptr<ClrArray<std::uint8_t>> data, const std::string& filename,
        bool writeFiles, std::stop_token token)
    {
        co_await MovieNativeRuntime::MovieTask::TrackLifetime(_lifetime);
        if (!data)
        {
            // MemoryStream(byte[]) faults the async method with ArgumentNullException.
            throw System::ArgumentNullException("buffer");
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(data->Length()));
        for (std::int32_t i = 0; i < data->Length(); ++i)
        {
            bytes[static_cast<std::size_t>(i)] = (*data)[i];
        }
        // new MemoryStream(byte[]): fixed in size and writable.
        auto stream = std::make_shared<::MphRead::NativeRuntime::MemoryStream>(std::move(bytes));
        co_await Decode(std::static_pointer_cast<MovieNativeRuntime::Stream>(stream),
            filename, writeFiles, token);
        co_return;
    }

    MovieNativeRuntime::MovieTask VxDecoder::Decode(
        std::shared_ptr<MovieNativeRuntime::Stream> stream, const std::string& filename,
        bool writeFiles, std::stop_token token)
    {
        co_await MovieNativeRuntime::MovieTask::TrackLifetime(_lifetime);
        co_await DecodeCore(std::move(stream), filename, writeFiles, token);
        co_return;
    }

    MovieNativeRuntime::MovieTask VxDecoder::DecodeCore(
        std::shared_ptr<MovieNativeRuntime::Stream> stream, const std::string& filename,
        bool writeFiles, std::stop_token token)
    {
        const std::string folder = Paths::Combine(Paths::Export(), Stem(PathFromUtf8(filename)));
        if (writeFiles)
        {
            std::filesystem::create_directories(PathFromUtf8(folder));
        }

        MovieNativeRuntime::BinaryReader reader(std::move(stream));
        std::exception_ptr bodyException;
        bool cancelled = false;
        try
        {
            _nextPlaneBufferIndex = 0;
        _nextSampleBufferIndex = 0;
        _framesQueued.store(0, std::memory_order_relaxed);

        (*Magic)[0] = reader.ReadChar();
        (*Magic)[1] = reader.ReadChar();
        (*Magic)[2] = reader.ReadChar();
        (*Magic)[3] = reader.ReadChar();
        FrameCount = reader.ReadInt32();
        FrameWidth = reader.ReadInt32();
        FrameHeight = reader.ReadInt32();
        FrameRate = System::Decimal::DivideInt32By65536(reader.ReadInt32());
        Quantizer = reader.ReadInt32();
        AudioSampleRate = reader.ReadInt32();
        AudioStreamCount = reader.ReadInt32();
        if (AudioStreamCount > 1)
        {
            throw ProgramException("VX decoding error 022: " + std::to_string(AudioStreamCount));
        }
        MaxDataSize = reader.ReadInt32();
        MaxDataSize = UncheckedAdd(MaxDataSize, -2);
        assert(MaxDataSize % 2 == 0);
        ExtradataOffset = reader.ReadInt32();
        SeekTableOffset = reader.ReadInt32();
        SeekTableCount = reader.ReadInt32();

        if (FrameWidth % 16 != 0 || FrameHeight % 16 != 0)
        {
            throw ProgramException("VX decoding error 001: "
                + std::to_string(FrameWidth) + " x " + std::to_string(FrameHeight));
        }

        const std::int64_t prevPosition = reader.Position();
        reader.Position(ExtradataOffset);

        for (std::int32_t i = 0; i < 3; ++i)
        {
            for (std::int32_t j = 0; j < 64; ++j)
            {
                for (std::int32_t k = 0; k < 8; ++k)
                {
                    (*Extradata->LpcCodebooks)(i, j, k) = reader.ReadInt16();
                }
            }
        }

        for (std::int32_t i = 0; i < 8; ++i)
        {
            (*Extradata->ScaleModifiers)[i] = reader.ReadUInt16();
        }
        for (std::int32_t i = 0; i < 8; ++i)
        {
            (*Extradata->LpcBase)[i] = reader.ReadInt32();
        }
        Extradata->ScaleInitial = reader.ReadInt32();

        reader.Position(SeekTableOffset);
        for (std::int32_t i = 0; i < SeekTableCount; ++i)
        {
            // Named so the two reads keep the order C# gives them.
            const std::int32_t frameId = reader.ReadInt32();
            const std::int32_t frameOffset = reader.ReadInt32();
            (*SeekTable)[0] = SeekTableEntry(frameId, frameOffset);
        }

        if (Quantizer < 12 || Quantizer > 161)
        {
            throw ProgramException("VX decoding error 002: " + std::to_string(Quantizer));
        }
        const std::int32_t qy = Quantizer / 6;
        const std::int32_t qx = Quantizer % 6;
        const std::array<std::int32_t, 3>& table = _quantizer4x4Table.at(static_cast<std::size_t>(qx));
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(table.size());
            i = UncheckedAdd(i, 1))
        {
            (*QuantizerTable)[i] = table[static_cast<std::size_t>(i)] << qy;
        }

        reader.Position(prevPosition);

        (void)VLC::Temp();

        std::shared_ptr<ClrArray<std::uint8_t>> buffer = GetDataBuffer();
        FillClrArray(*buffer, std::uint8_t{0});

        std::shared_ptr<ByteArray2D> coeffBufferY;
        std::shared_ptr<ByteArray2D> coeffBufferUV;
        std::shared_ptr<Vector2irArray2D> vectors;
        if (UseStaticBuffers)
        {
            coeffBufferY = _coeffBufferY;
            coeffBufferUV = _coeffBufferUV;
            vectors = _vectors;
        }
        else
        {
            coeffBufferY = std::make_shared<ByteArray2D>(FrameHeight / 4 + 1, FrameWidth / 4 + 1);
            coeffBufferUV = std::make_shared<ByteArray2D>(FrameHeight / 8 + 1, FrameWidth / 8 + 1);
            vectors = std::make_shared<Vector2irArray2D>(FrameHeight / 16 + 1, FrameWidth / 16 + 2);
        }

        if (FrameCount < 0)
        {
            throw std::out_of_range("Non-negative number required. (Parameter 'capacity')");
        }
        {
            std::lock_guard<std::mutex> lock(_vxFramesMutex);
            _vxFrames = std::make_shared<std::vector<std::shared_ptr<VxFrame>>>();
            _vxFrames->reserve(static_cast<std::size_t>(FrameCount));
        }
        RequireManagedReference(_prevVideoFrames)[0].reset();
        RequireManagedReference(_prevVideoFrames)[1].reset();
        RequireManagedReference(_prevVideoFrames)[2].reset();
        ClearClrArray(*_prevSampleBuffer);
        ClearClrArray(*_prevPulseBuffer);
        ClearClrArray(*_lpcFilterBuffer);
        ClearClrArray(*_influenceBuffer);
        std::shared_ptr<AudioFrame> prevAudioFrame{};
        _audioFrameTotal.store(0, std::memory_order_release);

        std::ofstream waveFile;
        std::ostringstream nullFile;
        std::ostream* output = &nullFile;
        if (writeFiles && AudioStreamCount >= 1)
        {
            waveFile.open(PathFromUtf8(Paths::Combine(folder, "audio.wav")),
                std::ios::binary | std::ios::trunc);
            if (!waveFile)
            {
                throw std::runtime_error("Could not create audio.wav.");
            }
            output = &waveFile;
        }
        Sound::BinaryWriter writer(*output);
        std::vector<std::uint8_t> fileOutputBuffer;
        if (writeFiles)
        {
            const std::int32_t size = UncheckedMultiply(
                UncheckedMultiply(FrameWidth, FrameHeight), 3);
            if (size < 0)
            {
                throw System::OverflowException();
            }
            fileOutputBuffer.resize(static_cast<std::size_t>(size));
        }

        for (std::int32_t i = 0; i < 11; ++i)
        {
            writer.Write(static_cast<std::int32_t>(0));
        }

        for (std::int32_t i = 0; i < FrameCount; ++i)
        {
            while (!writeFiles && _framesQueued.load(std::memory_order_relaxed) >= 4 && !token.stop_requested())
            {
                co_await MovieNativeRuntime::MovieTask::DelayOneMillisecond();
            }
            if (token.stop_requested())
            {
                cancelled = true;
                break;
            }

            std::int32_t dataSize = reader.ReadUInt16();
            dataSize = UncheckedAdd(dataSize, -2);
            assert(dataSize % 2 == 0);
            assert(dataSize <= MaxDataSize);
            const std::int32_t audioFrameCount = reader.ReadUInt16();

            std::shared_ptr<ByteArray2D> planeBufferY{};
            std::shared_ptr<ByteArray2D> planeBufferU{};
            std::shared_ptr<ByteArray2D> planeBufferV{};
            if (UseStaticBuffers)
            {
                auto planeBuffers = GetPlaneBuffers();
                planeBufferY = std::move(planeBuffers[0]);
                planeBufferU = std::move(planeBuffers[1]);
                planeBufferV = std::move(planeBuffers[2]);
            }

            ClearRectArray(*coeffBufferY);
            ClearRectArray(*coeffBufferUV);
            ClearRectArray(*vectors);

            VxBuffers buffers(
                _prevVideoFrames, QuantizerTable,
                planeBufferY, planeBufferU, planeBufferV,
                coeffBufferY, coeffBufferUV, vectors,
                _prevSampleBuffer, _prevPulseBuffer,
                _lpcFilterBuffer, _influenceBuffer, _sampleBuffer);

            auto vxFrame = std::make_shared<VxFrame>(
                FrameWidth, FrameHeight, audioFrameCount,
                Extradata, prevAudioFrame, buffers, _nextSampleBufferIndex);
            vxFrame->Decode(reader, buffer, dataSize);

            if (audioFrameCount > 0)
            {
                prevAudioFrame = (*vxFrame->AudioFrames)[UncheckedSubtract(audioFrameCount, 1)];
                _nextSampleBufferIndex = UncheckedAdd(_nextSampleBufferIndex, audioFrameCount);
                _nextSampleBufferIndex %= SampleBufferCount();
            }

            const std::int32_t audioFrameTotal = UncheckedAdd(
                _audioFrameTotal.load(std::memory_order_relaxed), audioFrameCount);
            _audioFrameTotal.store(audioFrameTotal, std::memory_order_release);
            {
                std::lock_guard<std::mutex> lock(_vxFramesMutex);
                _vxFrames->push_back(vxFrame);
            }
            const std::int32_t queued = _framesQueued.load(std::memory_order_relaxed);
            _framesQueued.store(UncheckedAdd(queued, 1), std::memory_order_relaxed);
            RequireManagedReference(_prevVideoFrames)[2] = RequireManagedReference(_prevVideoFrames)[1];
            RequireManagedReference(_prevVideoFrames)[1] = RequireManagedReference(_prevVideoFrames)[0];
            RequireManagedReference(_prevVideoFrames)[0] = vxFrame->VideoFrame;

            if (writeFiles && UseStaticBuffers)
            {
                WriteFile(fileOutputBuffer, *vxFrame, folder, i);
            }

            if (writeFiles && audioFrameCount > 0)
            {
                for (std::int32_t j = 0; j < audioFrameCount; ++j)
                {
                    const std::shared_ptr<AudioFrame>& audioFrame
                        = (*vxFrame->AudioFrames)[j];
                    const std::span<const std::int16_t> samples = audioFrame->SampleBuffer();
                    for (std::int32_t k = 0; k < 128; ++k)
                    {
                        const std::uint16_t sample
                            = std::bit_cast<std::uint16_t>(samples[static_cast<std::size_t>(k)]);
                        writer.Write(static_cast<std::uint8_t>(sample & 0xFFU));
                        writer.Write(static_cast<std::uint8_t>((sample >> 8) & 0xFFU));
                    }
                }
            }
        }

        if (!cancelled && writeFiles && _audioFrameTotal.load(std::memory_order_acquire) > 0)
        {
            output->clear();
            output->seekp(0, std::ios::beg);
            Sound::SoundRead::WriteWavHeader(
                writer,
                static_cast<std::uint32_t>(_audioFrameTotal.load(std::memory_order_relaxed)) * 128U,
                static_cast<std::uint16_t>(AudioSampleRate),
                WaveFormat::PCM16);
        }

        if (!cancelled && writeFiles && !UseStaticBuffers)
        {
            std::shared_ptr<std::vector<std::shared_ptr<VxFrame>>> frames;
            {
                std::lock_guard<std::mutex> lock(_vxFramesMutex);
                frames = _vxFrames;
            }
            std::int32_t frame = 0;
            for (const std::shared_ptr<VxFrame>& vxFrame : *frames)
            {
                WriteFile(fileOutputBuffer, *vxFrame, folder, frame);
                frame = UncheckedAdd(frame, 1);
            }
        }
        }
        catch (...)
        {
            bodyException = std::current_exception();
        }

        std::exception_ptr disposeException;
        try
        {
            reader.Dispose();
        }
        catch (...)
        {
            disposeException = std::current_exception();
        }

        if (disposeException)
        {
            std::rethrow_exception(disposeException);
        }
        if (bodyException)
        {
            std::rethrow_exception(bodyException);
        }
        co_return;
    }

    void VxDecoder::WriteFile(
        std::span<std::uint8_t> pixelBuffer, const VxFrame& vxFrame,
        const std::string& folder, std::int32_t frameIndex)
    {
        const VideoFrame& videoFrame = *vxFrame.VideoFrame;
        for (std::int32_t y = 0; y < FrameHeight; ++y)
        {
            for (std::int32_t x = 0; x < FrameWidth; ++x)
            {
                const std::int32_t cy = (*videoFrame.PlaneBufferY())(y, x);
                const std::int32_t cu = (*videoFrame.PlaneBufferU())(y / 2, x / 2);
                const std::int32_t cv = (*videoFrame.PlaneBufferV())(y / 2, x / 2);
                const ColorRgb rgb = YuvToRgb(cy, cu, cv);
                std::int32_t index = UncheckedMultiply(
                    UncheckedAdd(UncheckedMultiply(y, FrameWidth), x), 3);
                if (index < 0 || static_cast<std::size_t>(index) + 2U >= pixelBuffer.size())
                {
                    throw System::IndexOutOfRangeException();
                }
                pixelBuffer[static_cast<std::size_t>(index)] = rgb.Red;
                index = UncheckedAdd(index, 1);
                pixelBuffer[static_cast<std::size_t>(index)] = rgb.Green;
                index = UncheckedAdd(index, 1);
                pixelBuffer[static_cast<std::size_t>(index)] = rgb.Blue;
            }
        }

        std::ostringstream name;
        name << std::setw(4) << std::setfill('0') << frameIndex << ".png";
        WriteRgbPng(Paths::Combine(folder, name.str()), pixelBuffer, FrameWidth, FrameHeight);
    }

    bool VxDecoder::GetImage(std::int32_t frameIndex,
        const std::shared_ptr<ClrArray<std::uint8_t>>& texture)
    {
        std::shared_ptr<VxFrame> vxFrame;
        {
            std::lock_guard<std::mutex> lock(_vxFramesMutex);
            if (!_vxFrames || frameIndex >= static_cast<std::int32_t>(_vxFrames->size()))
            {
                return false;
            }
            if (frameIndex < 0)
            {
                throw System::ArgumentOutOfRangeException("index");
            }
            vxFrame = (*_vxFrames)[static_cast<std::size_t>(frameIndex)];
        }
        for (std::int32_t y = 0; y < FrameHeight; y = UncheckedAdd(y, 1))
        {
            for (std::int32_t x = 0; x < FrameWidth; x = UncheckedAdd(x, 1))
            {
                const std::int32_t cy = (*vxFrame->VideoFrame->PlaneBufferY())(y, x);
                const std::int32_t cu = (*vxFrame->VideoFrame->PlaneBufferU())(y / 2, x / 2);
                const std::int32_t cv = (*vxFrame->VideoFrame->PlaneBufferV())(y / 2, x / 2);
                const ColorRgb rgb = YuvToRgb(cy, cu, cv);
                const std::int32_t base = UncheckedAdd(
                    UncheckedMultiply(y, 256 * 3), UncheckedMultiply(x, 3));
                const std::array<std::uint8_t, 3> values{rgb.Red, rgb.Green, rgb.Blue};
                for (std::int32_t channel = 0; channel < 3; channel = UncheckedAdd(channel, 1))
                {
                    const std::int32_t index = UncheckedAdd(base, channel);
                    RequireManagedReference(texture)[index]
                        = values[static_cast<std::size_t>(channel)];
                }
            }
        }
        const std::int32_t queued = _framesQueued.load(std::memory_order_relaxed);
        _framesQueued.store(UncheckedAdd(queued, -1), std::memory_order_relaxed);
        return true;
    }

    std::span<const std::int16_t> VxDecoder::GetAudioBuffer(std::int32_t index) const
    {
        index %= SampleBufferCount();
        const std::int32_t start = UncheckedMultiply(128, index);
        if (start < 0 || start > RequireManagedReference(_sampleBuffer).Length()
            || RequireManagedReference(_sampleBuffer).Length() - start < 128)
        {
            throw System::ArgumentOutOfRangeException("start");
        }
        return std::span<const std::int16_t>(
            std::addressof(RequireManagedReference(_sampleBuffer)[start]), 128);
    }

    std::shared_ptr<ClrArray<std::uint8_t>> VxDecoder::GetDataBuffer()
    {
        if (UseStaticBuffers || MaxDataSize <= _mphMaxDataSize)
        {
            return _dataBuffer;
        }
        return std::make_shared<ClrArray<std::uint8_t>>(MaxDataSize);
    }

    std::array<std::shared_ptr<ByteArray2D>, 3> VxDecoder::GetPlaneBuffers()
    {
        auto takeBuffer = [this]() -> std::shared_ptr<ByteArray2D>
        {
            const std::int32_t index = _nextPlaneBufferIndex;
            _nextPlaneBufferIndex = UncheckedAdd(_nextPlaneBufferIndex, 1);
            if (index < 0 || static_cast<std::size_t>(index) >= _planeBuffers.size())
            {
                throw System::IndexOutOfRangeException();
            }
            return _planeBuffers[static_cast<std::size_t>(index)];
        };
        std::shared_ptr<ByteArray2D> bufferY = takeBuffer();
        std::shared_ptr<ByteArray2D> bufferU = takeBuffer();
        std::shared_ptr<ByteArray2D> bufferV = takeBuffer();
        _nextPlaneBufferIndex %= static_cast<std::int32_t>(_planeBuffers.size());

        // Preserve the source exactly: V is cleared twice; Y is not cleared here.
        ClearRectArray(*bufferV);
        ClearRectArray(*bufferU);
        ClearRectArray(*bufferV);
        return {std::move(bufferY), std::move(bufferU), std::move(bufferV)};
    }

    ColorRgb VxDecoder::YuvToRgb(std::int32_t y, std::int32_t u, std::int32_t v) noexcept
    {
        u -= 128;
        v -= 128;
        const std::int32_t r = y + 2 * v;
        const std::int32_t g = y - u / 2 - v;
        const std::int32_t b = y + 2 * u;
        return ColorRgb(
            static_cast<std::uint8_t>(MathClamp(r, 0, 255)),
            static_cast<std::uint8_t>(MathClamp(g, 0, 255)),
            static_cast<std::uint8_t>(MathClamp(b, 0, 255)));
    }

    VxFrame::VxFrame(
        std::int32_t frameWidth, std::int32_t frameHeight, std::int32_t audioFrameCount,
        std::shared_ptr<AudioExtradata> extradata, std::shared_ptr<AudioFrame> prevAudioFrame,
        const VxBuffers& buffers, std::int32_t sampleBufferIndex)
        : VideoFrame(std::make_shared<::MphRead::Formats::VideoFrame>(
            frameWidth, frameHeight, buffers)),
          AudioFrameCount(audioFrameCount),
          AudioFrames([extradata = std::move(extradata), prevAudioFrame = std::move(prevAudioFrame),
              &buffers, sampleBufferIndex, audioFrameCount]() mutable
          {
              auto frames = std::make_shared<
                  ClrArray<std::shared_ptr<::MphRead::Formats::AudioFrame>>>(audioFrameCount);
              for (std::int32_t i = 0; i < audioFrameCount; i = UncheckedAdd(i, 1))
              {
                  auto audioFrame = std::make_shared<::MphRead::Formats::AudioFrame>(
                      extradata, prevAudioFrame, buffers, sampleBufferIndex);
                  sampleBufferIndex = UncheckedAdd(sampleBufferIndex, 1);
                  (*frames)[i] = audioFrame;
                  prevAudioFrame = std::move(audioFrame);
                  sampleBufferIndex %= VxDecoder::SampleBufferCount();
              }
              return frames;
          }())
    {
    }

    void VxFrame::Decode(
        MovieNativeRuntime::BinaryReader& reader,
        const std::shared_ptr<ClrArray<std::uint8_t>>& buffer,
        std::int32_t length)
    {
        for (std::int32_t i = 0; i < length; i = UncheckedAdd(i, 2))
        {
            RequireManagedReference(buffer)[UncheckedAdd(i, 1)] = reader.ReadByte();
            RequireManagedReference(buffer)[i] = reader.ReadByte();
        }
        BitStreamReader bitReader(buffer, length);
        VideoFrame->Decode(bitReader);
        for (std::int32_t i = 0; i < AudioFrameCount; ++i)
        {
            (*AudioFrames)[i]->Decode(bitReader);
        }
    }

    BitStreamReader::BitStreamReader(
        std::shared_ptr<ClrArray<std::uint8_t>> buffer, std::int32_t length)
        : _buffer(std::move(buffer)), _length(length)
    {
    }

    std::int32_t BitStreamReader::ReadBit()
    {
        const std::int32_t bytePosition = _bitPosition / 8;
        const std::int32_t bitPosition = _bitPosition % 8;
        _bitPosition = UncheckedAdd(_bitPosition, 1);
        return (RequireManagedReference(_buffer)[bytePosition]
            >> UncheckedSubtract(7, bitPosition)) & 1;
    }

    std::int32_t BitStreamReader::ConsumeUntilNotZero()
    {
        std::int32_t count = 0;
        while (ReadBit() == 0)
        {
            count = UncheckedAdd(count, 1);
        }
        return count;
    }

    std::int32_t BitStreamReader::ReadUnsignedExpGolomb()
    {
        const std::int32_t zeroCount = ConsumeUntilNotZero();
        assert(zeroCount <= 30);
        std::int32_t value = ShiftLeft(1, zeroCount);
        for (std::int32_t i = 0; i < zeroCount; i = UncheckedAdd(i, 1))
        {
            const std::int32_t shift = UncheckedSubtract(
                UncheckedSubtract(zeroCount, i), 1);
            value |= ShiftLeft(ReadBit(), shift);
        }
        return UncheckedAdd(value, -1);
    }

    std::int32_t BitStreamReader::ReadSignedExpGolomb()
    {
        const std::int32_t value = UncheckedAdd(ReadUnsignedExpGolomb(), 1);
        const std::int32_t sign = UncheckedAdd(
            UncheckedMultiply(value & 1, -2), 1);
        return UncheckedMultiply(ArithmeticInt32ShiftRight(value, 1), sign);
    }

    std::int32_t BitStreamReader::ReadInt(std::int32_t bitCount)
    {
        assert(bitCount >= 0 && bitCount <= 32);
        std::int32_t value = 0;
        for (std::int32_t i = 0; i < bitCount; i = UncheckedAdd(i, 1))
        {
            const std::int32_t shift = UncheckedSubtract(
                UncheckedSubtract(bitCount, i), 1);
            value |= ShiftLeft(ReadBit(), shift);
        }
        return value;
    }

    std::int32_t BitStreamReader::ReadVLC2(const VLCData& vlc)
    {
        std::int32_t bitCount = 1;
        std::int32_t hashCode = ::MphRead::NativeRuntime::HashCodeCombine(0, ReadBit());
        std::int32_t index = vlc.FindBitPattern(hashCode);
        while (index == -1)
        {
            assert(bitCount < vlc.MaxBitCount());
            bitCount = UncheckedAdd(bitCount, 1);
            hashCode = ::MphRead::NativeRuntime::HashCodeCombine(hashCode, ReadBit());
            index = vlc.FindBitPattern(hashCode);
        }
        return index;
    }

    void BitStreamReader::EnsureWordAlignment()
    {
        while (_bitPosition % 16 != 0)
        {
            _bitPosition = UncheckedAdd(_bitPosition, 1);
        }
    }

    const std::array<std::int32_t, 32> VideoFrame::_residueMaskTable{
        0x00, 0x08, 0x04, 0x02, 0x01, 0x1F, 0x0F, 0x0A,
        0x05, 0x0C, 0x03, 0x10, 0x0E, 0x0D, 0x0B, 0x07,
        0x09, 0x06, 0x1E, 0x1B, 0x1A, 0x1D, 0x17, 0x15,
        0x18, 0x12, 0x11, 0x1C, 0x14, 0x13, 0x16, 0x19
    };

    const std::array<std::int32_t, 17> VideoFrame::_tokenIndexTable{
        0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3
    };

    const std::array<std::int32_t, 7> VideoFrame::_suffixLimits{
        0, 3, 6, 12, 24, 48, 0x8000
    };

    const std::array<std::int32_t, 16> VideoFrame::_zigzagScanTable{
        0 * 4 + 0, 1 * 4 + 0, 0 * 4 + 1, 0 * 4 + 2,
        1 * 4 + 1, 2 * 4 + 0, 3 * 4 + 0, 2 * 4 + 1,
        1 * 4 + 2, 0 * 4 + 3, 1 * 4 + 3, 2 * 4 + 2,
        3 * 4 + 1, 3 * 4 + 2, 2 * 4 + 3, 3 * 4 + 3
    };

    VideoFrame::VideoFrame(
        std::int32_t frameWidth, std::int32_t frameHeight, const VxBuffers& buffers)
        : FrameWidth(frameWidth),
          FrameHeight(frameHeight),
          _planeBufferY(buffers.PlaneBufferY.Shared()),
          _planeBufferU(buffers.PlaneBufferU.Shared()),
          _planeBufferV(buffers.PlaneBufferV.Shared()),
          _coeffBufferY(buffers.CoeffBufferY),
          _coeffBufferUV(buffers.CoeffBufferUV),
          _vectors(buffers.Vectors),
          _prevVideoFrames(buffers.PrevVideoFrames),
          _quantizerTable(buffers.QuantizerTable)
    {
    }

    void VideoFrame::Decode(BitStreamReader& reader)
    {
        _reader = &reader;

        if (!_planeBufferY)
        {
            _planeBufferY = std::make_shared<ByteArray2D>(FrameHeight, FrameWidth);
            _planeBufferU = std::make_shared<ByteArray2D>(FrameHeight / 2, FrameWidth / 2);
            _planeBufferV = std::make_shared<ByteArray2D>(FrameHeight / 2, FrameWidth / 2);
        }

        for (std::int32_t y = 0; y < FrameHeight; y = UncheckedAdd(y, 16))
        {
            for (std::int32_t x = 0; x < FrameWidth; x = UncheckedAdd(x, 16))
            {
                const Vector2ir predictionVector(
                    GetMiddleValue(
                        RequireManagedReference(_vectors)((y / 16) + 1, (x / 16) + 0).X,
                        RequireManagedReference(_vectors)((y / 16) + 0, (x / 16) + 1).X,
                        RequireManagedReference(_vectors)((y / 16) + 0, (x / 16) + 2).X),
                    GetMiddleValue(
                        RequireManagedReference(_vectors)((y / 16) + 1, (x / 16) + 0).Y,
                        RequireManagedReference(_vectors)((y / 16) + 0, (x / 16) + 1).Y,
                        RequireManagedReference(_vectors)((y / 16) + 0, (x / 16) + 2).Y));
                DecodeBlock(Block(x, y, 16, 16), predictionVector);
            }
        }

        _reader->EnsureWordAlignment();
        _reader = nullptr;
    }

    std::int32_t VideoFrame::GetMiddleValue(
        std::int32_t a, std::int32_t b, std::int32_t c) noexcept
    {
        std::array<std::int32_t, 3> array{a, b, c};
        std::sort(array.begin(), array.end());
        return array[1];
    }

    std::uint8_t VideoFrame::PlaneBufferGetter(
        const ByteArray2D& planeBuffer, std::int32_t step, std::int32_t x, std::int32_t y)
    {
        return planeBuffer(y / step, x / step);
    }

    void VideoFrame::DecodeBlock(Block block, Vector2ir predictionVector)
    {
        const std::int32_t mode = _reader->ReadUnsignedExpGolomb();

        if (mode == 0)
        {
            if (block.W == 2)
            {
                throw ProgramException("VX decoding error 003");
            }
            DecodeBlock(block.HalfLeft(), predictionVector);
            DecodeBlock(block.HalfRight(), predictionVector);
            if (block.W == 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 1)
        {
            PredictInter(block, predictionVector, false, RequireManagedReference(_prevVideoFrames)[0]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 2)
        {
            if (block.H == 2)
            {
                throw ProgramException("VX decoding error 004");
            }
            DecodeBlock(block.HalfUp(), predictionVector);
            DecodeBlock(block.HalfDown(), predictionVector);
            if (block.W >= 8 && block.H == 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 3)
        {
            PredictInterDC(block);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 4)
        {
            PredictInter(block, predictionVector, true, RequireManagedReference(_prevVideoFrames)[0]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 5)
        {
            PredictInter(block, predictionVector, true, RequireManagedReference(_prevVideoFrames)[1]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 6)
        {
            PredictInter(block, predictionVector, true, RequireManagedReference(_prevVideoFrames)[2]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 7)
        {
            PredictMBPlane(block);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 8)
        {
            if (block.W == 2)
            {
                throw ProgramException("VX decoding error 005");
            }
            DecodeBlock(block.HalfLeft(), predictionVector);
            DecodeBlock(block.HalfRight(), predictionVector);
            DecodeResidueBlocks(block);
        }
        else if (mode == 9)
        {
            PredictInter(block, predictionVector, false, RequireManagedReference(_prevVideoFrames)[1]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 10)
        {
            PredictInterDC(block);
            DecodeResidueBlocks(block);
        }
        else if (mode == 11)
        {
            PredictNoTile(block);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 12)
        {
            PredictInter(block, predictionVector, false, RequireManagedReference(_prevVideoFrames)[0]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 13)
        {
            if (block.H == 2)
            {
                throw ProgramException("VX decoding error 006");
            }
            DecodeBlock(block.HalfUp(), predictionVector);
            DecodeBlock(block.HalfDown(), predictionVector);
            DecodeResidueBlocks(block);
        }
        else if (mode == 14)
        {
            PredictInter(block, predictionVector, false, RequireManagedReference(_prevVideoFrames)[2]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 15)
        {
            Predict4(block);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 16)
        {
            PredictInter(block, predictionVector, true, RequireManagedReference(_prevVideoFrames)[0]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 17)
        {
            PredictInter(block, predictionVector, true, RequireManagedReference(_prevVideoFrames)[1]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 18)
        {
            PredictInter(block, predictionVector, true, RequireManagedReference(_prevVideoFrames)[2]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 19)
        {
            Predict4(block);
            DecodeResidueBlocks(block);
        }
        else if (mode == 20)
        {
            PredictInter(block, predictionVector, false, RequireManagedReference(_prevVideoFrames)[1]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 21)
        {
            PredictInter(block, predictionVector, false, RequireManagedReference(_prevVideoFrames)[2]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 22)
        {
            PredictNoTile(block);
            DecodeResidueBlocks(block);
        }
        else if (mode == 23)
        {
            PredictMBPlane(block);
            DecodeResidueBlocks(block);
        }
        else
        {
            throw ProgramException("VX decoding error 007: " + std::to_string(mode));
        }
    }

    void VideoFrame::PredictInter(
        Block block, Vector2ir predictionVector, bool hasDelta,
        const std::shared_ptr<VideoFrame>& prevVideoFrame)
    {
        assert(prevVideoFrame);
        if (!prevVideoFrame)
        {
            throw System::NullReferenceException();
        }

        if (hasDelta)
        {
            // The two reads are named so they happen in the order C# gives
            // them: argument evaluation order is unspecified in C++, and
            // reading a bit stream out of order swaps X with Y.
            const std::int32_t deltaX = _reader->ReadSignedExpGolomb();
            const std::int32_t deltaY = _reader->ReadSignedExpGolomb();
            predictionVector = Vector2ir(
                UncheckedAdd(predictionVector.X, deltaX),
                UncheckedAdd(predictionVector.Y, deltaY));
        }

        RequireManagedReference(_vectors)((block.Y / 16) + 1, (block.X / 16) + 1) = predictionVector;

        for (std::int32_t y = block.Y; y < UncheckedAdd(block.Y, block.H); y = UncheckedAdd(y, 1))
        {
            for (std::int32_t x = block.X; x < UncheckedAdd(block.X, block.W); x = UncheckedAdd(x, 1))
            {
                RequireManagedReference(_planeBufferY)(y, x) = PlaneBufferGetter(
                    RequireManagedReference(prevVideoFrame->_planeBufferY), 1,
                    UncheckedAdd(x, predictionVector.X),
                    UncheckedAdd(y, predictionVector.Y));
            }
        }
        for (std::int32_t y = block.Y; y < UncheckedAdd(block.Y, block.H); y = UncheckedAdd(y, 2))
        {
            for (std::int32_t x = block.X; x < UncheckedAdd(block.X, block.W); x = UncheckedAdd(x, 2))
            {
                RequireManagedReference(_planeBufferU)(y / 2, x / 2) = PlaneBufferGetter(
                    RequireManagedReference(prevVideoFrame->_planeBufferU), 2,
                    UncheckedAdd(x, predictionVector.X),
                    UncheckedAdd(y, predictionVector.Y));
            }
        }
        for (std::int32_t y = block.Y; y < UncheckedAdd(block.Y, block.H); y = UncheckedAdd(y, 2))
        {
            for (std::int32_t x = block.X; x < UncheckedAdd(block.X, block.W); x = UncheckedAdd(x, 2))
            {
                RequireManagedReference(_planeBufferV)(y / 2, x / 2) = PlaneBufferGetter(
                    RequireManagedReference(prevVideoFrame->_planeBufferV), 2,
                    UncheckedAdd(x, predictionVector.X),
                    UncheckedAdd(y, predictionVector.Y));
            }
        }
    }

    void VideoFrame::PredictInterDC(Block block)
    {
        const std::int32_t vecX = _reader->ReadSignedExpGolomb();
        const std::int32_t vecY = _reader->ReadSignedExpGolomb();
        const Vector2ir vec(vecX, vecY);

        const std::int32_t sourceX = UncheckedAdd(block.X, vec.X);
        const std::int32_t sourceY = UncheckedAdd(block.Y, vec.Y);
        const std::int32_t sourceRight = UncheckedAdd(sourceX, block.W);
        const std::int32_t sourceBottom = UncheckedAdd(sourceY, block.H);
        if (sourceX < 0 || sourceRight > FrameWidth
            || sourceY < 0 || sourceBottom > FrameHeight)
        {
            throw ProgramException("VX decoding error 008");
        }

        std::int32_t dcY = _reader->ReadSignedExpGolomb();
        if (dcY < -(1 << 16) || dcY >= (1 << 16))
        {
            throw ProgramException("VX decoding error 009");
        }
        dcY *= 2;

        std::int32_t dcU = _reader->ReadSignedExpGolomb();
        if (dcU < -(1 << 16) || dcU >= (1 << 16))
        {
            throw ProgramException("VX decoding error 010");
        }
        dcU *= 2;

        std::int32_t dcV = _reader->ReadSignedExpGolomb();
        if (dcV < -(1 << 16) || dcV >= (1 << 16))
        {
            throw ProgramException("VX decoding error 011");
        }
        dcV *= 2;

        const std::shared_ptr<VideoFrame>& prevVideoFrame = RequireManagedReference(_prevVideoFrames)[0];
        assert(prevVideoFrame);

        for (std::int32_t y = block.Y; y < UncheckedAdd(block.Y, block.H); y = UncheckedAdd(y, 1))
        {
            for (std::int32_t x = block.X; x < UncheckedAdd(block.X, block.W); x = UncheckedAdd(x, 1))
            {
                const std::int32_t predicted = PlaneBufferGetter(
                    RequireManagedReference(prevVideoFrame->_planeBufferY), 1,
                    UncheckedAdd(x, vec.X), UncheckedAdd(y, vec.Y));
                const std::int32_t pixel = UncheckedAdd(predicted, dcY);
                RequireManagedReference(_planeBufferY)(y, x) = static_cast<std::uint8_t>(MathClamp(pixel, 0, 255));
            }
        }
        for (std::int32_t y = block.Y; y < UncheckedAdd(block.Y, block.H); y = UncheckedAdd(y, 2))
        {
            for (std::int32_t x = block.X; x < UncheckedAdd(block.X, block.W); x = UncheckedAdd(x, 2))
            {
                const std::int32_t predicted = PlaneBufferGetter(
                    RequireManagedReference(prevVideoFrame->_planeBufferU), 2,
                    UncheckedAdd(x, vec.X), UncheckedAdd(y, vec.Y));
                const std::int32_t pixel = UncheckedAdd(predicted, dcU);
                RequireManagedReference(_planeBufferU)(y / 2, x / 2) = static_cast<std::uint8_t>(MathClamp(pixel, 0, 255));
            }
        }
        for (std::int32_t y = block.Y; y < UncheckedAdd(block.Y, block.H); y = UncheckedAdd(y, 2))
        {
            for (std::int32_t x = block.X; x < UncheckedAdd(block.X, block.W); x = UncheckedAdd(x, 2))
            {
                const std::int32_t predicted = PlaneBufferGetter(
                    RequireManagedReference(prevVideoFrame->_planeBufferV), 2,
                    UncheckedAdd(x, vec.X), UncheckedAdd(y, vec.Y));
                const std::int32_t pixel = UncheckedAdd(predicted, dcV);
                RequireManagedReference(_planeBufferV)(y / 2, x / 2) = static_cast<std::uint8_t>(MathClamp(pixel, 0, 255));
            }
        }
    }

    void VideoFrame::PredictMBPlane(Block block)
    {
        std::int32_t value = _reader->ReadSignedExpGolomb();
        if (value < -(1 << 16) || value >= (1 << 16))
        {
            throw ProgramException("VX decoding error 012: " + std::to_string(value));
        }
        PredictPlane(block, RequireManagedReference(_planeBufferY), 1, UncheckedMultiply(value, 2));

        value = _reader->ReadSignedExpGolomb();
        if (value < -(1 << 16) || value >= (1 << 16))
        {
            throw ProgramException("VX decoding error 013: " + std::to_string(value));
        }
        PredictPlane(block, RequireManagedReference(_planeBufferU), 2, UncheckedMultiply(value, 2));

        value = _reader->ReadSignedExpGolomb();
        if (value < -(1 << 16) || value >= (1 << 16))
        {
            throw ProgramException("VX decoding error 014: " + std::to_string(value));
        }
        PredictPlane(block, RequireManagedReference(_planeBufferV), 2, UncheckedMultiply(value, 2));
    }

    void VideoFrame::DecodeResidueBlocks(Block block)
    {
        for (std::int32_t y = 0; y < block.H; y = UncheckedAdd(y, 8))
        {
            for (std::int32_t x = 0; x < block.W; x = UncheckedAdd(x, 8))
            {
                const std::int32_t bx = UncheckedAdd(block.X, x);
                const std::int32_t by = UncheckedAdd(block.Y, y);
                const std::int32_t bx4 = UncheckedAdd(bx, 4);
                const std::int32_t by4 = UncheckedAdd(by, 4);
                const std::int32_t index = _reader->ReadUnsignedExpGolomb();
                if (index > 31)
                {
                    throw ProgramException("VX decoding error 015: " + std::to_string(index));
                }
                const std::int32_t residueMask = _residueMaskTable.at(static_cast<std::size_t>(index));

                if ((residueMask & 1) != 0)
                {
                    const std::int32_t coeffLeft = GetCoeffBuffer(
                        RequireManagedReference(_coeffBufferY), 1, UncheckedSubtract(bx, 1), by);
                    const std::int32_t coeffTop = GetCoeffBuffer(
                        RequireManagedReference(_coeffBufferY), 1, bx, UncheckedSubtract(by, 1));
                    const std::int32_t nc = (coeffLeft + coeffTop + 1) / 2;
                    const std::int32_t outTotalCoeff = DecodeResidueCAVLC(
                        bx, by, nc, RequireManagedReference(_planeBufferY), 1);
                    SetCoeffBuffer(RequireManagedReference(_coeffBufferY), 1, bx, by,
                        static_cast<std::uint8_t>(outTotalCoeff));
                }
                else
                {
                    SetCoeffBuffer(RequireManagedReference(_coeffBufferY), 1, bx, by, 0);
                }

                if ((residueMask & 2) != 0)
                {
                    const std::int32_t coeffLeft = GetCoeffBuffer(
                        RequireManagedReference(_coeffBufferY), 1, UncheckedSubtract(bx4, 1), by);
                    const std::int32_t coeffTop = GetCoeffBuffer(
                        RequireManagedReference(_coeffBufferY), 1, bx4, UncheckedSubtract(by, 1));
                    const std::int32_t nc = (coeffLeft + coeffTop + 1) / 2;
                    const std::int32_t outTotalCoeff = DecodeResidueCAVLC(
                        bx4, by, nc, RequireManagedReference(_planeBufferY), 1);
                    SetCoeffBuffer(RequireManagedReference(_coeffBufferY), 1, bx4, by,
                        static_cast<std::uint8_t>(outTotalCoeff));
                }
                else
                {
                    SetCoeffBuffer(RequireManagedReference(_coeffBufferY), 1, bx4, by, 0);
                }

                if ((residueMask & 4) != 0)
                {
                    const std::int32_t coeffLeft = GetCoeffBuffer(
                        RequireManagedReference(_coeffBufferY), 1, UncheckedSubtract(bx, 1), by4);
                    const std::int32_t coeffTop = GetCoeffBuffer(
                        RequireManagedReference(_coeffBufferY), 1, bx, UncheckedSubtract(by4, 1));
                    const std::int32_t nc = (coeffLeft + coeffTop + 1) / 2;
                    const std::int32_t outTotalCoeff = DecodeResidueCAVLC(
                        bx, by4, nc, RequireManagedReference(_planeBufferY), 1);
                    SetCoeffBuffer(RequireManagedReference(_coeffBufferY), 1, bx, by4,
                        static_cast<std::uint8_t>(outTotalCoeff));
                }
                else
                {
                    SetCoeffBuffer(RequireManagedReference(_coeffBufferY), 1, bx, by4, 0);
                }

                if ((residueMask & 8) != 0)
                {
                    const std::int32_t coeffLeft = GetCoeffBuffer(
                        RequireManagedReference(_coeffBufferY), 1, UncheckedSubtract(bx4, 1), by4);
                    const std::int32_t coeffTop = GetCoeffBuffer(
                        RequireManagedReference(_coeffBufferY), 1, bx4, UncheckedSubtract(by4, 1));
                    const std::int32_t nc = (coeffLeft + coeffTop + 1) / 2;
                    const std::int32_t outTotalCoeff = DecodeResidueCAVLC(
                        bx4, by4, nc, RequireManagedReference(_planeBufferY), 1);
                    SetCoeffBuffer(RequireManagedReference(_coeffBufferY), 1, bx4, by4,
                        static_cast<std::uint8_t>(outTotalCoeff));
                }
                else
                {
                    SetCoeffBuffer(RequireManagedReference(_coeffBufferY), 1, bx4, by4, 0);
                }

                if ((residueMask & 16) != 0)
                {
                    const std::int32_t coeffLeft = GetCoeffBuffer(
                        RequireManagedReference(_coeffBufferUV), 2, UncheckedSubtract(bx, 1), by);
                    const std::int32_t coeffTop = GetCoeffBuffer(
                        RequireManagedReference(_coeffBufferUV), 2, bx, UncheckedSubtract(by, 1));
                    const std::int32_t nc = (coeffLeft + coeffTop + 1) / 2;
                    const std::int32_t totalCoeffU = DecodeResidueCAVLC(
                        bx, by, nc, RequireManagedReference(_planeBufferU), 2);
                    const std::int32_t totalCoeffV = DecodeResidueCAVLC(
                        bx, by, nc, RequireManagedReference(_planeBufferV), 2);
                    const std::int32_t outTotalCoeff = (totalCoeffU + totalCoeffV + 1) / 2;
                    SetCoeffBuffer(RequireManagedReference(_coeffBufferUV), 2, bx, by,
                        static_cast<std::uint8_t>(outTotalCoeff));
                }
                else
                {
                    SetCoeffBuffer(RequireManagedReference(_coeffBufferUV), 2, bx, by, 0);
                }
            }
        }
    }

    std::int32_t VideoFrame::DecodeResidueCAVLC(
        std::int32_t x, std::int32_t y, std::int32_t nc,
        ByteArray2D& planeBuffer, std::int32_t step)
    {
        const std::int32_t coeffToken = _reader->ReadVLC2(
            VLC::CoeffTokenVlc().at(
                static_cast<std::size_t>(_tokenIndexTable.at(static_cast<std::size_t>(nc)))));
        if (coeffToken == -1)
        {
            throw ProgramException("VX decoding error 016");
        }

        std::int32_t trailingOnes = coeffToken & 3;
        std::int32_t totalCoeff = coeffToken >> 2;
        const std::int32_t outTotalCoeff = totalCoeff;
        if (totalCoeff == 0)
        {
            return outTotalCoeff;
        }

        std::array<std::int32_t, 16> level{};
        std::int32_t levelPos = 0;
        std::int32_t zeroesRemaining;
        if (totalCoeff == 16)
        {
            zeroesRemaining = 0;
        }
        else
        {
            zeroesRemaining = _reader->ReadVLC2(
                VLC::TotalZeroesVlc().at(static_cast<std::size_t>(totalCoeff)));
            for (std::int32_t i = 0; i < UncheckedSubtract(16, UncheckedAdd(totalCoeff, zeroesRemaining)); i = UncheckedAdd(i, 1))
            {
                level.at(static_cast<std::size_t>(levelPos)) = 0;
                levelPos = UncheckedAdd(levelPos, 1);
            }
        }

        std::int32_t suffixLength = 0;
        while (true)
        {
            if (trailingOnes > 0)
            {
                trailingOnes = UncheckedAdd(trailingOnes, -1);
                level.at(static_cast<std::size_t>(levelPos))
                    = _reader->ReadBit() == 0 ? 1 : -1;
                levelPos = UncheckedAdd(levelPos, 1);
            }
            else
            {
                std::int32_t levelPrefix = 0;
                while (_reader->ReadBit() == 0)
                {
                    levelPrefix = UncheckedAdd(levelPrefix, 1);
                }

                std::int32_t levelSuffix;
                if (levelPrefix == 15)
                {
                    levelSuffix = _reader->ReadInt(11);
                }
                else
                {
                    levelSuffix = _reader->ReadInt(suffixLength);
                }

                std::int32_t levelCode = WrapInt32Add3(
                    ShiftLeft(levelPrefix, suffixLength), levelSuffix, 1);
                const std::int32_t suffixIndex = UncheckedAdd(suffixLength, 1);
                if (suffixIndex < 0
                    || static_cast<std::size_t>(suffixIndex) >= _suffixLimits.size())
                {
                    throw System::IndexOutOfRangeException();
                }
                if (levelCode > _suffixLimits[static_cast<std::size_t>(suffixIndex)])
                {
                    suffixLength = UncheckedAdd(suffixLength, 1);
                }
                if (_reader->ReadBit() == 1)
                {
                    levelCode = UncheckedNegate(levelCode);
                }
                level.at(static_cast<std::size_t>(levelPos)) = levelCode;
                levelPos = UncheckedAdd(levelPos, 1);
            }

            totalCoeff = UncheckedAdd(totalCoeff, -1);
            if (totalCoeff == 0)
            {
                break;
            }
            if (zeroesRemaining == 0)
            {
                continue;
            }

            std::int32_t runBefore;
            if (zeroesRemaining < 7)
            {
                runBefore = _reader->ReadVLC2(
                    VLC::RunVlc().at(static_cast<std::size_t>(zeroesRemaining)));
            }
            else
            {
                runBefore = _reader->ReadVLC2(VLC::Run7Vlc());
            }

            zeroesRemaining = UncheckedSubtract(zeroesRemaining, runBefore);
            for (std::int32_t i = 0; i < runBefore; i = UncheckedAdd(i, 1))
            {
                level.at(static_cast<std::size_t>(levelPos)) = 0;
                levelPos = UncheckedAdd(levelPos, 1);
            }
        }

        for (std::int32_t i = 0; i < zeroesRemaining; i = UncheckedAdd(i, 1))
        {
            level.at(static_cast<std::size_t>(levelPos)) = 0;
                levelPos = UncheckedAdd(levelPos, 1);
        }

        assert(levelPos == 16);
        DecodeDct(x, y, planeBuffer, step, level);
        return outTotalCoeff;
    }

    void VideoFrame::DecodeDct(
        std::int32_t x, std::int32_t y, ByteArray2D& planeBuffer,
        std::int32_t step, std::span<const std::int32_t> level)
    {
        std::array<std::int32_t, 16> dct{};

        for (std::size_t i = 0; i < _zigzagScanTable.size(); ++i)
        {
            const std::int32_t z = _zigzagScanTable[i];
            dct.at(static_cast<std::size_t>(z)) = UncheckedMultiply(
                level[15U - i],
                (RequireManagedReference(_quantizerTable))[(z & 1) + ((z >> 2) & 1)]);
        }

        dct[0] = UncheckedAdd(dct[0], ShiftLeft(1, 5));

        for (std::int32_t i = 0; i < 4; i = UncheckedAdd(i, 1))
        {
            const std::size_t si = static_cast<std::size_t>(i);
            const std::int32_t z0 = UncheckedAdd(
                dct[si + 4U * 0U], dct[si + 4U * 2U]);
            const std::int32_t z1 = UncheckedSubtract(
                dct[si + 4U * 0U], dct[si + 4U * 2U]);
            const std::int32_t z2 = UncheckedSubtract(
                dct[si + 4U * 1U] / 2, dct[si + 4U * 3U]);
            const std::int32_t z3 = UncheckedAdd(
                dct[si + 4U * 1U], dct[si + 4U * 3U] / 2);

            dct[si + 4U * 0U] = UncheckedAdd(z0, z3);
            dct[si + 4U * 1U] = UncheckedAdd(z1, z2);
            dct[si + 4U * 2U] = UncheckedSubtract(z1, z2);
            dct[si + 4U * 3U] = UncheckedSubtract(z0, z3);
        }

        for (std::int32_t i = 0; i < 4; i = UncheckedAdd(i, 1))
        {
            const std::size_t base = 4U * static_cast<std::size_t>(i);
            const std::int32_t z0 = UncheckedAdd(dct[0U + base], dct[2U + base]);
            const std::int32_t z1 = UncheckedSubtract(dct[0U + base], dct[2U + base]);
            const std::int32_t z2 = UncheckedSubtract(dct[1U + base] / 2, dct[3U + base]);
            const std::int32_t z3 = UncheckedAdd(dct[1U + base], dct[3U + base] / 2);

            const std::int32_t bx = UncheckedAdd(x, UncheckedMultiply(step, i));
            std::int32_t by = y;
            std::int32_t pixel = UncheckedAdd(
                PlaneBufferGetter(planeBuffer, step, bx, by),
                ArithmeticInt32ShiftRight(UncheckedAdd(z0, z3), 6));
            planeBuffer(by / step, bx / step)
                = static_cast<std::uint8_t>(MathClamp(pixel, 0, 255));

            by = UncheckedAdd(y, step);
            pixel = UncheckedAdd(
                PlaneBufferGetter(planeBuffer, step, bx, by),
                ArithmeticInt32ShiftRight(UncheckedAdd(z1, z2), 6));
            planeBuffer(by / step, bx / step)
                = static_cast<std::uint8_t>(MathClamp(pixel, 0, 255));

            by = UncheckedAdd(y, UncheckedMultiply(step, 2));
            pixel = UncheckedAdd(
                PlaneBufferGetter(planeBuffer, step, bx, by),
                ArithmeticInt32ShiftRight(UncheckedSubtract(z1, z2), 6));
            planeBuffer(by / step, bx / step)
                = static_cast<std::uint8_t>(MathClamp(pixel, 0, 255));

            by = UncheckedAdd(y, UncheckedMultiply(step, 3));
            pixel = UncheckedAdd(
                PlaneBufferGetter(planeBuffer, step, bx, by),
                ArithmeticInt32ShiftRight(UncheckedSubtract(z0, z3), 6));
            planeBuffer(by / step, bx / step)
                = static_cast<std::uint8_t>(MathClamp(pixel, 0, 255));
        }
    }

    void VideoFrame::PredictNoTile(Block block)
    {
        const std::int32_t mode = _reader->ReadUnsignedExpGolomb();
        if (mode == 0)
        {
            PredictVertical(block, RequireManagedReference(_planeBufferY), 1);
        }
        else if (mode == 1)
        {
            PredictHorizontal(block, RequireManagedReference(_planeBufferY), 1);
        }
        else if (mode == 2)
        {
            PredictDC(block, RequireManagedReference(_planeBufferY), 1);
        }
        else if (mode == 3)
        {
            PredictPlane(block, RequireManagedReference(_planeBufferY), 1, 0);
        }
        else
        {
            throw ProgramException("VX decoding error 017: " + std::to_string(mode));
        }
        PredictNoTileUV(block);
    }

    void VideoFrame::PredictVertical(Block block, ByteArray2D& planeBuffer, std::int32_t step)
    {
        for (std::int32_t y = block.Y; y < UncheckedAdd(block.Y, block.H); y = UncheckedAdd(y, step))
        {
            for (std::int32_t x = block.X; x < UncheckedAdd(block.X, block.W); x = UncheckedAdd(x, step))
            {
                planeBuffer(y / step, x / step)
                    = PlaneBufferGetter(planeBuffer, step, x, block.Y - 1);
            }
        }
    }

    void VideoFrame::PredictHorizontal(Block block, ByteArray2D& planeBuffer, std::int32_t step)
    {
        for (std::int32_t y = block.Y; y < UncheckedAdd(block.Y, block.H); y = UncheckedAdd(y, step))
        {
            for (std::int32_t x = block.X; x < UncheckedAdd(block.X, block.W); x = UncheckedAdd(x, step))
            {
                planeBuffer(y / step, x / step)
                    = PlaneBufferGetter(planeBuffer, step, block.X - 1, y);
            }
        }
    }

    void VideoFrame::PredictDC(Block block, ByteArray2D& planeBuffer, std::int32_t step)
    {
        std::uint8_t dc = 128;
        if (block.X != 0 && block.Y != 0)
        {
            std::int32_t sumX = block.W / 2;
            for (std::int32_t x = 0; x < block.W; x = UncheckedAdd(x, 1))
            {
                sumX = UncheckedAdd(sumX, PlaneBufferGetter(planeBuffer, step,
                    UncheckedAdd(block.X, x), UncheckedSubtract(block.Y, 1)));
            }
            std::int32_t sumY = block.H / 2;
            for (std::int32_t y = 0; y < block.H; y = UncheckedAdd(y, 1))
            {
                sumY = UncheckedAdd(sumY, PlaneBufferGetter(planeBuffer, step,
                    UncheckedSubtract(block.X, 1), UncheckedAdd(block.Y, y)));
            }
            dc = static_cast<std::uint8_t>(((sumX / block.W) + (sumY / block.H) + 1) / 2);
        }
        else if (block.X == 0 && block.Y != 0)
        {
            std::int32_t sumX = block.W / 2;
            for (std::int32_t x = 0; x < block.W; x = UncheckedAdd(x, 1))
            {
                sumX = UncheckedAdd(sumX, PlaneBufferGetter(planeBuffer, step,
                    UncheckedAdd(block.X, x), UncheckedSubtract(block.Y, 1)));
            }
            dc = static_cast<std::uint8_t>(sumX / block.W);
        }
        else if (block.X != 0 && block.Y == 0)
        {
            std::int32_t sumY = block.H / 2;
            for (std::int32_t y = 0; y < block.H; y = UncheckedAdd(y, 1))
            {
                sumY = UncheckedAdd(sumY, PlaneBufferGetter(planeBuffer, step,
                    UncheckedSubtract(block.X, 1), UncheckedAdd(block.Y, y)));
            }
            dc = static_cast<std::uint8_t>(sumY / block.H);
        }

        for (std::int32_t y = block.Y; y < UncheckedAdd(block.Y, block.H); y = UncheckedAdd(y, step))
        {
            for (std::int32_t x = block.X; x < UncheckedAdd(block.X, block.W); x = UncheckedAdd(x, step))
            {
                planeBuffer(y / step, x / step) = dc;
            }
        }
    }

    void VideoFrame::PredictPlane(
        Block block, ByteArray2D& planeBuffer, std::int32_t step, std::int32_t value)
    {
        const std::int32_t bottomLeft
            = PlaneBufferGetter(planeBuffer, step, UncheckedSubtract(block.X, 1), UncheckedSubtract(UncheckedAdd(block.Y, block.H), 1));
        const std::int32_t topRight
            = PlaneBufferGetter(planeBuffer, step, UncheckedSubtract(UncheckedAdd(block.X, block.W), 1), UncheckedSubtract(block.Y, 1));
        const std::int32_t pixel = (bottomLeft + topRight + 1) / 2 + value;
        const std::int32_t x = UncheckedSubtract(UncheckedAdd(block.X, block.W), 1);
        const std::int32_t y = UncheckedSubtract(UncheckedAdd(block.Y, block.H), 1);
        planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(pixel);
        PredictPlaneRecursive(block, planeBuffer, step);
    }

    void VideoFrame::PredictPlaneRecursive(
        Block block, ByteArray2D& planeBuffer, std::int32_t step)
    {
        if (block.W == step && block.H == step)
        {
            return;
        }
        if (block.W == step && block.H > step)
        {
            const std::int32_t top
                = PlaneBufferGetter(planeBuffer, step, block.X, UncheckedSubtract(block.Y, 1));
            const std::int32_t bottom
                = PlaneBufferGetter(planeBuffer, step, block.X, UncheckedSubtract(UncheckedAdd(block.Y, block.H), 1));
            const std::int32_t pixel = (top + bottom) / 2;
            const std::int32_t x = block.X;
            const std::int32_t y = UncheckedSubtract(UncheckedAdd(block.Y, block.H / 2), 1);
            planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(pixel);
            PredictPlaneRecursive(block.HalfUp(), planeBuffer, step);
            PredictPlaneRecursive(block.HalfDown(), planeBuffer, step);
        }
        else if (block.W > step && block.H == step)
        {
            const std::int32_t left
                = PlaneBufferGetter(planeBuffer, step, UncheckedSubtract(block.X, 1), block.Y);
            const std::int32_t right
                = PlaneBufferGetter(planeBuffer, step, UncheckedSubtract(UncheckedAdd(block.X, block.W), 1), block.Y);
            const std::int32_t pixel = (left + right) / 2;
            const std::int32_t x = UncheckedSubtract(UncheckedAdd(block.X, block.W / 2), 1);
            const std::int32_t y = block.Y;
            planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(pixel);
            PredictPlaneRecursive(block.HalfLeft(), planeBuffer, step);
            PredictPlaneRecursive(block.HalfRight(), planeBuffer, step);
        }
        else
        {
            const std::int32_t bottomLeft
                = PlaneBufferGetter(planeBuffer, step, UncheckedSubtract(block.X, 1), UncheckedSubtract(UncheckedAdd(block.Y, block.H), 1));
            const std::int32_t topRight
                = PlaneBufferGetter(planeBuffer, step, UncheckedSubtract(UncheckedAdd(block.X, block.W), 1), UncheckedSubtract(block.Y, 1));
            const std::int32_t bottomRight
                = PlaneBufferGetter(planeBuffer, step,
                    UncheckedSubtract(UncheckedAdd(block.X, block.W), 1), UncheckedSubtract(UncheckedAdd(block.Y, block.H), 1));
            const std::int32_t bottomCenter = (bottomLeft + bottomRight) / 2;
            const std::int32_t centerRight = (topRight + bottomRight) / 2;
            std::int32_t pixel;
            std::int32_t x = UncheckedSubtract(UncheckedAdd(block.X, block.W / 2), 1);
            std::int32_t y = UncheckedSubtract(UncheckedAdd(block.Y, block.H), 1);
            planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(bottomCenter);
            x = UncheckedSubtract(UncheckedAdd(block.X, block.W), 1);
            y = UncheckedSubtract(UncheckedAdd(block.Y, block.H / 2), 1);
            planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(centerRight);

            if ((block.W == UncheckedMultiply(4, step) || block.W == UncheckedMultiply(16, step))
                != (block.H == UncheckedMultiply(4, step) || block.H == UncheckedMultiply(16, step)))
            {
                const std::int32_t centerLeft
                    = PlaneBufferGetter(planeBuffer, step,
                        UncheckedSubtract(block.X, 1), UncheckedSubtract(UncheckedAdd(block.Y, block.H / 2), 1));
                pixel = (centerLeft + centerRight) / 2;
            }
            else
            {
                const std::int32_t topCenter
                    = PlaneBufferGetter(planeBuffer, step,
                        UncheckedSubtract(UncheckedAdd(block.X, block.W / 2), 1), UncheckedSubtract(block.Y, 1));
                pixel = (topCenter + bottomCenter) / 2;
            }
            x = UncheckedSubtract(UncheckedAdd(block.X, block.W / 2), 1);
            y = UncheckedSubtract(UncheckedAdd(block.Y, block.H / 2), 1);
            planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(pixel);
            PredictPlaneRecursive(block.HalfLeft().HalfUp(), planeBuffer, step);
            PredictPlaneRecursive(block.HalfRight().HalfUp(), planeBuffer, step);
            PredictPlaneRecursive(block.HalfLeft().HalfDown(), planeBuffer, step);
            PredictPlaneRecursive(block.HalfRight().HalfDown(), planeBuffer, step);
        }
    }

    void VideoFrame::PredictNoTileUV(Block block)
    {
        const std::int32_t mode = _reader->ReadUnsignedExpGolomb();
        if (mode == 0)
        {
            PredictDC(block, RequireManagedReference(_planeBufferU), 2);
            PredictDC(block, RequireManagedReference(_planeBufferV), 2);
        }
        else if (mode == 1)
        {
            PredictHorizontal(block, RequireManagedReference(_planeBufferU), 2);
            PredictHorizontal(block, RequireManagedReference(_planeBufferV), 2);
        }
        else if (mode == 2)
        {
            PredictVertical(block, RequireManagedReference(_planeBufferU), 2);
            PredictVertical(block, RequireManagedReference(_planeBufferV), 2);
        }
        else if (mode == 3)
        {
            PredictPlane(block, RequireManagedReference(_planeBufferU), 2, 0);
            PredictPlane(block, RequireManagedReference(_planeBufferV), 2, 0);
        }
        else
        {
            throw ProgramException("VX decoding error 018: " + std::to_string(mode));
        }
    }

    void VideoFrame::Predict4(Block block)
    {
        std::array<std::int32_t, 25> cache{};
        cache.fill(9);

        for (std::int32_t y2 = 0; y2 < block.H / 4; y2 = UncheckedAdd(y2, 1))
        {
            for (std::int32_t x2 = 0; x2 < block.W / 4; x2 = UncheckedAdd(x2, 1))
            {
                const std::int32_t topIndex = UncheckedAdd(
                    UncheckedMultiply(y2, 5), UncheckedAdd(1, x2));
                const std::int32_t leftIndex = UncheckedAdd(
                    UncheckedMultiply(UncheckedAdd(y2, 1), 5), x2);
                if (topIndex < 0 || leftIndex < 0
                    || static_cast<std::size_t>(topIndex) >= cache.size()
                    || static_cast<std::size_t>(leftIndex) >= cache.size())
                {
                    throw System::IndexOutOfRangeException();
                }
                std::int32_t mode = std::min(
                    cache[static_cast<std::size_t>(topIndex)],
                    cache[static_cast<std::size_t>(leftIndex)]);
                if (mode == 9)
                {
                    mode = 2;
                }

                if (_reader->ReadBit() == 0)
                {
                    const std::int32_t val = _reader->ReadInt(3);
                    mode = UncheckedAdd(val, val >= mode ? 1 : 0);
                }

                const std::int32_t cacheIndex = UncheckedAdd(
                    UncheckedMultiply(UncheckedAdd(y2, 1), 5), UncheckedAdd(1, x2));
                if (cacheIndex < 0 || static_cast<std::size_t>(cacheIndex) >= cache.size())
                {
                    throw System::IndexOutOfRangeException();
                }
                cache[static_cast<std::size_t>(cacheIndex)] = mode;

                const Vector2ir vec(
                    UncheckedAdd(block.X, UncheckedMultiply(x2, 4)),
                    UncheckedAdd(block.Y, UncheckedMultiply(y2, 4)));
                if (mode == 0)
                {
                    Predict4x4Vertical(RequireManagedReference(_planeBufferY), vec);
                }
                else if (mode == 1)
                {
                    Predict4x4Horizontal(RequireManagedReference(_planeBufferY), vec);
                }
                else if (mode == 2)
                {
                    if (vec.X != 0 && vec.Y != 0)
                    {
                        Predict4x4Dc(RequireManagedReference(_planeBufferY), vec);
                    }
                    else if (vec.X != 0)
                    {
                        Predict4x4LeftDc(RequireManagedReference(_planeBufferY), vec);
                    }
                    else if (vec.Y != 0)
                    {
                        Predict4x4TopDc(RequireManagedReference(_planeBufferY), vec);
                    }
                    else
                    {
                        Predict4x4Dc128(RequireManagedReference(_planeBufferY), vec);
                    }
                }
                else if (mode == 3)
                {
                    Predict4x4DownLeft(RequireManagedReference(_planeBufferY), vec);
                }
                else if (mode == 4)
                {
                    Predict4x4DownRight(RequireManagedReference(_planeBufferY), vec);
                }
                else if (mode == 5)
                {
                    Predict4x4VerticalRight(RequireManagedReference(_planeBufferY), vec);
                }
                else if (mode == 6)
                {
                    Predict4x4HorizontalDown(RequireManagedReference(_planeBufferY), vec);
                }
                else if (mode == 7)
                {
                    Predict4x4VerticalLeft(RequireManagedReference(_planeBufferY), vec);
                }
                else if (mode == 8)
                {
                    Predict4x4HorizontalUp(RequireManagedReference(_planeBufferY), vec);
                }
                else
                {
                    throw ProgramException("VX decoding error 019: " + std::to_string(mode));
                }
            }
        }

        PredictNoTileUV(block);
    }

    void VideoFrame::Predict4x4Vertical(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        for (std::int32_t y = 0; y < 4; y = UncheckedAdd(y, 1))
        {
            for (std::int32_t x = 0; x < 4; x = UncheckedAdd(x, 1))
            {
                planeBuffer(UncheckedAdd(vec.Y, y), UncheckedAdd(vec.X, x)) = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, x));
            }
        }
    }

    void VideoFrame::Predict4x4Horizontal(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        for (std::int32_t y = 0; y < 4; y = UncheckedAdd(y, 1))
        {
            const std::uint8_t value = planeBuffer(UncheckedAdd(vec.Y, y), UncheckedSubtract(vec.X, 1));
            for (std::int32_t x = 0; x < 4; x = UncheckedAdd(x, 1))
            {
                planeBuffer(UncheckedAdd(vec.Y, y), UncheckedAdd(vec.X, x)) = value;
            }
        }
    }

    void VideoFrame::Predict4x4Dc(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::uint8_t value = static_cast<std::uint8_t>(
            (planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 0))
                + planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 1))
                + planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 2))
                + planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 3))
                + planeBuffer(UncheckedAdd(vec.Y, 0), UncheckedSubtract(vec.X, 1))
                + planeBuffer(UncheckedAdd(vec.Y, 1), UncheckedSubtract(vec.X, 1))
                + planeBuffer(UncheckedAdd(vec.Y, 2), UncheckedSubtract(vec.X, 1))
                + planeBuffer(UncheckedAdd(vec.Y, 3), UncheckedSubtract(vec.X, 1)) + 4) / 8);
        for (std::int32_t y = 0; y < 4; y = UncheckedAdd(y, 1))
        {
            for (std::int32_t x = 0; x < 4; x = UncheckedAdd(x, 1))
            {
                planeBuffer(UncheckedAdd(vec.Y, y), UncheckedAdd(vec.X, x)) = value;
            }
        }
    }

    void VideoFrame::Predict4x4LeftDc(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::uint8_t value = static_cast<std::uint8_t>(
            (planeBuffer(UncheckedAdd(vec.Y, 0), UncheckedSubtract(vec.X, 1))
                + planeBuffer(UncheckedAdd(vec.Y, 1), UncheckedSubtract(vec.X, 1))
                + planeBuffer(UncheckedAdd(vec.Y, 2), UncheckedSubtract(vec.X, 1))
                + planeBuffer(UncheckedAdd(vec.Y, 3), UncheckedSubtract(vec.X, 1)) + 2) / 4);
        for (std::int32_t y = 0; y < 4; y = UncheckedAdd(y, 1))
        {
            for (std::int32_t x = 0; x < 4; x = UncheckedAdd(x, 1))
            {
                planeBuffer(UncheckedAdd(vec.Y, y), UncheckedAdd(vec.X, x)) = value;
            }
        }
    }

    void VideoFrame::Predict4x4TopDc(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::uint8_t value = static_cast<std::uint8_t>(
            (planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 0))
                + planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 1))
                + planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 2))
                + planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 3)) + 2) / 4);
        for (std::int32_t y = 0; y < 4; y = UncheckedAdd(y, 1))
        {
            for (std::int32_t x = 0; x < 4; x = UncheckedAdd(x, 1))
            {
                planeBuffer(UncheckedAdd(vec.Y, y), UncheckedAdd(vec.X, x)) = value;
            }
        }
    }

    void VideoFrame::Predict4x4Dc128(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        for (std::int32_t y = 0; y < 4; y = UncheckedAdd(y, 1))
        {
            for (std::int32_t x = 0; x < 4; x = UncheckedAdd(x, 1))
            {
                planeBuffer(UncheckedAdd(vec.Y, y), UncheckedAdd(vec.X, x)) = 128;
            }
        }
    }

    void VideoFrame::Predict4x4DownLeft(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t t0 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 0));
        const std::int32_t t1 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 1));
        const std::int32_t t2 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 2));
        const std::int32_t t3 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 3));
        const std::int32_t t4 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 4));
        const std::int32_t t5 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 5));
        const std::int32_t t6 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 6));
        const std::int32_t t7 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 7));

        const std::array<std::int32_t, 7> pixels{
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + 2 * t2 + t3 + 2) / 4,
            (t2 + 2 * t3 + t4 + 2) / 4,
            (t3 + 2 * t4 + t5 + 2) / 4,
            (t4 + 2 * t5 + t6 + 2) / 4,
            (t5 + 2 * t6 + t7 + 2) / 4,
            (t6 + 3 * t7 + 2) / 4
        };

        for (std::int32_t y = 0; y < 4; y = UncheckedAdd(y, 1))
        {
            for (std::int32_t x = 0; x < 4; x = UncheckedAdd(x, 1))
            {
                planeBuffer(UncheckedAdd(vec.Y, y), UncheckedAdd(vec.X, x))
                    = static_cast<std::uint8_t>(pixels[static_cast<std::size_t>(x + y)]);
            }
        }
    }

    void VideoFrame::Predict4x4DownRight(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t lt = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedSubtract(vec.X, 1));
        const std::int32_t t0 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 0));
        const std::int32_t t1 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 1));
        const std::int32_t t2 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 2));
        const std::int32_t t3 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 3));
        const std::int32_t l0 = planeBuffer(UncheckedAdd(vec.Y, 0), UncheckedSubtract(vec.X, 1));
        const std::int32_t l1 = planeBuffer(UncheckedAdd(vec.Y, 1), UncheckedSubtract(vec.X, 1));
        const std::int32_t l2 = planeBuffer(UncheckedAdd(vec.Y, 2), UncheckedSubtract(vec.X, 1));
        const std::int32_t l3 = planeBuffer(UncheckedAdd(vec.Y, 3), UncheckedSubtract(vec.X, 1));

        const std::array<std::int32_t, 7> pixels{
            (l3 + 2 * l2 + l1 + 2) / 4,
            (l2 + 2 * l1 + l0 + 2) / 4,
            (l1 + 2 * l0 + lt + 2) / 4,
            (l0 + 2 * lt + t0 + 2) / 4,
            (lt + 2 * t0 + t1 + 2) / 4,
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + 2 * t2 + t3 + 2) / 4
        };

        for (std::int32_t y = 0; y < 4; y = UncheckedAdd(y, 1))
        {
            for (std::int32_t x = 0; x < 4; x = UncheckedAdd(x, 1))
            {
                planeBuffer(UncheckedAdd(vec.Y, y), UncheckedAdd(vec.X, x))
                    = static_cast<std::uint8_t>(
                        pixels[static_cast<std::size_t>(3 + x - y)]);
            }
        }
    }

    void VideoFrame::Predict4x4VerticalRight(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t lt = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedSubtract(vec.X, 1));
        const std::int32_t t0 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 0));
        const std::int32_t t1 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 1));
        const std::int32_t t2 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 2));
        const std::int32_t t3 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 3));
        const std::int32_t l0 = planeBuffer(UncheckedAdd(vec.Y, 0), UncheckedSubtract(vec.X, 1));
        const std::int32_t l1 = planeBuffer(UncheckedAdd(vec.Y, 1), UncheckedSubtract(vec.X, 1));
        const std::int32_t l2 = planeBuffer(UncheckedAdd(vec.Y, 2), UncheckedSubtract(vec.X, 1));

        const std::array<std::int32_t, 10> pixels{
            (l0 + 2 * l1 + l2 + 2) / 4,
            (lt + 2 * l0 + l1 + 2) / 4,
            (l0 + 2 * lt + t0 + 2) / 4,
            (lt + t0 + 1) / 2,
            (lt + 2 * t0 + t1 + 2) / 4,
            (t0 + t1 + 1) / 2,
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + t2 + 1) / 2,
            (t1 + 2 * t2 + t3 + 2) / 4,
            (t2 + t3 + 1) / 2
        };

        for (std::int32_t y = 0; y < 4; y = UncheckedAdd(y, 1))
        {
            for (std::int32_t x = 0; x < 4; x = UncheckedAdd(x, 1))
            {
                planeBuffer(UncheckedAdd(vec.Y, y), UncheckedAdd(vec.X, x))
                    = static_cast<std::uint8_t>(
                        pixels[static_cast<std::size_t>(3 + 2 * x - y)]);
            }
        }
    }

    void VideoFrame::Predict4x4HorizontalDown(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t lt = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedSubtract(vec.X, 1));
        const std::int32_t t0 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 0));
        const std::int32_t t1 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 1));
        const std::int32_t t2 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 2));
        const std::int32_t l0 = planeBuffer(UncheckedAdd(vec.Y, 0), UncheckedSubtract(vec.X, 1));
        const std::int32_t l1 = planeBuffer(UncheckedAdd(vec.Y, 1), UncheckedSubtract(vec.X, 1));
        const std::int32_t l2 = planeBuffer(UncheckedAdd(vec.Y, 2), UncheckedSubtract(vec.X, 1));
        const std::int32_t l3 = planeBuffer(UncheckedAdd(vec.Y, 3), UncheckedSubtract(vec.X, 1));

        const std::array<std::int32_t, 10> pixels{
            (t0 + 2 * t1 + t2 + 2) / 4,
            (lt + 2 * t0 + t1 + 2) / 4,
            (l0 + 2 * lt + t0 + 2) / 4,
            (lt + l0 + 1) / 2,
            (lt + 2 * l0 + l1 + 2) / 4,
            (l0 + l1 + 1) / 2,
            (l0 + 2 * l1 + l2 + 2) / 4,
            (l1 + l2 + 1) / 2,
            (l1 + 2 * l2 + l3 + 2) / 4,
            (l2 + l3 + 1) / 2
        };

        for (std::int32_t y = 0; y < 4; y = UncheckedAdd(y, 1))
        {
            for (std::int32_t x = 0; x < 4; x = UncheckedAdd(x, 1))
            {
                planeBuffer(UncheckedAdd(vec.Y, y), UncheckedAdd(vec.X, x))
                    = static_cast<std::uint8_t>(
                        pixels[static_cast<std::size_t>(3 - x + 2 * y)]);
            }
        }
    }

    void VideoFrame::Predict4x4VerticalLeft(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t t0 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 0));
        const std::int32_t t1 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 1));
        const std::int32_t t2 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 2));
        const std::int32_t t3 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 3));
        const std::int32_t t4 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 4));
        const std::int32_t t5 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 5));
        const std::int32_t t6 = planeBuffer(UncheckedSubtract(vec.Y, 1), UncheckedAdd(vec.X, 6));

        const std::array<std::int32_t, 10> pixels{
            (t0 + t1 + 1) / 2,
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + t2 + 1) / 2,
            (t1 + 2 * t2 + t3 + 2) / 4,
            (t2 + t3 + 1) / 2,
            (t2 + 2 * t3 + t4 + 2) / 4,
            (t3 + t4 + 1) / 2,
            (t3 + 2 * t4 + t5 + 2) / 4,
            (t4 + t5 + 1) / 2,
            (t4 + 2 * t5 + t6 + 2) / 4
        };

        for (std::int32_t y = 0; y < 4; y = UncheckedAdd(y, 1))
        {
            for (std::int32_t x = 0; x < 4; x = UncheckedAdd(x, 1))
            {
                planeBuffer(UncheckedAdd(vec.Y, y), UncheckedAdd(vec.X, x))
                    = static_cast<std::uint8_t>(
                        pixels[static_cast<std::size_t>(2 * x + y)]);
            }
        }
    }

    void VideoFrame::Predict4x4HorizontalUp(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t l0 = planeBuffer(UncheckedAdd(vec.Y, 0), UncheckedSubtract(vec.X, 1));
        const std::int32_t l1 = planeBuffer(UncheckedAdd(vec.Y, 1), UncheckedSubtract(vec.X, 1));
        const std::int32_t l2 = planeBuffer(UncheckedAdd(vec.Y, 2), UncheckedSubtract(vec.X, 1));
        const std::int32_t l3 = planeBuffer(UncheckedAdd(vec.Y, 3), UncheckedSubtract(vec.X, 1));

        const std::array<std::int32_t, 7> pixels{
            (l0 + l1 + 1) / 2,
            (l0 + 2 * l1 + l2 + 2) / 4,
            (l1 + l2 + 1) / 2,
            (l1 + 2 * l2 + l3 + 2) / 4,
            (l2 + l3 + 1) / 2,
            (l2 + 2 * l3 + l3 + 2) / 4,
            l3
        };

        for (std::int32_t y = 0; y < 4; y = UncheckedAdd(y, 1))
        {
            for (std::int32_t x = 0; x < 4; x = UncheckedAdd(x, 1))
            {
                planeBuffer(UncheckedAdd(vec.Y, y), UncheckedAdd(vec.X, x))
                    = static_cast<std::uint8_t>(
                        pixels[static_cast<std::size_t>(std::min(x + 2 * y, 6))]);
            }
        }
    }

    std::uint8_t VideoFrame::GetCoeffBuffer(
        const ByteArray2D& buffer, std::int32_t step, std::int32_t x, std::int32_t y)
    {
        return buffer(y / (step * 4) + 1, x / (step * 4) + 1);
    }

    void VideoFrame::SetCoeffBuffer(
        ByteArray2D& buffer, std::int32_t step, std::int32_t x, std::int32_t y,
        std::uint8_t value)
    {
        buffer(y / (step * 4) + 1, x / (step * 4) + 1) = value;
    }

    void VideoFrame::ClearTotalCoeff(Block block)
    {
        for (std::int32_t y = 0; y < block.H; y = UncheckedAdd(y, 8))
        {
            for (std::int32_t x = 0; x < block.W; x = UncheckedAdd(x, 8))
            {
                const std::int32_t bx = UncheckedAdd(block.X, x);
                const std::int32_t by = UncheckedAdd(block.Y, y);
                const std::int32_t bx4 = UncheckedAdd(bx, 4);
                const std::int32_t by4 = UncheckedAdd(by, 4);
                SetCoeffBuffer(RequireManagedReference(_coeffBufferY), 1, bx, by, 0);
                SetCoeffBuffer(RequireManagedReference(_coeffBufferY), 1, bx, by4, 0);
                SetCoeffBuffer(RequireManagedReference(_coeffBufferY), 1, bx4, by, 0);
                SetCoeffBuffer(RequireManagedReference(_coeffBufferY), 1, bx4, by4, 0);
                SetCoeffBuffer(RequireManagedReference(_coeffBufferUV), 2, bx, by, 0);
            }
        }
    }

    const std::array<std::int32_t, 4> AudioFrame::_pulseDataLengths{8, 5, 4, 3};
    const std::array<std::int32_t, 4> AudioFrame::_pulseDistances{3, 3, 4, 5};

    AudioFrame::AudioFrame(
        std::shared_ptr<AudioExtradata> extradata,
        std::shared_ptr<AudioFrame> prevAudioFrame,
        const VxBuffers& buffers,
        std::int32_t sampleBufferIndex)
        : _extradata(std::move(extradata)),
          _prevAudioFrame(std::move(prevAudioFrame)),
          _prevSampleBuffer(buffers.PrevSampleBuffer),
          _prevPulseBuffer(buffers.PrevPulseBuffer),
          _lpcFilterBuffer(buffers.LpcFilterBuffer),
          _influenceBuffer(buffers.InfluenceBuffer),
          _sampleBuffer(buffers.SampleBuffer),
          _sampleBufferIndex(sampleBufferIndex)
    {
    }

    std::span<std::int16_t> AudioFrame::SampleBuffer()
    {
        if (!_sampleBuffer)
        {
            throw System::NullReferenceException();
        }
        const std::int32_t start = UncheckedMultiply(128, _sampleBufferIndex);
        if (start < 0 || start > RequireManagedReference(_sampleBuffer).Length()
            || RequireManagedReference(_sampleBuffer).Length() - start < 128)
        {
            throw System::ArgumentOutOfRangeException("start");
        }
        return std::span<std::int16_t>(std::addressof(RequireManagedReference(_sampleBuffer)[start]), 128);
    }

    std::span<const std::int16_t> AudioFrame::SampleBuffer() const
    {
        if (!_sampleBuffer)
        {
            throw System::NullReferenceException();
        }
        const std::int32_t start = UncheckedMultiply(128, _sampleBufferIndex);
        if (start < 0 || start > RequireManagedReference(_sampleBuffer).Length()
            || RequireManagedReference(_sampleBuffer).Length() - start < 128)
        {
            throw System::ArgumentOutOfRangeException("start");
        }
        return std::span<const std::int16_t>(std::addressof(RequireManagedReference(_sampleBuffer)[start]), 128);
    }

    void AudioFrame::Decode(BitStreamReader& reader)
    {
        _reader = &reader;
        if (!_extradata)
        {
            throw System::NullReferenceException();
        }
        std::array<std::int32_t, 3> lpcCodebookIndices{};
        const std::int32_t header1 = reader.ReadInt(16);
        const std::int32_t header2 = reader.ReadInt(16);
        lpcCodebookIndices[0] = header1 & 0x3F;
        const std::int32_t scaleModifierIndex = (header1 >> 6) & 7;
        const std::int32_t prevFrameOffset = (header1 >> 9) & 0x7F;
        lpcCodebookIndices[2] = header2 & 0x3F;
        lpcCodebookIndices[1] = (header2 >> 6) & 0x3F;
        const std::int32_t pulsePackingMode = (header2 >> 12) & 3;
        const std::int32_t pulseStartPosition = (header2 >> 14) & 3;

        if (pulsePackingMode >= static_cast<std::int32_t>(_pulseDataLengths.size()))
        {
            throw ProgramException(
                "VX decoding error 020: " + std::to_string(pulsePackingMode));
        }
        if (scaleModifierIndex >= _extradata->ScaleModifiers->Length())
        {
            throw ProgramException(
                "VX decoding error 021: " + std::to_string(scaleModifierIndex)
                + ", " + std::to_string(_extradata->ScaleModifiers->Length()));
        }

        const std::int32_t pulseDataLength
            = _pulseDataLengths[static_cast<std::size_t>(pulsePackingMode)];
        std::array<std::uint16_t, 8> pulseData{};
        for (std::int32_t i = 0; i < pulseDataLength; ++i)
        {
            pulseData[static_cast<std::size_t>(i)]
                = static_cast<std::uint16_t>(reader.ReadInt(16));
        }

        std::array<std::int32_t, 42> pulseValues{};
        std::int32_t pulseValueLength
            = pulsePackingMode == 0 ? 42 : UncheckedMultiply(pulseDataLength, 8);
        if (pulsePackingMode == 0)
        {
            std::int32_t v = 0;
            for (std::int32_t i = 0; i < pulseDataLength; ++i)
            {
                for (std::int32_t j = 13; j >= 0; j -= 3)
                {
                    pulseValues[static_cast<std::size_t>(v++)]
                        = ((pulseData[static_cast<std::size_t>(i)] >> j) & 7) * 2 - 7;
                }
            }
            pulseValues[static_cast<std::size_t>(v++)]
                = ((pulseData[0] & 1) * 4 + (pulseData[1] & 1) * 2
                    + (pulseData[2] & 1)) * 2 - 7;
            pulseValues[static_cast<std::size_t>(v++)]
                = ((pulseData[3] & 1) * 4 + (pulseData[4] & 1) * 2
                    + (pulseData[5] & 1)) * 2 - 7;
        }
        else
        {
            std::int32_t v = 0;
            for (std::int32_t i = 0; i < pulseDataLength; ++i)
            {
                for (std::int32_t j = 14; j >= 0; j -= 2)
                {
                    pulseValues[static_cast<std::size_t>(v++)]
                        = ((pulseData[static_cast<std::size_t>(i)] >> j) & 3) * 2 - 3;
                }
            }
        }

        _scale = _extradata->ScaleInitial;
        if (prevFrameOffset != 127)
        {
            assert(_prevAudioFrame != nullptr);
            if (!_prevAudioFrame)
            {
                throw System::NullReferenceException();
            }
            _scale = _prevAudioFrame->Scale();
        }
        _scale = UncheckedMultiply(
            _scale,
            RequireManagedReference(_extradata->ScaleModifiers)[static_cast<std::size_t>(scaleModifierIndex)]) / 8192;

        const std::int32_t pulseDistance
            = _pulseDistances[static_cast<std::size_t>(pulsePackingMode)];

        // C# stackalloc memory is zeroed (the assembly does not opt out with
        // SkipLocalsInit); the loop below leaves it untouched when
        // prevFrameOffset >= 126, and those zeros are what the frame adds to.
        std::array<std::int32_t, 128> pulseBuffer{};
        if (prevFrameOffset < 126)
        {
            for (std::int32_t i = 0; i < 128; i = UncheckedAdd(i, 1))
            {
                const std::int32_t volume = std::min(8,
                    std::min(UncheckedAdd(i, 1), UncheckedSubtract(128, i)));
                const std::int32_t previousIndex = UncheckedSubtract(
                    UncheckedAdd(i, 127), prevFrameOffset);
                pulseBuffer[static_cast<std::size_t>(i)] = UncheckedMultiply(
                    RequireManagedReference(_prevPulseBuffer)[static_cast<std::size_t>(previousIndex)], volume) / 16;
            }
        }

        for (std::int32_t i = 0; i < 128; i = UncheckedAdd(i, 1))
        {
            const std::int32_t dividend = UncheckedSubtract(i, pulseStartPosition);
            const std::int32_t index = dividend / pulseDistance;
            const std::int32_t remainder = dividend % pulseDistance;
            if (remainder == 0 && index >= 0 && index < pulseValueLength)
            {
                pulseBuffer[static_cast<std::size_t>(i)] = UncheckedAdd(
                    pulseBuffer[static_cast<std::size_t>(i)],
                    UncheckedMultiply(pulseValues[static_cast<std::size_t>(index)], _scale));
            }
        }

        if (prevFrameOffset == 127)
        {
            for (std::int32_t i = 0; i < _extradata->LpcBase->Length();
                i = UncheckedAdd(i, 1))
            {
                RequireManagedReference(_lpcFilterBuffer)[i] = RequireManagedReference(_extradata->LpcBase)[i];
            }
        }

        for (std::int32_t i = 0; i < 8; i = UncheckedAdd(i, 1))
        {
            std::int32_t coeffSum = 0;
            for (std::int32_t j = 0; j < 3; j = UncheckedAdd(j, 1))
            {
                const std::int32_t index = lpcCodebookIndices[static_cast<std::size_t>(j)];
                coeffSum = UncheckedAdd(coeffSum,
                    RequireManagedReference(_extradata->LpcCodebooks)(j, index, i));
            }
            RequireManagedReference(_lpcFilterBuffer)[static_cast<std::size_t>(i)] = UncheckedAdd(
                RequireManagedReference(_lpcFilterBuffer)[static_cast<std::size_t>(i)], coeffSum);
        }

        std::array<std::int32_t, 8> influenceValues{};
        std::array<std::int32_t, 8> influenceTemp{};
        for (std::int32_t i = 0; i < 8; i = UncheckedAdd(i, 1))
        {
            std::copy(influenceValues.begin(), influenceValues.end(), influenceTemp.begin());
            const std::int32_t coeff
                = RequireManagedReference(_lpcFilterBuffer)[static_cast<std::size_t>(i)];
            for (std::int32_t j = 0; j < i; j = UncheckedAdd(j, 1))
            {
                const std::int32_t sourceIndex = UncheckedSubtract(
                    UncheckedSubtract(i, j), 1);
                const std::int32_t contribution = UncheckedMultiply(
                    influenceTemp[static_cast<std::size_t>(sourceIndex)], coeff) / 32768;
                influenceValues[static_cast<std::size_t>(j)] = UncheckedAdd(
                    influenceValues[static_cast<std::size_t>(j)], contribution);
            }
            influenceValues[static_cast<std::size_t>(i)] = coeff;
        }
        for (std::int32_t i = 0; i < 8; i = UncheckedAdd(i, 1))
        {
            influenceValues[static_cast<std::size_t>(i)] /= -2;
        }

        std::array<std::int32_t, 32> influenceQuarters{};
        if (prevFrameOffset != 127)
        {
            assert(_prevAudioFrame != nullptr);
            std::copy(influenceValues.begin(), influenceValues.end(),
                influenceQuarters.begin() + 24);
            for (std::int32_t i = 0; i < 8; ++i)
            {
                influenceQuarters[static_cast<std::size_t>(8 + i)]
                    = UncheckedAdd(RequireManagedReference(_influenceBuffer)[static_cast<std::size_t>(i)],
                        influenceQuarters[static_cast<std::size_t>(UncheckedAdd(24, i))]) / 2;
            }
            for (std::int32_t i = 0; i < 8; ++i)
            {
                influenceQuarters[static_cast<std::size_t>(i)]
                    = UncheckedAdd(RequireManagedReference(_influenceBuffer)[static_cast<std::size_t>(i)],
                        influenceQuarters[static_cast<std::size_t>(UncheckedAdd(8, i))]) / 2;
            }
            for (std::int32_t i = 0; i < 8; ++i)
            {
                influenceQuarters[static_cast<std::size_t>(16 + i)]
                    = UncheckedAdd(
                        influenceQuarters[static_cast<std::size_t>(UncheckedAdd(8, i))],
                        influenceQuarters[static_cast<std::size_t>(UncheckedAdd(24, i))]) / 2;
            }
        }
        else
        {
            for (std::int32_t q = 0; q < 4; ++q)
            {
                std::copy(influenceValues.begin(), influenceValues.end(),
                    influenceQuarters.begin() + q * 8);
            }
        }

        std::span<std::int16_t> sampleBuffer = SampleBuffer();
        std::fill(sampleBuffer.begin(), sampleBuffer.end(), 0);
        for (std::int32_t i = 0; i < 128; i = UncheckedAdd(i, 1))
        {
            const std::int32_t quarterStart = UncheckedMultiply(
                UncheckedMultiply(i, 4) / 128, 8);
            std::int32_t sample = UncheckedMultiply(
                pulseBuffer[static_cast<std::size_t>(i)], 16384);
            for (std::int32_t j = 0; j < 8; j = UncheckedAdd(j, 1))
            {
                const std::int32_t sampleIndex = UncheckedSubtract(
                    UncheckedSubtract(i, j), 1);
                const std::int32_t prevSample = sampleIndex >= 0
                    ? sampleBuffer[static_cast<std::size_t>(sampleIndex)]
                    : RequireManagedReference(_prevSampleBuffer)[static_cast<std::size_t>(UncheckedAdd(sampleIndex, 8))];
                sample = UncheckedAdd(sample, UncheckedMultiply(
                    prevSample,
                    influenceQuarters[static_cast<std::size_t>(
                        UncheckedAdd(quarterStart, j))]));
            }
            sample /= 16384;
            sampleBuffer[static_cast<std::size_t>(i)]
                = static_cast<std::int16_t>(
                    MathClamp(sample, std::numeric_limits<std::int16_t>::min(),
                        std::numeric_limits<std::int16_t>::max()));
        }

        for (std::int32_t i = 0; i < 128; i = UncheckedAdd(i, 1))
        {
            RequireManagedReference(_prevPulseBuffer)[i] = RequireManagedReference(_prevPulseBuffer)[UncheckedAdd(i, 128)];
            RequireManagedReference(_prevPulseBuffer)[UncheckedAdd(i, 128)] = pulseBuffer[static_cast<std::size_t>(i)];
        }
        for (std::int32_t i = 0; i < 8; i = UncheckedAdd(i, 1))
        {
            RequireManagedReference(_prevSampleBuffer)[i] = sampleBuffer[static_cast<std::size_t>(UncheckedAdd(i, 120))];
            RequireManagedReference(_influenceBuffer)[i] = influenceValues[static_cast<std::size_t>(i)];
        }
        _reader = nullptr;
    }

    VLCData::VLCData(
        std::span<const std::int32_t> lengthList, std::span<const std::int32_t> bitList)
    {
        for (std::size_t i = 0; i < lengthList.size(); ++i)
        {
            if (i >= bitList.size())
            {
                throw System::IndexOutOfRangeException();
            }
            const std::int32_t length = lengthList[i];
            const std::int32_t value = bitList[i];
            if (length != 0)
            {
                if (length < 0)
                {
                    throw System::ArgumentOutOfRangeException("totalWidth");
                }
                std::string bitString;
                if (value == 0)
                {
                    bitString = "0";
                }
                else
                {
                    const std::uint32_t unsignedValue = std::bit_cast<std::uint32_t>(value);
                    const int width = 32 - std::countl_zero(unsignedValue);
                    bitString.reserve(static_cast<std::size_t>(width));
                    for (int bit = width - 1; bit >= 0; --bit)
                    {
                        bitString.push_back(
                            ((unsignedValue >> bit) & 1U) != 0U ? '1' : '0');
                    }
                }
                if (bitString.size() < static_cast<std::size_t>(length))
                {
                    bitString.insert(bitString.begin(),
                        static_cast<std::size_t>(length) - bitString.size(), '0');
                }

                std::int32_t hashCode = 0;
                for (char c : bitString)
                {
                    hashCode = ::MphRead::NativeRuntime::HashCodeCombine(hashCode, c - '0');
                }
                const auto [it, inserted] = _bitDict.emplace(
                    hashCode, static_cast<std::int32_t>(i));
                (void)it;
                if (!inserted)
                {
                    throw System::ArgumentException(
                        "An item with the same key has already been added.");
                }
                _maxBitCount = std::max(
                    _maxBitCount, static_cast<std::int32_t>(bitString.size()));
            }
        }
    }

    std::int32_t VLCData::FindBitPattern(std::int32_t hashCode) const noexcept
    {
        const auto item = _bitDict.find(hashCode);
        return item == _bitDict.end() ? -1 : item->second;
    }

    struct VLC::State
    {
        std::vector<VLCData> CoeffTokenVlc;
        std::vector<VLCData> TotalZeroesVlc;
        std::vector<VLCData> RunVlc;
        VLCData Run7Vlc;
        std::int32_t Temp = 0;

        State()
            : Run7Vlc(
                std::array<std::int32_t, 15>{3, 3, 3, 3, 3, 3, 3, 4, 5, 6, 7, 8, 9, 10, 11},
                std::array<std::int32_t, 15>{7, 6, 5, 4, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1})
        {
            static const std::array<std::array<std::int32_t, 68>, 4> coeffTokenLengths{{
                {{
                    1, 0, 0, 0,
                    6, 2, 0, 0, 8, 6, 3, 0, 9, 8, 7, 5, 10, 9, 8, 6,
                    11, 10, 9, 7, 13, 11, 10, 8, 13, 13, 11, 9, 13, 13, 13, 10,
                    14, 14, 13, 11, 14, 14, 14, 13, 15, 15, 14, 14, 15, 15, 15, 14,
                    16, 15, 15, 15, 16, 16, 16, 15, 16, 16, 16, 16, 16, 16, 16, 16
                }},
                {{
                    2, 0, 0, 0,
                    6, 2, 0, 0, 6, 5, 3, 0, 7, 6, 6, 4, 8, 6, 6, 4,
                    8, 7, 7, 5, 9, 8, 8, 6, 11, 9, 9, 6, 11, 11, 11, 7,
                    12, 11, 11, 9, 12, 12, 12, 11, 12, 12, 12, 11, 13, 13, 13, 12,
                    13, 13, 13, 13, 13, 14, 13, 13, 14, 14, 14, 13, 14, 14, 14, 14
                }},
                {{
                    4, 0, 0, 0,
                    6, 4, 0, 0, 6, 5, 4, 0, 6, 5, 5, 4, 7, 5, 5, 4,
                    7, 5, 5, 4, 7, 6, 6, 4, 7, 6, 6, 4, 8, 7, 7, 5,
                    8, 8, 7, 6, 9, 8, 8, 7, 9, 9, 8, 8, 9, 9, 9, 8,
                    10, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10
                }},
                {{
                    6, 0, 0, 0,
                    6, 6, 0, 0, 6, 6, 6, 0, 6, 6, 6, 6, 6, 6, 6, 6,
                    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
                }}
            }};
            static const std::array<std::array<std::int32_t, 68>, 4> coeffTokenBits{{
                {{
                    1, 0, 0, 0,
                    5, 1, 0, 0, 7, 4, 1, 0, 7, 6, 5, 3, 7, 6, 5, 3,
                    7, 6, 5, 4, 15, 6, 5, 4, 11, 14, 5, 4, 8, 10, 13, 4,
                    15, 14, 9, 4, 11, 10, 13, 12, 15, 14, 9, 12, 11, 10, 13, 8,
                    15, 1, 9, 12, 11, 14, 13, 8, 7, 10, 9, 12, 4, 6, 5, 8
                }},
                {{
                    3, 0, 0, 0,
                    11, 2, 0, 0, 7, 7, 3, 0, 7, 10, 9, 5, 7, 6, 5, 4,
                    4, 6, 5, 6, 7, 6, 5, 8, 15, 6, 5, 4, 11, 14, 13, 4,
                    15, 10, 9, 4, 11, 14, 13, 12, 8, 10, 9, 8, 15, 14, 13, 12,
                    11, 10, 9, 12, 7, 11, 6, 8, 9, 8, 10, 1, 7, 6, 5, 4
                }},
                {{
                    15, 0, 0, 0,
                    15, 14, 0, 0, 11, 15, 13, 0, 8, 12, 14, 12, 15, 10, 11, 11,
                    11, 8, 9, 10, 9, 14, 13, 9, 8, 10, 9, 8, 15, 14, 13, 13,
                    11, 14, 10, 12, 15, 10, 13, 12, 11, 14, 9, 12, 8, 10, 13, 8,
                    13, 7, 9, 12, 9, 12, 11, 10, 5, 8, 7, 6, 1, 4, 3, 2
                }},
                {{
                    3, 0, 0, 0,
                    0, 1, 0, 0, 4, 5, 6, 0, 8, 9, 10, 11, 12, 13, 14, 15,
                    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
                    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
                    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
                }}
            }};

            static const std::array<std::vector<std::int32_t>, 16> totalZeroesLengths{{
                {},
                {1, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9},
                {3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 6, 6, 6, 6},
                {4, 3, 3, 3, 4, 4, 3, 3, 4, 5, 5, 6, 5, 6},
                {5, 3, 4, 4, 3, 3, 3, 4, 3, 4, 5, 5, 5},
                {4, 4, 4, 3, 3, 3, 3, 3, 4, 5, 4, 5},
                {6, 5, 3, 3, 3, 3, 3, 3, 4, 3, 6},
                {6, 5, 3, 3, 3, 2, 3, 4, 3, 6},
                {6, 4, 5, 3, 2, 2, 3, 3, 6},
                {6, 6, 4, 2, 2, 3, 2, 5},
                {5, 5, 3, 2, 2, 2, 4},
                {4, 4, 3, 3, 1, 3},
                {4, 4, 2, 1, 3},
                {3, 3, 1, 2},
                {2, 2, 1},
                {1, 1}
            }};
            static const std::array<std::vector<std::int32_t>, 16> totalZeroesBits{{
                {},
                {1, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 1},
                {7, 6, 5, 4, 3, 5, 4, 3, 2, 3, 2, 3, 2, 1, 0},
                {5, 7, 6, 5, 4, 3, 4, 3, 2, 3, 2, 1, 1, 0},
                {3, 7, 5, 4, 6, 5, 4, 3, 3, 2, 2, 1, 0},
                {5, 4, 3, 7, 6, 5, 4, 3, 2, 1, 1, 0},
                {1, 1, 7, 6, 5, 4, 3, 2, 1, 1, 0},
                {1, 1, 5, 4, 3, 3, 2, 1, 1, 0},
                {1, 1, 1, 3, 3, 2, 2, 1, 0},
                {1, 0, 1, 3, 2, 1, 1, 1},
                {1, 0, 1, 3, 2, 1, 1},
                {0, 1, 1, 2, 1, 3},
                {0, 1, 1, 1, 1},
                {0, 1, 1, 1},
                {0, 1, 1},
                {0, 1}
            }};

            static const std::array<std::vector<std::int32_t>, 7> runLengths{{
                {}, {1, 1}, {1, 2, 2}, {2, 2, 2, 2},
                {2, 2, 2, 3, 3}, {2, 2, 3, 3, 3, 3}, {2, 3, 3, 3, 3, 3, 3}
            }};
            static const std::array<std::vector<std::int32_t>, 7> runBits{{
                {}, {1, 0}, {1, 1, 0}, {3, 2, 1, 0},
                {3, 2, 1, 1, 0}, {3, 2, 3, 2, 1, 0}, {3, 0, 1, 3, 2, 5, 4}
            }};

            CoeffTokenVlc.reserve(coeffTokenLengths.size());
            for (std::size_t i = 0; i < coeffTokenLengths.size(); ++i)
            {
                CoeffTokenVlc.emplace_back(coeffTokenLengths[i], coeffTokenBits[i]);
            }

            TotalZeroesVlc.reserve(totalZeroesLengths.size());
            for (std::size_t i = 0; i < totalZeroesLengths.size(); ++i)
            {
                TotalZeroesVlc.emplace_back(totalZeroesLengths[i], totalZeroesBits[i]);
            }

            RunVlc.reserve(runLengths.size());
            for (std::size_t i = 0; i < runLengths.size(); ++i)
            {
                RunVlc.emplace_back(runLengths[i], runBits[i]);
            }

            Temp = 1;
        }
    };

    const VLC::State& VLC::GetState()
    {
        static const State state;
        return state;
    }

    std::int32_t VLC::Temp()
    {
        return GetState().Temp;
    }

    const std::vector<VLCData>& VLC::CoeffTokenVlc()
    {
        return GetState().CoeffTokenVlc;
    }

    const std::vector<VLCData>& VLC::TotalZeroesVlc()
    {
        return GetState().TotalZeroesVlc;
    }

    const std::vector<VLCData>& VLC::RunVlc()
    {
        return GetState().RunVlc;
    }

    const VLCData& VLC::Run7Vlc()
    {
        return GetState().Run7Vlc;
    }
}

// Scene members declared by Formats/Movie.cs (public partial class Scene).
namespace MphRead
{
    namespace
    {
        namespace GL = ::OpenTK::Graphics::OpenGL::GL;
#if defined(__ANDROID__)
        using AL = ::OpenTK::Audio::OpenAL::AL;
#else
        namespace AL = ::OpenTK::Audio::OpenAL::AL;
#endif
        using ::OpenTK::Audio::OpenAL::ALFormat;
        using ::OpenTK::Audio::OpenAL::ALGetSourcei;
        using ::OpenTK::Audio::OpenAL::ALSourcef;
        using ::OpenTK::Audio::OpenAL::ALSourceState;
        using ::MphRead::Formats::VxDecoder;
        using ::MphRead::Formats::MovieNativeRuntime::MovieTask;

        constexpr std::int32_t Int32MaxValue = std::numeric_limits<std::int32_t>::max();

#if defined(DEBUG)
#define MPH_MOVIE_DEBUG_ASSERT(condition) ::MphRead::NativeRuntime::DebugAssert(condition)
#else
#define MPH_MOVIE_DEBUG_ASSERT(condition) do { } while (false)
#endif

        [[nodiscard]] std::uint8_t* BufferData(
            const std::shared_ptr<Formats::ClrArray<std::uint8_t>>& buffer)
        {
            if (!buffer)
            {
                throw System::NullReferenceException();
            }
            return buffer->Length() == 0 ? nullptr : &(*buffer)[0];
        }
    }

    void Scene::StartMovies(Movie movieId, Movie afterMovieId, MphRead::FadeType fadeToMovieType,
        float fadeToMovieLength, MphRead::FadeType fadeFromMovieType, float fadeFromMovieLength,
        AfterMovie afterMovieAction)
    {
        StartMovie(movieId, fadeToMovieType, fadeToMovieLength, fadeFromMovieType, fadeFromMovieLength,
            std::nullopt, std::nullopt, afterMovieId, afterMovieAction);
    }

    void Scene::StartMovie(Movie movieId, MphRead::FadeType fadeToMovieType, float fadeToMovieLength,
        MphRead::FadeType fadeFromMovieType, float fadeFromMovieLength,
        std::optional<OpenTK::Mathematics::Vector3> afterPosition,
        std::optional<OpenTK::Mathematics::Vector3> afterFacing, std::optional<Movie> afterMovieId,
        AfterMovie afterMovieAction)
    {
        _movieSettings.MovieId = movieId;
        _movieSettings.AfterMovieId = afterMovieId;
        _movieSettings.AfterFadeType = fadeFromMovieType;
        _movieSettings.AfterFadeLength = fadeFromMovieLength;
        _movieSettings.AfterPosition = afterPosition;
        _movieSettings.AfterFacing = afterFacing;
        _movieSettings.AfterMovieAction = afterMovieAction;
        if (GameState::MatchState() == MatchState::InProgress)
        {
            const std::shared_ptr<Entities::PlayerEntity> main = Entities::PlayerEntity::Main();
            if (!main)
            {
                throw System::NullReferenceException();
            }
            if (main->Health() > 0)
            {
                GameState::PausePrevented(true);
            }
        }
        SetFade(fadeToMovieType, fadeToMovieLength, true, AfterFade::PlayMovie);
    }

    // StartMovie(..., afterMovieAction: value) with the other optional arguments left out.
    void Scene::StartMovie(Movie movieId, MphRead::FadeType fadeToMovieType, float fadeToMovieLength,
        MphRead::FadeType fadeFromMovieType, float fadeFromMovieLength, AfterMovie afterMovieAction)
    {
        StartMovie(movieId, fadeToMovieType, fadeToMovieLength, fadeFromMovieType, fadeFromMovieLength,
            std::nullopt, std::nullopt, std::nullopt, afterMovieAction);
    }

    bool Scene::MoviePlaying() const noexcept
    {
        return _movieFrameIndex != -1;
    }

    void Scene::PlayMovie(Movie movieId)
    {
        Music::Stop();
        GameState::PauseDialog();
        Sound::Sfx::SfxMute = true;
        Sound::Sfx::LongSfxMute++;
        Sound::Sfx::TimedSfxMute++;
        Sound::Sfx::ForceFieldSfxMute++;
        VxDecoder::Instance1().Reset();
        VxDecoder::Instance2().Reset();
        _decoderCts.store(std::make_shared<std::stop_source>());
        if (_audioHandle != -1)
        {
            AL::SourceStop(_audioHandle);
            AL::DeleteSource(_audioHandle);
            AL::DeleteBuffers(_audioBufferIds);
        }
        _audioHandle = AL::GenSource();
        AL::GenBuffers(_audioBufferIds);
        AL::Source(_audioHandle, ALSourcef::Gain, Music::UserVolume() * 0.5F);
        std::fill(_audioBuffersAvailable.begin(), _audioBuffersAvailable.end(), true);
        _audioBufferIndex = 0;
        const std::vector<std::shared_ptr<Metadata::MovieInfo>>& movieFiles = Metadata::MovieFiles;
        const std::size_t movieIndex = static_cast<std::size_t>(static_cast<std::int32_t>(movieId));
        if (movieIndex >= movieFiles.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        const std::shared_ptr<Metadata::MovieInfo>& info = movieFiles[movieIndex];
        if (!info)
        {
            throw System::NullReferenceException();
        }
        _dualScreenMovie = info->BottomScreenPath.has_value();
        const std::string topPath = Paths::Combine(Paths::FileSystem(), info->TopScreenPath.value_or(std::string()));
        {
            const std::stop_token token = _decoderCts.load()->get_token();
            NativeRuntime::TaskRun([topPath, token]()
            {
                (void)VxDecoder::Instance1().Decode(topPath, false, token);
            }, token);
        }
        if (_dualScreenMovie)
        {
            const std::string botPath = Paths::Combine(Paths::FileSystem(), *info->BottomScreenPath);
            const std::stop_token token = _decoderCts.load()->get_token();
            NativeRuntime::TaskRun([botPath, token]()
            {
                (void)VxDecoder::Instance2().Decode(botPath, false, token);
            }, token);
        }
        else
        {
            std::fill_n(BufferData(_botImageBuffer), _botImageBuffer->Length(), std::uint8_t{0});
        }
        while (VxDecoder::Instance1().FramesQueued() < 4)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        (void)VxDecoder::Instance1().GetImage(0, _topImageBuffer);
        if (_dualScreenMovie)
        {
            while (VxDecoder::Instance2().FramesQueued() < 4)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            (void)VxDecoder::Instance2().GetImage(0, _botImageBuffer);
        }
        if (_topMovieBinding == -1)
        {
            _topMovieBinding = ++_textureCount;
            _botMovieBinding = ++_textureCount;
        }
        GL::BindTexture(GL::TextureTarget::Texture2D, _topMovieBinding);
        GL::TexImage2D(GL::TextureTarget::Texture2D, 0, GL::PixelInternalFormat::Rgb, _frameWidth, _frameHeight, 0,
            GL::PixelFormat::Rgb, GL::PixelType::UnsignedByte, BufferData(_topImageBuffer));
        GL::BindTexture(GL::TextureTarget::Texture2D, _botMovieBinding);
        GL::TexImage2D(GL::TextureTarget::Texture2D, 0, GL::PixelInternalFormat::Rgb, _frameWidth, _frameHeight, 0,
            GL::PixelFormat::Rgb, GL::PixelType::UnsignedByte, BufferData(_botImageBuffer));
        GL::BindTexture(GL::TextureTarget::Texture2D, 0);
        MPH_MOVIE_DEBUG_ASSERT(!_dualScreenMovie
            || VxDecoder::Instance1().FrameCount == VxDecoder::Instance2().FrameCount);
        _movieFrameTotal = VxDecoder::Instance1().FrameCount;
        _lastRenderedMovieFrameIndex = 0;
        _movieFrameIndex = 0;
        _movieFrameCount = 0;
        _skipMovie = false;
        {
            const std::stop_token token = _decoderCts.load()->get_token();
            NativeRuntime::TaskRun([this]()
            {
                // The lambda reads _decoderCts afresh on every pass, as the C# closure does.
                [](Scene* scene) -> MovieTask
                {
                    while (!scene->_decoderCts.load()->get_token().stop_requested()
                        && scene->_movieFrameCount != Int32MaxValue)
                    {
                        co_await scene->UpdateMovieAudio(scene->_decoderCts.load()->get_token());
                    }
                }(this);
            }, token);
        }
        {
            const std::stop_token token = _decoderCts.load()->get_token();
            NativeRuntime::TaskRun([this]()
            {
                (void)UpdateMovieImage(_decoderCts.load()->get_token());
            }, token);
        }
    }

    std::int32_t Scene::MovieAudioHandle() const noexcept
    {
        return _audioHandle;
    }

    Formats::MovieNativeRuntime::MovieTask Scene::UpdateMovieAudio(std::stop_token token)
    {
        (void)token;
        std::array<std::int16_t, 128 * 2> stereoBuffer{};
        const std::int32_t framesAvailable = VxDecoder::Instance1().AudioFrameTotal() - _audioBufferIndex;
        if (framesAvailable > 0)
        {
            std::int32_t buffersAvailable = 0;
            if (_audioBufferIndex == 0)
            {
                buffersAvailable = _audioBufferCount;
            }
            else
            {
                const std::int32_t buffersProcessed = AL::GetSource(_audioHandle, ALGetSourcei::BuffersProcessed);
                if (buffersProcessed > 0)
                {
                    std::vector<std::int32_t> processedIds(
                        static_cast<std::size_t>(std::min(framesAvailable, buffersProcessed)));
                    AL::SourceUnqueueBuffers(_audioHandle, processedIds);
                    for (std::int32_t i = 0; i < _audioBufferCount; i++)
                    {
                        _audioBuffersAvailable[static_cast<std::size_t>(i)] = std::find(processedIds.begin(),
                            processedIds.end(), _audioBufferIds[static_cast<std::size_t>(i)]) != processedIds.end();
                    }
                }
                for (std::int32_t i = 0; i < _audioBufferCount; i++)
                {
                    if (_audioBuffersAvailable[static_cast<std::size_t>(i)])
                    {
                        buffersAvailable++;
                    }
                }
            }
            if (buffersAvailable > 0)
            {
                std::vector<std::int32_t> queueBuffers(
                    static_cast<std::size_t>(std::min(framesAvailable, buffersAvailable)));
                std::int32_t bufferIndex = 0;
                for (std::size_t i = 0; i < queueBuffers.size(); i++)
                {
                    for (; bufferIndex < _audioBufferCount; bufferIndex++)
                    {
                        if (_audioBuffersAvailable[static_cast<std::size_t>(bufferIndex)])
                        {
                            queueBuffers[i] = _audioBufferIds[static_cast<std::size_t>(bufferIndex)];
                            _audioBuffersAvailable[static_cast<std::size_t>(bufferIndex)] = false;
                            break;
                        }
                    }
                    MPH_MOVIE_DEBUG_ASSERT(bufferIndex < _audioBufferCount);
                    if (_dualScreenMovie)
                    {
                        const std::span<const std::int16_t> buffer1
                            = VxDecoder::Instance1().GetAudioBuffer(_audioBufferIndex);
                        const std::span<const std::int16_t> buffer2
                            = VxDecoder::Instance2().GetAudioBuffer(_audioBufferIndex++);
                        for (std::size_t j = 0; j < 128; j++)
                        {
                            stereoBuffer[j * 2] = buffer1[j];
                            stereoBuffer[j * 2 + 1] = buffer2[j];
                        }
                        AL::BufferData(queueBuffers[i], ALFormat::Stereo16,
                            std::span<const std::int16_t>(stereoBuffer), VxDecoder::Instance1().AudioSampleRate);
                    }
                    else
                    {
                        const std::span<const std::int16_t> buffer
                            = VxDecoder::Instance1().GetAudioBuffer(_audioBufferIndex++);
                        AL::BufferData(queueBuffers[i], ALFormat::Mono16, buffer,
                            VxDecoder::Instance1().AudioSampleRate);
                    }
                }
                AL::SourceQueueBuffers(_audioHandle, queueBuffers);
                const auto state = static_cast<ALSourceState>(AL::GetSource(_audioHandle, ALGetSourcei::SourceState));
                if (state != ALSourceState::Playing)
                {
                    AL::SourcePlay(_audioHandle);
                }
            }
        }
        co_await MovieTask::DelayOneMillisecond();
    }

    void Scene::StopMovie()
    {
        if (_audioHandle != -1)
        {
            AL::SourceStop(_audioHandle);
            AL::DeleteSource(_audioHandle);
            AL::DeleteBuffers(_audioBufferIds);
            _audioHandle = -1;
        }
        if (_movieSettings.AfterMovieId.has_value())
        {
            Sound::Sfx::LongSfxMute--;
            Sound::Sfx::TimedSfxMute--;
            Sound::Sfx::ForceFieldSfxMute--;
            if (const std::shared_ptr<std::stop_source> decoderCts = _decoderCts.load())
            {
                decoderCts->request_stop();
            }
            _movieSettings.MovieId = _movieSettings.AfterMovieId.value();
            _movieSettings.AfterMovieId.reset();
            PlayMovie(_movieSettings.MovieId);
            _fadeType = MphRead::FadeType::None;
        }
        else if (_movieSettings.AfterMovieAction == AfterMovie::EndGame)
        {
            QuitGame(true);
        }
        else
        {
            if (_movieSettings.AfterMovieAction != AfterMovie::StartGame)
            {
                MPH_MOVIE_DEBUG_ASSERT(_room != nullptr);
                if (!_room)
                {
                    throw System::NullReferenceException();
                }
                _room->LoadRoom(GameState::TransitionRoomId() == -1);
            }
            else
            {
                SetFade(MphRead::FadeType::FadeInWhite, 5.0F / 30.0F, true, AfterFade::None, 5.0F / 30.0F);
                Music::PlayPausedMusic();
                _playingLandingMovie = false;
            }
            Sound::Sfx::SfxMute = false;
            Sound::Sfx::LongSfxMute--;
            Sound::Sfx::TimedSfxMute--;
            Sound::Sfx::ForceFieldSfxMute--;
            GameState::UnpauseDialog();
            if (const std::shared_ptr<std::stop_source> decoderCts = _decoderCts.load())
            {
                decoderCts->request_stop();
            }
            _movieFrameIndex = -1;
        }
    }

    void Scene::SkipMovie()
    {
        if (_movieFrameIndex != -1 && _movieFrameCount != Int32MaxValue)
        {
            _skipMovie = true;
        }
    }

    void Scene::UpdateMovie()
    {
        if (_skipMovie)
        {
            if (_movieFrameCount != Int32MaxValue)
            {
                _movieFrameCount = Int32MaxValue;
                SetFade(_movieSettings.AfterFadeType, _movieSettings.AfterFadeLength, true, AfterFade::StopMovie);
            }
            return;
        }
    }

    Formats::MovieNativeRuntime::MovieTask Scene::UpdateMovieImage(std::stop_token token)
    {
        using Duration = std::chrono::steady_clock::duration;
        const Duration frameTime = std::chrono::duration_cast<Duration>(std::chrono::duration<double>(1.0 / 15.0));
        Duration nextFrameElapsed = frameTime;
        const Duration tolerance = std::chrono::milliseconds(15);
        const auto start = std::chrono::steady_clock::now();
        while (!token.stop_requested() && _movieFrameCount != Int32MaxValue)
        {
            const Duration elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= nextFrameElapsed && nextFrameElapsed - elapsed < tolerance)
            {
                _movieFrameCount = _movieFrameCount + 1;
                nextFrameElapsed += frameTime;
            }
            const std::int32_t frameIndex = _movieFrameCount;
            if (_movieFrameCount != _movieFrameIndex)
            {
                if (frameIndex == _movieFrameTotal)
                {
                    _movieFrameCount = Int32MaxValue;
                    SetFade(_movieSettings.AfterFadeType, _movieSettings.AfterFadeLength, true, AfterFade::StopMovie);
                    co_return;
                }
                if (frameIndex < _movieFrameTotal)
                {
                    // C# precedence: (a && !b) || c
                    if ((VxDecoder::Instance1().GetImage(frameIndex, _topImageBuffer) && !_dualScreenMovie)
                        || VxDecoder::Instance2().GetImage(frameIndex, _botImageBuffer))
                    {
                        _movieFrameIndex = frameIndex;
                    }
                }
            }
            co_await MovieTask::DelayOneMillisecond();
        }
    }

    void Scene::DrawMovieFrame()
    {
        GL::Uniform1(_shaderLocations->LayerAlpha, 1);
        bool newFrame = false;

        const auto drawScreen = [this, &newFrame](std::int32_t movieBinding,
            const std::shared_ptr<Formats::ClrArray<std::uint8_t>>& imageBuffer, float y)
        {
            GL::BindTexture(GL::TextureTarget::Texture2D, movieBinding);
            const std::int32_t minParameter = static_cast<std::int32_t>(GL::TextureMinFilter::Nearest);
            const std::int32_t magParameter = static_cast<std::int32_t>(GL::TextureMagFilter::Nearest);
            GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureMinFilter, minParameter);
            GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureMagFilter, magParameter);
            GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureWrapS,
                static_cast<std::int32_t>(GL::TextureWrapMode::ClampToEdge));
            GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureWrapT,
                static_cast<std::int32_t>(GL::TextureWrapMode::ClampToEdge));
            if (_movieFrameIndex != _lastRenderedMovieFrameIndex || (newFrame && _dualScreenMovie))
            {
                newFrame = true;
                _lastRenderedMovieFrameIndex = _movieFrameIndex;
                GL::TexSubImage2D(GL::TextureTarget::Texture2D, 0, 0, 0, _frameWidth, _frameHeight,
                    GL::PixelFormat::Rgb, GL::PixelType::UnsignedByte, BufferData(imageBuffer));
            }
            GL::Begin(GL::PrimitiveType::TriangleStrip);
            GL::TexCoord3(1.0F, 0.0F, 0.0F);
            GL::Vertex3(0.5F, y, 0.0F);
            GL::TexCoord3(0.0F, 0.0F, 0.0F);
            GL::Vertex3(-0.5F, y, 0.0F);
            GL::TexCoord3(1.0F, 1.0F, 0.0F);
            GL::Vertex3(0.5F, y - 1, 0.0F);
            GL::TexCoord3(0.0F, 1.0F, 0.0F);
            GL::Vertex3(-0.5F, y - 1, 0.0F);
            GL::End();
            GL::BindTexture(GL::TextureTarget::Texture2D, 0);
        };

        // GL.Uniform4(FadeColor, 0, 0, 0, 1) in C#: the int overload, glUniform4i.
        // fade_color is a vec4, so the call is rejected and the fade colour set
        // earlier in the frame (alpha 0) stays; sending 1.0F instead paints the
        // whole movie with the rtt shader's solid fade colour.
        GL::Uniform4(_shaderLocations->FadeColor, 0, 0, 0, 1);
        GL::Begin(GL::PrimitiveType::TriangleStrip);
        GL::TexCoord3(1.0F, 1.0F, 0.0F);
        GL::Vertex3(1.0F, 1.0F, 0.0F);
        GL::TexCoord3(0.0F, 1.0F, 0.0F);
        GL::Vertex3(-1.0F, 1.0F, 0.0F);
        GL::TexCoord3(1.0F, 0.0F, 0.0F);
        GL::Vertex3(1.0F, -1.0F, 0.0F);
        GL::TexCoord3(0.0F, 0.0F, 0.0F);
        GL::Vertex3(-1.0F, -1.0F, 0.0F);
        GL::End();
        drawScreen(_topMovieBinding, _topImageBuffer, 1.0F);
        drawScreen(_botMovieBinding, _botImageBuffer, 0.0F);
    }
}
