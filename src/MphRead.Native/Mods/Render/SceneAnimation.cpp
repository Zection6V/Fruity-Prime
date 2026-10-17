#include "SceneAnimation.hpp"

#include "FrameTiming.hpp"
#include "Mods/Network/Replay/DemoPlayback.hpp"
#include "../Headless.hpp"
#include "../../Formats/Model.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <unordered_map>

namespace MphRead::Mods::Render
{
    namespace
    {
        const AnimationClock& StepSimulation() noexcept
        {
            AnimationClock& clock = AnimationClock::Simulation();
            clock.Step();
            return clock;
        }
    }

    void SceneAnimation::TickFromDraw(ModelInstance& instance)
    {
        const AnimationClock& clock = AnimationClock::Simulation();
        const AnimationTick& tick = NativeRuntime::RequireReference(instance.AnimInfo).ModTick;
        if (tick.Clock == &clock && tick.Time == clock.TickTime()) return;
        const AnimationClock::TickScope scope(clock);
        instance.UpdateAnimFrames();
    }

    std::int32_t SceneAnimation::StepsForDraw(const void* key)
    {
        thread_local std::unordered_map<const void*, double> advanced;
        const double now = AnimationClock::Simulation().TickTime();
        const auto [it, first] = advanced.try_emplace(key, now);
        if (first) return 1;
        const double steps = now - it->second;
        it->second = now;
        return static_cast<std::int32_t>(std::clamp(steps, 0.0,
            static_cast<double>(FrameTiming::MaxCatchUpSteps)));
    }

    SceneAnimation::Step::Step() noexcept
    {
        if (!Headless::Active()) _ticks.emplace(StepSimulation());
    }

    SceneAnimation::Draw::Draw() noexcept
    {
        // At demo EOF the final world step is held. Do not advance even the
        // fractional interpolation phase between display refreshes.
        AnimationClock::Simulation().Phase(
            Network::DemoPlayback::HoldLastFrame() ? 0.0 : FrameTiming::StepPhase());
    }
}
