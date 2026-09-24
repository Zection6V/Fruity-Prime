#pragma once

// What a C# member access does to a null reference: throw
// NullReferenceException. The port calls RequireReference(p).Member where the
// C# wrote p.Member, for a raw pointer or a shared_ptr alike.

#include "Exceptions.hpp"

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
}
