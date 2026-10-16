#pragma once

#include "Mods/Network/Protocol/Packets/FormTransitionPacket.hpp"
#include <optional>

namespace MphRead::Mods::Network
{
    struct FormClipObservation final
    {
        std::uint16_t Frame = 0, Frames = 0;
        // Last native animation tick, in the same authority clock as snapshots.
        std::uint32_t AnchorFrame = 0;
        bool Paused = false;
    };

    // Observes accepted simulation results, never requests or relayed input.
    class FormTransitionAuthorityTracker final
    {
    public:
        bool Observe(std::uint32_t frame, std::uint8_t slot, std::uint16_t generation,
            std::uint16_t life, bool live, const ReplicatedFormState& form, const FormClipObservation& clip);
        [[nodiscard]] const std::optional<FormTransitionRecord>& Latest() const noexcept { return _latest; }
        void Reset() noexcept { *this = {}; }
    private:
        std::optional<FormTransitionRecord> _latest;
        std::uint16_t _generation = 0, _life = 0, _nextId = 0;
        std::uint32_t _frame = 0;
    };
}
