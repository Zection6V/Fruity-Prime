#pragma once

#include <cstdint>

namespace MphRead::Mods::Network
{
    enum class FormCorrection : std::uint8_t { None, Start, Force };

    // Per-slot timing for reconciling a puppet's form with its source.
    struct FormReconciliation final
    {
        void Reset() noexcept;

        [[nodiscard]] FormCorrection Step(std::uint32_t frame, bool desiredAlt, bool actualAlt,
            bool morphing, bool unmorphing, std::int32_t pingMilliseconds);

    private:
        static constexpr std::uint32_t MismatchGrace = 8;
        static constexpr std::uint32_t FailedSwitchGrace = 12;
        // The model's morph/unmorph animation length is asset dependent. This
        // is an emergency ceiling, not the normal mismatch grace.
        static constexpr std::uint32_t MaximumTransition = 90;

        std::uint32_t _mismatchSince = 0;
        std::uint32_t _attemptSince = 0;
        std::uint32_t _transitionSince = 0;
        std::uint32_t _transitionLastSeen = 0;
        bool _mismatching = false;
        bool _attempted = false;
        bool _transitionSeen = false;
        bool _transitionActive = false;
        bool _transitionTarget = false;
    };
}
