#pragma once

#include "AimAssistState.hpp"
#include "AimAssistTarget.hpp"
#include "AimAssistTuning.hpp"

#include <span>

namespace MphRead::Mods::Input::AimAssist
{
    class AimAssist final
    {
    public:
        AimAssist() = delete;

        [[nodiscard]] static AimAssistResult Apply(AimAssistState& state, std::span<const AimAssistTarget> targets,
            System::Numerics::Vector2 raw, float stickIntent, float moveIntent, float dt, bool eligible,
            const AimAssistWeaponProfile& profile);
    };
}
