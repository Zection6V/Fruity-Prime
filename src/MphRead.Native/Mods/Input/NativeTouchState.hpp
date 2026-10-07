#pragma once

#include <array>
#include <cstdint>

namespace MphRead::Mods::Input
{
    // The DS touch state the Morph Ball reads, as EU1.1 02029778 produces it
    // into the input slot: current contact (+0x34 bit0), continued contact
    // (+0x34 bit3), the last four signed deltas (+0x38..+0x46) and their SUM
    // (+0x2A/+0x2C). A sum, not an average: the `lsl #2; asr #2` there only
    // keeps the result in 30 bits before the halfword store.
    //
    // Fed in DS screen units (256x192), never in host pixels: the host adapter
    // decides what is a DS contact and where, and everything after it is ROM.
    struct NativeTouchState
    {
        bool Down = false;
        bool PreviousDown = false;
        bool Continued = false;

        std::uint8_t X = 0;
        std::uint8_t Y = 0;
        std::uint8_t PreviousX = 0;
        std::uint8_t PreviousY = 0;

        std::array<std::int16_t, 4> DeltaHistoryX{};
        std::array<std::int16_t, 4> DeltaHistoryY{};

        std::int16_t Delta4X = 0;
        std::int16_t Delta4Y = 0;

        // One producer tick. dsX/dsY are clamped to 0-255 / 0-191.
        void Update(bool down, std::int32_t dsX, std::int32_t dsY) noexcept;
        // The same tick for a stylus known only by how far it moved (a
        // mouse): the deltas go into the history as they are, clamped to
        // what two u8 coordinates can differ by, so no edge ever eats one.
        void UpdateRelative(bool down, std::int32_t dx, std::int32_t dy) noexcept;
        void Clear() noexcept;
    };

    // A DS contact some other head measured (Android's aim finger), already in
    // DS units. While one is published it replaces the desktop stylus zone as
    // the producer's input; it is the adapter, not a second gameplay path.
    class HostTouch final
    {
    public:
        HostTouch() = delete;
        static void Publish(bool down, std::int32_t dsX, std::int32_t dsY) noexcept
        {
            _published = true;
            _down = down;
            _x = dsX;
            _y = dsY;
        }
        static void Withdraw() noexcept { _published = false; _down = false; }
        [[nodiscard]] static bool Published() noexcept { return _published; }
        [[nodiscard]] static bool Down() noexcept { return _down; }
        [[nodiscard]] static std::int32_t X() noexcept { return _x; }
        [[nodiscard]] static std::int32_t Y() noexcept { return _y; }

    private:
        inline static bool _published = false;
        inline static bool _down = false;
        inline static std::int32_t _x = 0;
        inline static std::int32_t _y = 0;
    };

    // A mouse as the DS stylus (02029778 fed by relative motion). Host pixels
    // become DS units at a fixed gain -- an adapter setting, never a gameplay
    // threshold -- with the fraction carried so slow motion is not lost. The
    // stylus is "down" while the mouse is moving and for a few idle steps
    // after, so a polling gap does not lift it; once it rests it lifts, which
    // is what re-arms Touch Boost.
    class MouseStylus final
    {
    public:
        static constexpr float DsUnitsPerPixel = 0.25F;
        static constexpr std::int32_t IdleStepsBeforeLift = 4;

        // One simulation step. Writes the producer.
        void Step(float pixelDx, float pixelDy, NativeTouchState& touch) noexcept;
        void Reset() noexcept { _carryX = _carryY = 0; _idle = IdleStepsBeforeLift; }

    private:
        float _carryX = 0;
        float _carryY = 0;
        std::int32_t _idle = IdleStepsBeforeLift;
    };

    // s16 sum of four s16 samples, wrapped the way `strh` stores it.
    [[nodiscard]] std::int16_t Sum4AsS16(const std::array<std::int16_t, 4>& samples) noexcept;

    // A position on the stylus zone (window-normalised) to a discrete DS
    // coordinate: 0-255 across, 0-191 down, rounded, clamped.
    [[nodiscard]] std::int32_t ToDsX(float normalizedX, float zoneLeft, float zoneWidth) noexcept;
    [[nodiscard]] std::int32_t ToDsY(float normalizedY, float zoneTop, float zoneHeight) noexcept;

    // The arithmetic of EU1.1 02021C28's touch branches, apart from the
    // player so it can be pinned by tests.
    namespace MorphTouchRom
    {
        // 02021E1C: 0x5000 is 5.0 in fx20.12, multiplied by an integer delta
        // and shifted -- five raw units of speed per DS pixel.
        inline constexpr float TouchRollPerDsPixel = 5.0F / 4096.0F;
        // 02023658: strictly more than 90 DS units.
        inline constexpr std::int32_t TouchBoostThresholdSquared = 8100;

        enum class BoostBranch : std::int32_t
        {
            Shoulder,      // 02023844: the R path runs
            TouchBoost,    // 0202366C: fire, then 02023A24
            SkipShoulder   // 02023668 ble -> 02023A24: neither runs
        };

        [[nodiscard]] BoostBranch Arbitrate(bool boosting, bool canTouchBoost, const NativeTouchState& touch) noexcept;

        struct RollDelta
        {
            float X = 0;
            float Z = 0;
        };

        // 02021E28-02021F08. scale is TouchRollPerDsPixel, times
        // JumpPadSlideFactor while the jump pad lock holds.
        [[nodiscard]] RollDelta TouchRoll(std::int16_t delta4X, std::int16_t delta4Y, float scale,
            float rollFbX, float rollFbZ, float rollLrX, float rollLrZ) noexcept;

        // 0202366C-020236A0 and 020237BC-0202383C: full BoostSpeedMax along
        // -delta, against the current camera basis.
        [[nodiscard]] RollDelta TouchBoostImpulse(std::int32_t dx, std::int32_t dy, float boostSpeedMax,
            float cameraForwardX, float cameraForwardZ, float cameraSideX, float cameraSideZ) noexcept;
    }
}
