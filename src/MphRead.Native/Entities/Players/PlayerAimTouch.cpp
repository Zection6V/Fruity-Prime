#include "PlayerEntity.hpp"
#include "../../Features.hpp"
#include <algorithm>

namespace MphRead::Entities
{
    void PlayerEntity::ApplyTouchAim()
    {
        if (!_input.TouchSample().NewNativeSampleThisStep()) return;
        const auto& touch = _input.Touch();
        if (!touch.Down || touch.ContactDuration <= _values.AimMinTouchTime) return;
        // Preserve calibrated sensitivity; the angle itself is never quantized.
        const auto x = touch.Delta4X * _controls.NativeControl().TouchScaleX;
        const auto y = touch.Delta4Y * _controls.NativeControl().TouchScaleY;
        _aimFrame.Yaw = x; _aimFrame.Pitch = y;
        if (IsMainPlayer())
        {
            _hudShiftY = Features::HudSway() && !Features::FixedWeapon() ? std::clamp(-y / 4, -8.0F, 8.0F) : 0;
            _hudShiftX = Features::HudSway() && !Features::FixedWeapon() ? std::clamp(-x / 4, -8.0F, 8.0F) : 0;
            _objShiftY = -_hudShiftY / 2; _objShiftX = _hudShiftX / 2;
        }
        UpdateAimY(y); UpdateAimX(x);
        _input.HasInput |= x != 0 || y != 0;
    }
}
