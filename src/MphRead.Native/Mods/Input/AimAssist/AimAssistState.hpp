#pragma once

#include "../../../NativeRuntime/System/Numerics.hpp"

#include <cstdint>

namespace MphRead::Mods::Input::AimAssist
{
    class AimAssistState final
    {
    public:
        std::int32_t TargetSlot = -1;
        std::int64_t TargetLife = 0;
        float RetainedSeconds = 0;
        float HeadBlend = 0;
        System::Numerics::Vector2 PreviousError{};
        System::Numerics::Vector2 PreviousOutput{};
        System::Numerics::Vector2 AngularVelocity{};

        void Reset() noexcept
        {
            TargetSlot = -1;
            TargetLife = 0;
            RetainedSeconds = HeadBlend = 0;
            PreviousError = PreviousOutput = AngularVelocity = {};
        }
    };
}
