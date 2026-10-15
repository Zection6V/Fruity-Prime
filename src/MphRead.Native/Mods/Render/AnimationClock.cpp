#include "AnimationClock.hpp"

#include "FrameTiming.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Render
{
    thread_local const AnimationClock* AnimationClock::_ticking = nullptr;

    AnimationClock::TickScope::TickScope(const AnimationClock& clock) noexcept
        : _previous(_ticking)
    {
        _ticking = &clock;
    }

    AnimationClock::TickScope::~TickScope()
    {
        _ticking = _previous;
    }

    AnimationClock::AnimationClock() noexcept
        : AnimationClock(FrameTiming::StepSeconds)
    {
    }

    AnimationClock::AnimationClock(double secondsPerStep) noexcept
        : _secondsPerStep(secondsPerStep)
    {
    }

    void AnimationClock::Step() noexcept
    {
        ++_steps;
        _phase = 0;
    }

    void AnimationClock::Phase(double value) noexcept
    {
        _phase = std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0.0;
    }

    double AnimationClock::TickTime() const noexcept
    {
        return static_cast<double>(_steps);
    }

    double AnimationClock::DrawTime() const noexcept
    {
        return static_cast<double>(_steps) + _phase;
    }

    double AnimationClock::SecondsPerStep() const noexcept
    {
        return _secondsPerStep;
    }

    const AnimationClock* AnimationClock::Ticking() noexcept
    {
        return _ticking;
    }

    AnimationClock& AnimationClock::Simulation() noexcept
    {
        // Per thread: a server simulation on its own thread never draws, and
        // must not share a time line with the window's scene.
        thread_local AnimationClock clock;
        return clock;
    }
}
