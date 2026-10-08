#pragma once

namespace MphRead::Mods::Input
{
    // PC precision: retain the control policy without DS Q12/LUT quantization.
    inline constexpr float AimEnvelopeCos = 0.9659258262890683F; // cos(15 degrees)
    inline constexpr float AimEnvelopeSin = 0.2588190451025208F;
    inline constexpr float AimFollow = 0.1F;
    inline constexpr float AimTargetFollow = 0.6F;
    constexpr float NativeZoomSensitivity(float halfFov, float normalHalfFov, float factor) noexcept
    { return 1.0F + (halfFov - normalHalfFov) * factor; }
}
