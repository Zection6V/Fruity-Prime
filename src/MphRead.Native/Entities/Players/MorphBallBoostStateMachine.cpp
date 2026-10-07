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

    Result Advance(State& state, const Inputs& inputs, const ChargeLimits& limits) noexcept
    {
        // 0202360C: every frame without contact re-arms the touch boost.
        if (!inputs.TouchDown)
        {
            state.CanTouchBoost = true;
        }
        switch (MorphBallTouchRules::Arbitrate(state.Boosting, state.CanTouchBoost,
            inputs.TouchContinued, inputs.TouchDelta4X, inputs.TouchDelta4Y))
        {
        case MorphBallTouchRules::BoostBranch::TouchBoost:
            // 02023780-02023790. The charge is not touched: this branch never
            // reaches 02023A1C.
            state.Boosting = true;
            state.CanTouchBoost = false;
            return {Fired::TouchBoost, 0};
        case MorphBallTouchRules::BoostBranch::SkipShoulder:
            // 02023668 -> 02023A24: neither boost runs, the charge holds.
            return {};
        case MorphBallTouchRules::BoostBranch::Shoulder:
        default:
            return AdvanceShoulder(state, inputs.ShoulderHeld, limits);
        }
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
