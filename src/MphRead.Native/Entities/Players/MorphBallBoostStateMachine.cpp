#include "MorphBallBoostStateMachine.hpp"

#include "MorphBallTouchRules.hpp"

namespace MphRead::Entities::MorphBallBoostStateMachine
{
    namespace
    {
        // 02023844-02023A20: R held charges; R released fires if the charge
        // clears the minimum, and either way the charge is spent.
        Result AdvanceShoulder(State& state, bool held, const ChargeLimits& limits) noexcept
        {
            if (held)
            {
                if (state.Charge < limits.Max)
                {
                    ++state.Charge;
                }
                return {};
            }
            Result result{};
            if (state.Charge > limits.Min)
            {
                result.Boost = Fired::ShoulderBoost;
                result.ChargeSpent = limits.AlwaysFull ? limits.Max : state.Charge;
                state.Boosting = true;
            }
            state.Charge = 0;
            return result;
        }
    }

    namespace
    {
        MorphBallTouchRules::BoostBranch SelectBranch(State& state, const Inputs& inputs) noexcept
        {
            // 0202360C: a native sample without contact re-arms the boost.
            if (!inputs.TouchDown) state.CanTouchBoost = true;
            return MorphBallTouchRules::Arbitrate(state.Boosting, state.CanTouchBoost,
                inputs.TouchContinued, inputs.TouchDelta4X, inputs.TouchDelta4Y);
        }

        Result AdvanceBranch(State& state, const Inputs& inputs, const ChargeLimits& limits,
            MorphBallTouchRules::BoostBranch branch, bool newSample) noexcept
        {
            switch (branch)
            {
            case MorphBallTouchRules::BoostBranch::TouchBoost:
                // 02023780-02023790: preserve R's charge, fire only once.
                if (newSample)
                {
                    state.Boosting = true;
                    state.CanTouchBoost = false;
                }
                return {newSample ? Fired::TouchBoost : Fired::None, 0, branch};
            case MorphBallTouchRules::BoostBranch::SkipShoulder:
                // 02023668 -> 02023A24: neither boost runs, charge holds.
                return {Fired::None, 0, branch};
            case MorphBallTouchRules::BoostBranch::Shoulder:
            default:
                return AdvanceShoulder(state, inputs.ShoulderHeld, limits);
            }
        }
    }

    Result Advance(State& state, const Inputs& inputs, const ChargeLimits& limits) noexcept
    {
        return AdvanceBranch(state, inputs, limits, SelectBranch(state, inputs), true);
    }

    Result AdvanceSample(State& state, const Inputs& inputs, const ChargeLimits& limits,
        SampleLatch& latch, std::uint64_t sampleIdentity) noexcept
    {
        const bool newSample = !latch.Valid || latch.Identity != sampleIdentity;
        if (newSample)
        {
            latch.Valid = true;
            latch.Identity = sampleIdentity;
            latch.Branch = SelectBranch(state, inputs);
        }
        return AdvanceBranch(state, inputs, limits, latch.Branch, newSample);
    }

    Strength TouchBoostStrength(const BoostValues& values) noexcept
    {
        return {values.SpeedMax, values.SpeedCap, values.Damage};
    }

    Strength ShoulderBoostStrength(const BoostValues& values, std::uint16_t chargeSpent,
        std::uint16_t chargeMax) noexcept
    {
        if (chargeMax == 0)
        {
            return {values.SpeedMin, 0, 0};
        }
        // The same expressions, in the same order, as MphRead's.
        return {
            values.SpeedMin + chargeSpent * (values.SpeedMax - values.SpeedMin) / static_cast<float>(chargeMax),
            values.SpeedCap * chargeSpent / static_cast<float>(chargeMax),
            static_cast<std::uint16_t>(static_cast<std::int32_t>(values.Damage) * chargeSpent / chargeMax)
        };
    }
}
