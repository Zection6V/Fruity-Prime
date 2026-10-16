#include "PlayerEntity.hpp"
#include "../../Mods/Input/MouseAimTurn.hpp"

namespace MphRead::Entities
{
    void PlayerEntity::ApplyMouseAim()
    {
        const auto [yaw, pitch] = Mods::Input::MouseAimTurn(_input.MouseDeltaX(), _input.MouseDeltaY());
        _aimFrame.Yaw = yaw; _aimFrame.Pitch = pitch;
        _input.HasInput |= yaw != 0 || pitch != 0;
        UpdateHudShiftY(pitch); UpdateHudShiftX(yaw);
        // Mod hook: frames since the last step may already have turned part of
        // this step's motion at the display's rate (Mods/Input/FrameAim).
        const auto [shareX, shareY] = ModStepMouseAim(_input.MouseDeltaX(), _input.MouseDeltaY());
        const auto share = Mods::Input::MouseAimTurn(shareX, shareY);
        // Direct mouse never waits for Touch or native gameplay phase.
        UpdateAimY(share.Pitch); UpdateAimX(share.Yaw);
    }
}
