#include "PreviewAnimation.hpp"

#include "../../Formats/Model.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Render
{
    namespace
    {
        constexpr double TickHz = 30.0;
    }

    void PreviewAnimation::Update(ModelInstance& instance)
    {
        const auto now = std::chrono::steady_clock::now();
        // A stalled window (minimized, dragged) resumes instead of replaying
        // every missed 30 Hz tick in one draw.
        if (_last)
            Advance(instance, std::min(std::chrono::duration<double>(now - *_last).count(), 0.25));
        _last = now;
    }

    void PreviewAnimation::Advance(ModelInstance& instance, double seconds)
    {
        if (!std::isfinite(seconds) || seconds < 0) return;
        _phase += seconds * TickHz;
        const auto ticks = static_cast<std::uint64_t>(std::floor(_phase + 1e-9));
        _phase = std::max(0.0, _phase - static_cast<double>(ticks));
        for (std::uint64_t i = 0; i < ticks; ++i)
        {
            _clock.Step();
            const AnimationClock::TickScope scope(_clock);
            instance.UpdateAnimFrames();
        }
        _clock.Phase(_phase);
    }

    void PreviewAnimation::Reset() noexcept
    {
        _last.reset();
        _phase = 0;
        _clock.Phase(0);
    }

    const AnimationClock& PreviewAnimation::Clock() const noexcept
    {
        return _clock;
    }
}
