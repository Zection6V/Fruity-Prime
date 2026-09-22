#pragma once

// System.Enum.ToString() as the runtime performs it. The per-enum name tables
// are not here: each one belongs beside the enum it names, in the translation
// unit matching the .cs file that declares it. What lives here is only the
// algorithm every one of them shares.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace MphRead::NativeRuntime
{
    struct EnumNameEntry
    {
        std::uint64_t Value;
        const char* Name;
    };

    // Enum.ToString(): a defined name when one matches exactly; for [Flags]
    // enums, the names found walking from the largest value down, printed in
    // ascending order and joined with ", " (zero only by its own name);
    // otherwise the number.
    [[nodiscard]] std::string ManagedEnumToString(
        std::uint64_t raw,
        std::int64_t signedValue,
        bool isSigned,
        bool isFlags,
        const EnumNameEntry* names,
        std::size_t count);

    // Enum.TryParse(text, ignoreCase, out result) for a non-[Flags] enum: a
    // name match, or the decimal digits of the underlying type.
    [[nodiscard]] bool ManagedEnumTryParse(
        std::string_view text,
        bool ignoreCase,
        const EnumNameEntry* names,
        std::size_t count,
        std::uint64_t& raw);

    template <typename T>
    [[nodiscard]] std::string ManagedEnumToString(
        T value, const EnumNameEntry* names, std::size_t count, bool isFlags)
    {
        using Underlying = std::underlying_type_t<T>;
        using Unsigned = std::make_unsigned_t<Underlying>;
        return ManagedEnumToString(
            static_cast<std::uint64_t>(static_cast<Unsigned>(value)),
            static_cast<std::int64_t>(static_cast<Underlying>(value)),
            std::is_signed_v<Underlying>,
            isFlags,
            names,
            count);
    }
}
