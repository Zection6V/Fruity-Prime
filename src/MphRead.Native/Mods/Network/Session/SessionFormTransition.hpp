#pragma once

#include "Mods/Network/Gameplay/Replication/FormTransitionTimeline.hpp"

namespace MphRead::Mods::Network
{
    class SessionFormTransition final
    {
    public:
        static void Receive(std::span<const std::uint8_t> payload);
        [[nodiscard]] static std::optional<float> Sample(std::int32_t slot, double readFrame,
            ReplicatedFormState::Transition direction);
        static void Reset();
        static void ResetSlot(std::int32_t slot);
        static bool Enabled() noexcept { return _enabled; }
        static void Enabled(bool enabled) noexcept { _enabled = enabled; }
    private:
        static bool _enabled;
        static std::array<FormTransitionTimeline, NetworkSlotCapacity> _timelines;
    };
}
