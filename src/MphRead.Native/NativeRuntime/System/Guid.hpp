#pragma once

// System.Guid: sixteen bytes, laid out the way .NET lays them out -- the first
// three fields little-endian -- so a Guid written with TryWriteBytes on one
// machine reads back equal on another.

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace MphRead::NativeRuntime
{
    class Guid final
    {
    public:
        // Guid.Empty.
        constexpr Guid() noexcept = default;
        // new Guid(ReadOnlySpan<byte>): exactly sixteen bytes, or
        // System::ArgumentException.
        explicit Guid(std::span<const std::uint8_t> bytes);

        // Guid.NewGuid(): a version 4 identifier from the OS's random source.
        [[nodiscard]] static Guid NewGuid();
        [[nodiscard]] static constexpr Guid Empty() noexcept { return Guid(); }

        // TryWriteBytes(destination): false when there are fewer than sixteen.
        [[nodiscard]] bool TryWriteBytes(std::span<std::uint8_t> destination) const noexcept;
        // ToString() is "D"; "N" drops the hyphens.
        [[nodiscard]] std::string ToString(std::string_view format = "D") const;

        [[nodiscard]] friend bool operator==(const Guid& left, const Guid& right) noexcept = default;

    private:
        std::array<std::uint8_t, 16> _bytes{};
    };
}
