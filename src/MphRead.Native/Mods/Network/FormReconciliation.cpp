#include "FormReconciliation.hpp"

#include "../../NativeRuntime/System/Managed.hpp"

namespace MphRead::Mods::Network
{
    void FormReconciliation::Reset() noexcept
    {
        *this = FormReconciliation{};
    }

    FormCorrection FormReconciliation::Step(std::uint32_t frame, bool desiredAlt, bool actualAlt,
        bool morphing, bool unmorphing, std::int32_t pingMilliseconds)
    {
        const bool transitioning = morphing || unmorphing;
        if (transitioning)
        {
            const bool target = morphing;
            if (!_transitionActive || _transitionTarget != target)
            {
                _transitionSince = frame;
            }
            _transitionSeen = true;
            _transitionTarget = target;
            _transitionLastSeen = frame;
        }
        // Keep the last transition for latency grace, but do not charge a
        // later same-direction animation for time spent settled in between.
        _transitionActive = transitioning;

        if (actualAlt == desiredAlt && !transitioning)
        {
            _mismatching = false;
            _attempted = false;
            return FormCorrection::None;
        }

        // A morph only changes IsAltForm at animation end; an unmorph changes
        // it immediately. In both cases older snapshots/intents can still
        // describe the form from before the relayed press.
        if (transitioning)
        {
            if (frame - _transitionSince >= MaximumTransition)
            {
                Reset();
                return FormCorrection::Force;
            }
            return FormCorrection::None;
        }
        const std::uint32_t latencyGrace = static_cast<std::uint32_t>(
            ::MphRead::NativeRuntime::MathClamp(pingMilliseconds * 60 / 1000 + 8, 8, 32));
        if (_transitionSeen && !_attempted && _transitionTarget == actualAlt
            && frame - _transitionLastSeen <= latencyGrace)
        {
            return FormCorrection::None;
        }

        if (_attempted)
        {
            if (frame - _attemptSince < FailedSwitchGrace)
            {
                return FormCorrection::None;
            }
            Reset();
            return FormCorrection::Force;
        }
        if (!_mismatching)
        {
            _mismatchSince = frame;
            _mismatching = true;
        }
        if (frame - _mismatchSince < MismatchGrace)
        {
            return FormCorrection::None;
        }
        _attempted = true;
        _attemptSince = frame;
        return FormCorrection::Start;
    }
}
