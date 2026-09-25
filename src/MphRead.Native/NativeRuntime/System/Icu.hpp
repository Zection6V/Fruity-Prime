#pragma once

// The ICU that .NET's globalization layer loads, and the switches that decide
// whether it is used at all. .NET reaches ICU through libSystem.Globalization.Native;
// here its functions are looked up directly, once, by their unversioned names.

#include <string_view>

namespace MphRead::NativeRuntime::Icu
{
    // DOTNET_SYSTEM_GLOBALIZATION_INVARIANT: the invariant culture everywhere,
    // and no ICU.
    [[nodiscard]] bool InvariantMode() noexcept;

#if defined(_WIN32)
    // DOTNET_SYSTEM_GLOBALIZATION_USENLS: Windows' own NLS instead of ICU.
    [[nodiscard]] bool UseNlsRequested() noexcept;
#endif

    // An ICU function by its C name ("u_toupper", "ucol_open"): the plain
    // symbol, or the one ICU renames with its major version ("u_toupper_74").
    // nullptr when there is no ICU, invariant mode is on, or it lacks the
    // function. Libraries are opened once and never closed.
    [[nodiscard]] void* Function(std::string_view name) noexcept;

    template <class TFunction>
    [[nodiscard]] TFunction Function(std::string_view name) noexcept
    {
        return reinterpret_cast<TFunction>(Function(name));
    }

    // Whether any ICU library was found.
    [[nodiscard]] bool Available() noexcept;
}
