#pragma once

#include <array>
#include <cstdint>

namespace MphRead::Mods::Input
{
    // A whip of the mouse read as a boost: a straight burst ending on this
    // frame that would have spun the player a third of a turn on foot.
    class MouseFlick final
    {
    public:
        MouseFlick() = delete;

        [[nodiscard]] static std::int32_t Fired() noexcept { return _fired; }
        static void Reset() noexcept;
        [[nodiscard]] static bool Check(float deltaX, float deltaY, std::uint64_t frame, float& dirX, float& dirY);

    private:
        static void ClearSamples() noexcept;

        static constexpr std::int32_t Burst = 5;
        static constexpr float TurnDegrees = 120;
        static constexpr float RestDegrees = 3;
        static constexpr float Coherence = 0.86F;
        static constexpr std::int32_t Cooldown = 21;

        inline static std::array<float, Burst> _deltaX{};
        inline static std::array<float, Burst> _deltaY{};
        inline static std::int32_t _count = 0;
        inline static std::int32_t _newest = -1;
        inline static std::uint64_t _lastFrame = 0;
        inline static bool _fed = false;
        inline static bool _armed = false;
        inline static std::uint64_t _cooldownUntil = 0;
        inline static std::int32_t _fired = 0;
    };
}
