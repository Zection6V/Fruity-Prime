#pragma once

#include <cstdint>

namespace MphRead::Entities::MorphBallTouchRules
{
    // How a touch moves the Morph Ball: which boost branch a frame takes, how
    // far a drag rolls it, and the impulse a swipe boost gives -- EU1.1
    // 02021C28's touch branches, kept apart from the player so they can be
    // pinned by tests. Pure functions over DS deltas.

    // 02021E1C: 0x5000 is 5.0 in fx20.12, multiplied by an integer delta and
    // shifted -- five raw units of speed per DS pixel, not five units.
    inline constexpr float TouchRollPerDsPixel = 5.0F / 4096.0F;
    // 02023658: strictly more than 90 DS units.
    inline constexpr std::int32_t TouchBoostThresholdSquared = 8100;

    enum class BoostBranch : std::int32_t
    {
        Shoulder,      // 02023844: the R path runs
        TouchBoost,    // 0202366C: fire, then 02023A24
        SkipShoulder   // 02023668 ble -> 02023A24: neither runs
    };

    struct PlanarDelta
    {
        float X = 0;
        float Z = 0;
    };

    // 02023624-02023668.
    [[nodiscard]] BoostBranch Arbitrate(bool boosting, bool canTouchBoost, bool continued,
        std::int16_t delta4X, std::int16_t delta4Y) noexcept;

    // 02021E28-02021F08. scale is TouchRollPerDsPixel, times
    // JumpPadSlideFactor while the jump pad lock holds.
    [[nodiscard]] PlanarDelta TouchRoll(std::int16_t delta4X, std::int16_t delta4Y, float scale,
        float rollFbX, float rollFbZ, float rollLrX, float rollLrZ) noexcept;

    // 0202366C-020236A0 and 020237BC-0202383C: full BoostSpeedMax along
    // -delta, against the current camera basis.
    [[nodiscard]] PlanarDelta TouchBoostImpulse(std::int32_t dx, std::int32_t dy, float boostSpeedMax,
        float cameraForwardX, float cameraForwardZ, float cameraSideX, float cameraSideZ) noexcept;
}
