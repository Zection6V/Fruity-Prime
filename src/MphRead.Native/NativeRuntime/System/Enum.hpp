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

    // Enum.TryParse(text, ignoreCase, out result): leading white space off;
    // then a number when it starts with a digit or a sign (in range for the
    // underlying type, which is `bits` wide), or else one or more names
    // separated by commas, each trimmed, whose values are or'ed together.
    // `raw` is the underlying value's bits, zero on failure.
    [[nodiscard]] bool ManagedEnumTryParse(
        std::string_view text,
        bool ignoreCase,
        const EnumNameEntry* names,
        std::size_t count,
        bool isSigned,
        std::int32_t bits,
        std::uint64_t& raw);
    // The same for an int-backed enum.
    [[nodiscard]] inline bool ManagedEnumTryParse(
        std::string_view text,
        bool ignoreCase,
        const EnumNameEntry* names,
        std::size_t count,
        std::uint64_t& raw)
    {
        return ManagedEnumTryParse(text, ignoreCase, names, count, true, 32, raw);
    }

    template <typename T>
    [[nodiscard]] bool ManagedEnumTryParse(
        std::string_view text, bool ignoreCase, const EnumNameEntry* names, std::size_t count, T& result)
    {
        using Underlying = std::underlying_type_t<T>;
        std::uint64_t raw = 0;
        const bool ok = ManagedEnumTryParse(text, ignoreCase, names, count,
            std::is_signed_v<Underlying>, static_cast<std::int32_t>(sizeof(Underlying) * 8), raw);
        result = static_cast<T>(static_cast<Underlying>(raw));
        return ok;
    }

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
