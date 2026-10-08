#include "PlayerEntity.hpp"
#include "../../Mods/Input/GamepadInput.hpp"
#include "../../Mods/Input/GamepadOptions.hpp"
#include "../../Mods/SpectatorMode.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

namespace MphRead::Entities
{
    void PlayerEntity::ApplyGamepadAim()
    {
        if ((*this).IsBot() || (*this).SlotIndex() != PlayerEntity::MainPlayerIndex()
            || Mods::SpectatorMode::IsSpectating()
            || TestFlag(_flags1, PlayerFlags1::NoAimInput))
        {
            _controllerAssist.Reset();
            return;
        }
        const bool zoomed = ::MphRead::NativeRuntime::RequireReference(_equipInfo).Zoomed;
        float x = Mods::Input::GamepadInput::AimDeltaX() * (zoomed ? Mods::Input::GamepadOptions::ScopedX() : 1);
        float y = Mods::Input::GamepadInput::AimDeltaY() * (zoomed ? Mods::Input::GamepadOptions::ScopedY() : 1);
        const auto assisted = ApplyControllerAssist(x, y);
        x = assisted.X();
        y = assisted.Y();
        if (x == 0.0F && y == 0.0F)
        {
            return;
        }

        _aimFrame.Yaw = x; _aimFrame.Pitch = y;
        ModNoteInput();
        (*this).UpdateHudShiftY(y);
        (*this).UpdateHudShiftX(x);
        (*this).UpdateAimY(y);
        (*this).UpdateAimX(x);
    }

}
