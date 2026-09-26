#pragma once

#include "../../../NativeRuntime/System/Numerics.hpp"

#include <cstdint>

namespace MphRead::Mods::Input::AimAssist
{
    enum class AimAssistPointType : std::int32_t { CenterMass, UpperChest, Head };

    // AimAssistPointType.ToString().
    [[nodiscard]] const char* ToString(AimAssistPointType value) noexcept;

    struct AimAssistTarget
    {
        std::int32_t Slot = 0;
        std::int64_t Life = 0;
        System::Numerics::Vector2 BodyError{};
        System::Numerics::Vector2 HeadError{};
        float Distance = 0;
        bool BodyVisible = false;
        bool HeadVisible = false;
        bool Eligible = true;
        AimAssistPointType BodyPointType = AimAssistPointType::UpperChest;

        friend bool operator==(const AimAssistTarget&, const AimAssistTarget&) = default;
    };

    struct AimAssistResult
    {
        float X = 0;
        float Y = 0;
        std::int32_t TargetSlot = -1;
        float Friction = 1;
        float RotationStrength = 0;
        AimAssistPointType PointType = AimAssistPointType::UpperChest;
        float HeadBlend = 0;
        float Score = 0;

        friend bool operator==(const AimAssistResult&, const AimAssistResult&) = default;
    };
}
