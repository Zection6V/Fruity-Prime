#pragma once

#include "NativeTouchClock.hpp"
#include "NativeTouchState.hpp"

namespace MphRead::Mods::Input
{
    // One 30 Hz sample, shared by two 60 Hz steps. A reported step is admitted
    // once: holding an intent through packet loss must not add its roll again.
    class NativeTouchSample final
    {
    public:
        void BeginStep() noexcept
        {
            _newSample = false;
            _rollShare = 0;
        }

        [[nodiscard]] bool AdvanceLocal() noexcept
        {
            BeginStep();
            _newSample = _clock.Advance();
            if (_newSample) ++_sequence;
            _secondStep = !_newSample;
            _rollShare = 1.0F / NativeTouchClock::SimulationStepsPerTick;
            return _newSample;
        }

        void Suspend() noexcept
        {
            BeginStep();
            _state.Clear();
            _clock.Reset();
            // Never reuse an identity after a menu hands input back.
            ++_sequence;
            _secondStep = false;
        }

        void ApplyReported(const NativeTouchState::Reported& reported,
            std::uint16_t generation, std::uint16_t life) noexcept
        {
            BeginStep();
            if (_generation != generation || _life != life)
            {
                _hasReported = false;
                _generation = generation;
                _life = life;
            }
            if (_hasReported && reported.SampleSequence != _sequence)
            {
                const std::uint32_t distance = reported.SampleSequence - _sequence;
                if (distance >= 0x80000000U) return; // stale, including across wrap
            }
            if (!_hasReported || reported.SampleSequence != _sequence)
            {
                _newSample = true;
                _sequence = reported.SampleSequence;
                _state.Assign(reported);
            }
            else if (_secondStep || !reported.SecondStep)
            {
                return; // duplicate or reordered first substep
            }
            _hasReported = true;
            _secondStep = reported.SecondStep;
            _rollShare = 1.0F / NativeTouchClock::SimulationStepsPerTick;
        }

        [[nodiscard]] NativeTouchState& State() noexcept { return _state; }
        [[nodiscard]] const NativeTouchState& State() const noexcept { return _state; }
        [[nodiscard]] bool NewNativeSampleThisStep() const noexcept { return _newSample; }
        [[nodiscard]] std::uint32_t NativeSampleSequence() const noexcept { return _sequence; }
        [[nodiscard]] std::uint64_t Identity() const noexcept
        {
            return (static_cast<std::uint64_t>(_generation) << 48)
                | (static_cast<std::uint64_t>(_life) << 32) | _sequence;
        }
        [[nodiscard]] float RollShare() const noexcept { return _rollShare; }
        [[nodiscard]] NativeTouchState::Reported Report() const noexcept
        {
            auto report = _state.Report();
            report.SampleSequence = _sequence;
            report.SecondStep = _secondStep;
            return report;
        }

    private:
        NativeTouchState _state{};
        NativeTouchClock _clock{};
        std::uint32_t _sequence = 0;
        std::uint16_t _generation = 0;
        std::uint16_t _life = 0;
        bool _secondStep = false;
        bool _newSample = false;
        bool _hasReported = false;
        float _rollShare = 0;
    };
}
