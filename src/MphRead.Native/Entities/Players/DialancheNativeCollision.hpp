#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"
#include <cstdint>

namespace MphRead::Entities
{
    // EU1.1 0200CCC0 records the rocks for the next 0200B55C gameplay tick.
    // Fixed two-generation history makes all targets independent of player
    // processing order. The visual pose may continue moving at 60 Hz.
    class DialancheNativeCollision final
    {
    public:
        struct Pose
        {
            OpenTK::Mathematics::Vector3 Left{};
            OpenTK::Mathematics::Vector3 Right{};
        };

        static constexpr float RockRadius = 0.5F;
        void Reset(OpenTK::Mathematics::Vector3 position) noexcept;
        void Record(std::uint64_t nativeTick, OpenTK::Mathematics::Vector3 left,
            OpenTK::Mathematics::Vector3 right) noexcept;
        [[nodiscard]] Pose PoseForHit(std::uint64_t nativeTick) const noexcept;
        [[nodiscard]] static constexpr bool IsNativeCollisionStep(std::uint64_t frame) noexcept
        {
            return frame != 0 && (frame & 1U) == 0;
        }
        [[nodiscard]] static constexpr std::uint64_t NativeTick(std::uint64_t frame) noexcept { return frame / 2; }

    private:
        struct Sample
        {
            bool Valid = false;
            std::uint64_t Tick = 0;
            Pose Value{};
        };
        Pose _initial{};
        Sample _previous{};
        Sample _latest{};
    };
}
