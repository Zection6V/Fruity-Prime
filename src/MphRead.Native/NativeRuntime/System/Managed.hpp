#pragma once

// What a C# member access does to a null reference: throw
// NullReferenceException. The port calls RequireReference(p).Member where the
// C# wrote p.Member, for a raw pointer or a shared_ptr alike.

#include "Exceptions.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <type_traits>

namespace MphRead::NativeRuntime
{
    namespace Detail
    {
        template <typename T>
        struct IsSharedPtr : std::false_type
        {
        };

        template <typename T>
        struct IsSharedPtr<std::shared_ptr<T>> : std::true_type
        {
        };
    }

    template <typename T>
    [[nodiscard]] T& RequireReference(T* value)
    {
        if (value == nullptr)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename T>
    [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    // Already a reference: nothing to check.
    template <typename T>
    requires (!std::is_pointer_v<T> && !Detail::IsSharedPtr<std::remove_cv_t<T>>::value)
    [[nodiscard]] T& RequireReference(T& value) noexcept
    {
        return value;
    }

    // Math.Round(double) and MathF.Round(float): to the nearest integer, a
    // tie to the even one, the sign of a zero kept. That is IEEE
    // round-to-nearest, which std::nearbyint does under the default rounding
    // mode -- and nothing in the program changes it. The per-file copies
    // this replaces floored and patched, and turned -0.4 into +0.
    [[nodiscard]] inline float RoundToEven(float value) noexcept
    {
        return std::nearbyint(value);
    }

    [[nodiscard]] inline double RoundToEven(double value) noexcept
    {
        return std::nearbyint(value);
    }

    // (int)value for a float, as .NET 9 does it: toward zero, NaN is 0, and
    // anything out of range saturates. Before .NET 9 the x64 JIT gave
    // int.MinValue for all three, which is what four of the per-file copies
    // this replaces still did.
    [[nodiscard]] inline std::int32_t ConvertToInt32Net9(float value) noexcept
    {
        if (std::isnan(value))
        {
            return 0;
        }
        const double wide = static_cast<double>(value);
        if (wide < static_cast<double>(std::numeric_limits<std::int32_t>::min()))
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        if (wide > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        {
            return std::numeric_limits<std::int32_t>::max();
        }
        return static_cast<std::int32_t>(std::trunc(wide));
    }

    namespace Detail
    {
        template <typename T>
        [[nodiscard]] T FloatMax(T val1, T val2) noexcept
        {
            if (val1 != val2)
            {
                if (!std::isnan(val1))
                {
                    return val2 < val1 ? val1 : val2;
                }
                return val1;
            }
            return std::signbit(val2) ? val1 : val2;
        }

        template <typename T>
        [[nodiscard]] T FloatMin(T val1, T val2) noexcept
        {
            if (val1 != val2)
            {
                if (!std::isnan(val1))
                {
                    return val1 < val2 ? val1 : val2;
                }
                return val1;
            }
            return std::signbit(val1) ? val1 : val2;
        }
    }

    // Math.Max / MathF.Max (.NET Core 3.0 and later): a NaN on either side
    // wins, and +0 is greater than -0. std::max and std::fmax do neither.
    // Math.Min / MathF.Min likewise, -0 less than +0. One overload per C#
    // overload, so a mixed call converts the way the C# one does.
    [[nodiscard]] inline float MathMax(float val1, float val2) noexcept { return Detail::FloatMax(val1, val2); }
    [[nodiscard]] inline double MathMax(double val1, double val2) noexcept { return Detail::FloatMax(val1, val2); }
    [[nodiscard]] inline float MathMin(float val1, float val2) noexcept { return Detail::FloatMin(val1, val2); }
    [[nodiscard]] inline double MathMin(double val1, double val2) noexcept { return Detail::FloatMin(val1, val2); }
    [[nodiscard]] constexpr std::int32_t MathMax(std::int32_t val1, std::int32_t val2) noexcept { return val1 >= val2 ? val1 : val2; }
    [[nodiscard]] constexpr std::int32_t MathMin(std::int32_t val1, std::int32_t val2) noexcept { return val1 <= val2 ? val1 : val2; }
    [[nodiscard]] constexpr std::uint32_t MathMax(std::uint32_t val1, std::uint32_t val2) noexcept { return val1 >= val2 ? val1 : val2; }
    [[nodiscard]] constexpr std::uint32_t MathMin(std::uint32_t val1, std::uint32_t val2) noexcept { return val1 <= val2 ? val1 : val2; }
    [[nodiscard]] constexpr std::int64_t MathMax(std::int64_t val1, std::int64_t val2) noexcept { return val1 >= val2 ? val1 : val2; }
    [[nodiscard]] constexpr std::int64_t MathMin(std::int64_t val1, std::int64_t val2) noexcept { return val1 <= val2 ? val1 : val2; }

    namespace Detail
    {
        template <typename T>
        [[nodiscard]] T Clamp(T value, T min, T max)
        {
            if (min > max)
            {
                throw System::ArgumentException(
                    "'" + std::to_string(min) + "' cannot be greater than " + std::to_string(max) + ".");
            }
            if (value < min)
            {
                return min;
            }
            if (value > max)
            {
                return max;
            }
            return value;
        }
    }

    // Math.Clamp(value, min, max): ArgumentException when min is greater
    // than max; a NaN value comes back as it went in.
    [[nodiscard]] inline float MathClamp(float value, float min, float max) { return Detail::Clamp(value, min, max); }
    [[nodiscard]] inline double MathClamp(double value, double min, double max) { return Detail::Clamp(value, min, max); }
    [[nodiscard]] inline std::int32_t MathClamp(std::int32_t value, std::int32_t min, std::int32_t max) { return Detail::Clamp(value, min, max); }
    [[nodiscard]] inline std::uint32_t MathClamp(std::uint32_t value, std::uint32_t min, std::uint32_t max) { return Detail::Clamp(value, min, max); }

    // C# integer arithmetic outside a checked block wraps around. In C++ a
    // signed overflow is undefined, so each of these works in the unsigned
    // type of the same width and reinterprets the bits -- which is two's
    // complement wrapping, exactly what the CLR does.
    [[nodiscard]] constexpr std::int32_t UncheckedAdd(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(left) + std::bit_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int64_t UncheckedAdd(std::int64_t left, std::int64_t right) noexcept
    {
        return std::bit_cast<std::int64_t>(std::bit_cast<std::uint64_t>(left) + std::bit_cast<std::uint64_t>(right));
    }

    // long + int: the int widens first, as in C#.
    [[nodiscard]] constexpr std::int64_t UncheckedAdd(std::int64_t left, std::int32_t right) noexcept
    {
        return UncheckedAdd(left, static_cast<std::int64_t>(right));
    }

    [[nodiscard]] constexpr std::uint32_t UncheckedAdd(std::uint32_t left, std::uint32_t right) noexcept
    {
        return left + right;
    }

    [[nodiscard]] constexpr std::int32_t UncheckedSubtract(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(left) - std::bit_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int64_t UncheckedSubtract(std::int64_t left, std::int64_t right) noexcept
    {
        return std::bit_cast<std::int64_t>(std::bit_cast<std::uint64_t>(left) - std::bit_cast<std::uint64_t>(right));
    }

    [[nodiscard]] constexpr std::uint32_t UncheckedSubtract(std::uint32_t left, std::uint32_t right) noexcept
    {
        return left - right;
    }

    [[nodiscard]] constexpr std::int32_t UncheckedMultiply(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(left) * std::bit_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int64_t UncheckedMultiply(std::int64_t left, std::int64_t right) noexcept
    {
        return std::bit_cast<std::int64_t>(std::bit_cast<std::uint64_t>(left) * std::bit_cast<std::uint64_t>(right));
    }

    [[nodiscard]] constexpr std::uint32_t UncheckedMultiply(std::uint32_t left, std::uint32_t right) noexcept
    {
        return left * right;
    }

    [[nodiscard]] constexpr std::int32_t UncheckedNegate(std::int32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(0U - std::bit_cast<std::uint32_t>(value));
    }

    [[nodiscard]] constexpr std::int64_t UncheckedNegate(std::int64_t value) noexcept
    {
        return std::bit_cast<std::int64_t>(0ULL - std::bit_cast<std::uint64_t>(value));
    }

    [[nodiscard]] constexpr std::int32_t UncheckedIncrement(std::int32_t value) noexcept
    {
        return UncheckedAdd(value, std::int32_t{1});
    }

    [[nodiscard]] constexpr std::int64_t UncheckedIncrement(std::int64_t value) noexcept
    {
        return UncheckedAdd(value, std::int64_t{1});
    }

    [[nodiscard]] constexpr std::int32_t UncheckedDecrement(std::int32_t value) noexcept
    {
        return UncheckedSubtract(value, std::int32_t{1});
    }

    [[nodiscard]] constexpr std::int64_t UncheckedDecrement(std::int64_t value) noexcept
    {
        return UncheckedSubtract(value, std::int64_t{1});
    }

    // value++ / value-- as statements.
    constexpr void IncrementInPlace(std::int32_t& value) noexcept { value = UncheckedIncrement(value); }
    constexpr void IncrementInPlace(std::int64_t& value) noexcept { value = UncheckedIncrement(value); }
    constexpr void DecrementInPlace(std::int32_t& value) noexcept { value = UncheckedDecrement(value); }
    constexpr void DecrementInPlace(std::int64_t& value) noexcept { value = UncheckedDecrement(value); }

    // unchecked((int)x) and friends: the low bits, reinterpreted.
    [[nodiscard]] constexpr std::int32_t UInt32ToInt32(std::uint32_t value) noexcept { return std::bit_cast<std::int32_t>(value); }
    [[nodiscard]] constexpr std::uint32_t Int32ToUInt32(std::int32_t value) noexcept { return std::bit_cast<std::uint32_t>(value); }
    [[nodiscard]] constexpr std::int32_t Int64ToInt32(std::int64_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(value)));
    }
    [[nodiscard]] constexpr std::int32_t UInt64ToInt32(std::uint64_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
    }

    // value << count and value >> count on an int: the count is masked to
    // five bits, the left shift wraps, the right shift carries the sign.
    [[nodiscard]] constexpr std::int32_t ShiftLeft(std::int32_t value, std::int32_t count) noexcept
    {
        return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value) << (std::bit_cast<std::uint32_t>(count) & 31U));
    }

    [[nodiscard]] constexpr std::int32_t ShiftRight(std::int32_t value, std::int32_t count) noexcept
    {
        // C++20 defines >> on a negative signed value as arithmetic.
        return value >> (std::bit_cast<std::uint32_t>(count) & 31U);
    }

    // Enum.HasFlag(flag): every bit of flag set. The same test as the C#'s
    // TestFlag extension (Formats/Types.hpp), under .NET's name.
    template <typename T>
    requires std::is_enum_v<T>
    [[nodiscard]] constexpr bool HasFlag(T value, T flag) noexcept
    {
        using U = std::make_unsigned_t<std::underlying_type_t<T>>;
        const U valueBits = static_cast<U>(value);
        const U flagBits = static_cast<U>(flag);
        return (valueBits & flagBits) == flagBits;
    }

    // array[index] and list[index] from the C#: an index outside the
    // collection throws, never reads past it. ManagedAt is an array's
    // IndexOutOfRangeException, ManagedListAt a List<T>'s
    // ArgumentOutOfRangeException. The collection is anything indexable
    // with a size (a vector, std::array, span, built-in array, ManagedArray),
    // or a pointer or shared_ptr to one, which throws NullReferenceException
    // when null; the index may be any integer type, and a negative one is
    // out of range. Each file used to carry its own.
    namespace ManagedAtDetail
    {
        template <typename TContainer>
        [[nodiscard]] constexpr std::size_t Size(const TContainer& values) noexcept
        {
            if constexpr (requires { values.Length(); })
            {
                return static_cast<std::size_t>(values.Length());
            }
            else
            {
                return static_cast<std::size_t>(std::size(values));
            }
        }

        template <typename TContainer>
        concept Indexable = !Detail::IsSharedPtr<std::remove_cv_t<TContainer>>::value
            && !std::is_pointer_v<TContainer>
            && requires(TContainer& values) {
                Size(values);
                values[std::size_t{}];
            };

        template <std::integral I>
        [[nodiscard]] constexpr bool InRange(I index, std::size_t size) noexcept
        {
            if constexpr (std::is_signed_v<I>)
            {
                if (index < 0)
                {
                    return false;
                }
            }
            return static_cast<std::make_unsigned_t<I>>(index) < size;
        }

        template <typename TException, std::integral I>
        [[nodiscard]] std::size_t Check(I index, std::size_t size)
        {
            if (!InRange(index, size))
            {
                throw TException();
            }
            return static_cast<std::size_t>(index);
        }

        template <typename T>
        [[nodiscard]] T& Deref(T* value)
        {
            if (value == nullptr)
            {
                throw System::NullReferenceException();
            }
            return *value;
        }
    }

    template <typename TContainer, std::integral I>
    requires ManagedAtDetail::Indexable<TContainer>
    [[nodiscard]] decltype(auto) ManagedAt(TContainer& values, I index)
    {
        return values[ManagedAtDetail::Check<System::IndexOutOfRangeException>(
            index, ManagedAtDetail::Size(values))];
    }

    template <typename TContainer, std::integral I>
    requires ManagedAtDetail::Indexable<TContainer>
    [[nodiscard]] decltype(auto) ManagedAt(TContainer* values, I index)
    {
        return ManagedAt(ManagedAtDetail::Deref(values), index);
    }

    template <typename TContainer, std::integral I>
    [[nodiscard]] decltype(auto) ManagedAt(const std::shared_ptr<TContainer>& values, I index)
    {
        return ManagedAt(ManagedAtDetail::Deref(values.get()), index);
    }

    template <typename TContainer, std::integral I>
    requires ManagedAtDetail::Indexable<TContainer>
    [[nodiscard]] decltype(auto) ManagedListAt(TContainer& values, I index)
    {
        return values[ManagedAtDetail::Check<System::ArgumentOutOfRangeException>(
            index, ManagedAtDetail::Size(values))];
    }

    template <typename TContainer, std::integral I>
    requires ManagedAtDetail::Indexable<TContainer>
    [[nodiscard]] decltype(auto) ManagedListAt(TContainer* values, I index)
    {
        return ManagedListAt(ManagedAtDetail::Deref(values), index);
    }

    template <typename TContainer, std::integral I>
    [[nodiscard]] decltype(auto) ManagedListAt(const std::shared_ptr<TContainer>& values, I index)
    {
        return ManagedListAt(ManagedAtDetail::Deref(values.get()), index);
    }
}
