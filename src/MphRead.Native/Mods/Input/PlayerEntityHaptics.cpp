#include "PlayerEntityHaptics.hpp"

#include "AimAssist/AimAssistTelemetry.hpp"
#include "GamepadInput.hpp"
#include "GamepadUiRouter.hpp"
#include "WeaponSelectionDirection.hpp"
#include "../SpectatorMode.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Hud/HudInfo.hpp"

namespace MphRead::Entities
{
    void PlayerEntity::ModControllerFeedback(Mods::Input::GamepadFeedback feedback)
    {
        if (IsMainPlayer() && !_isBot && !Mods::SpectatorMode::IsSpectating()
            && (feedback == Mods::Input::GamepadFeedback::Fire || feedback == Mods::Input::GamepadFeedback::ChargedShot))
        {
            Mods::Input::AimAssist::AimAssistTelemetry::Shot(_currentWeapon);
        }
        if (IsMainPlayer() && !_isBot && !Mods::SpectatorMode::IsSpectating()
            && Mods::Input::GamepadContexts::Current() == Mods::Input::GamepadContext::Gameplay)
        {
            Mods::Input::GamepadHaptics::Play(feedback);
        }
    }

    std::int32_t PlayerEntity::ModControllerWeaponSelection()
    {
        const auto stick = Mods::Input::GamepadInput::AimStick();
        const std::int32_t slot = Mods::Input::WeaponSelectionDirection::ControllerSlot(stick.first, stick.second);
        return ModResolveWeaponSlot(slot);
    }

    std::int32_t PlayerEntity::ModResolveWeaponSlot(std::int32_t slot)
    {
        if (slot < 0 || slot >= static_cast<std::int32_t>(_weaponSelectInsts.size()))
        {
            return -1;
        }
        const auto beam = static_cast<BeamType>(::MphRead::NativeRuntime::RequireReference(_weaponSelectInsts[static_cast<std::size_t>(slot)]).CurrentFrame);
        if (!_availableWeapons[beam])
        {
            return -1;
        }
        _weaponSelection = beam;
        return slot;
    }
}
