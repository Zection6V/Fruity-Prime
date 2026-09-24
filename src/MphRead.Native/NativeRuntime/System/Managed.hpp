#pragma once

// What a C# member access does to a null reference: throw
// NullReferenceException. The port calls RequireReference(p).Member where the
// C# wrote p.Member, for a raw pointer or a shared_ptr alike.

#include "Exceptions.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
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
}
