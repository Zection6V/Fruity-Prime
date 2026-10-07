#pragma once

#include <cstdint>
#include "MorphBallTouchRules.hpp"

namespace MphRead::Entities::MorphBallBoostStateMachine
{
    // The Morph Ball's two boosts as one state machine (EU1.1 0202360C-
    // 02023A24): what one frame does to Boosting, CanTouchBoost and the
    // shoulder charge, and which boost (if any) fires. PlayerEntity keeps the
    // state in its own fields, hands it over each frame and applies the
    // physics and side effects of whatever fired; nothing here touches them.

    struct State
    {
        bool Boosting = false;
        bool CanTouchBoost = true;
        std::uint16_t Charge = 0;
    };

    struct Inputs
    {
        bool TouchDown = false;
        bool TouchContinued = false;
        std::int16_t TouchDelta4X = 0;
        std::int16_t TouchDelta4Y = 0;
        bool ShoulderHeld = false;
    };

    // In simulation frames, already doubled for 60 Hz the way MphRead does it.
    struct ChargeLimits
    {
        std::uint16_t Min = 0;
        std::uint16_t Max = 0;
        // Features::FullBoostCharge: a release that clears Min uses Max.
        bool AlwaysFull = false;
    };

    enum class Fired : std::int32_t
    {
        None,
        TouchBoost,
        ShoulderBoost
    };

    struct Result
    {
        Fired Boost = Fired::None;
        // For ShoulderBoost: the charge the release spent (the state's own
        // charge is 0 by then).
        std::uint16_t ChargeSpent = 0;
        MorphBallTouchRules::BoostBranch Branch = MorphBallTouchRules::BoostBranch::Shoulder;
    };

    struct SampleLatch
    {
        bool Valid = false;
        std::uint64_t Identity = 0;
        MorphBallTouchRules::BoostBranch Branch = MorphBallTouchRules::BoostBranch::Shoulder;
    };

    // One frame. Updates state and says what fired.
    [[nodiscard]] Result Advance(State& state, const Inputs& inputs, const ChargeLimits& limits) noexcept;

    // Touch arbitration once per native sample; Shoulder retains its 60 Hz
    // adaptation. TouchBoost and SkipShoulder own the entire sample pair.
    [[nodiscard]] Result AdvanceSample(State& state, const Inputs& inputs, const ChargeLimits& limits,
        SampleLatch& latch, std::uint64_t sampleIdentity) noexcept;

    // How hard a fired boost is. Speed is the impulse magnitude, Cap the
    // horizontal speed cap it raises to, Damage the boost's collision damage.
    struct Strength
    {
        float Speed = 0;
        float Cap = 0;
        std::uint16_t Damage = 0;
    };

    struct BoostValues
    {
        float SpeedMin = 0;
        float SpeedMax = 0;
        float SpeedCap = 0;
        std::uint16_t Damage = 0;
    };

    // 0202369C-020237A0: full, whatever the charge.
    [[nodiscard]] Strength TouchBoostStrength(const BoostValues& values) noexcept;
    // 02023844-02023A20: proportional to the charge spent.
    [[nodiscard]] Strength ShoulderBoostStrength(const BoostValues& values, std::uint16_t chargeSpent,
        std::uint16_t chargeMax) noexcept;
}
