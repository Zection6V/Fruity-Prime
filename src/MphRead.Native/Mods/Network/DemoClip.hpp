#pragma once

#include "DemoFile.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Network
{
    // C# internal static class DemoClip.
    class DemoClip final
    {
    public:
        DemoClip() = delete;
        DemoClip(const DemoClip&) = delete;
        DemoClip(DemoClip&&) = delete;
        DemoClip& operator=(const DemoClip&) = delete;
        DemoClip& operator=(DemoClip&&) = delete;

        // C# static readonly array: the array object cannot be replaced, while
        // its elements remain mutable.
        static std::int32_t (&Lengths)[3];

        [[nodiscard]] static std::int32_t Seconds() noexcept;
        static void Seconds(std::int32_t value) noexcept;

        [[nodiscard]] static bool Active();
        [[nodiscard]] static double Held();

        static void Add(std::span<const std::uint8_t> data);
        static void Purge();
        [[nodiscard]] static std::optional<std::string> Save();

    private:
        static constexpr std::uint32_t Slack = 30;
        static constexpr std::int64_t MaxBytes = 24LL * 1024LL * 1024LL;

        static void Trim();

        static std::int32_t _lengths[3];
        static std::int32_t _seconds;
        static std::deque<DemoRecord> _records;
        static std::int64_t _bytes;
    };
}
