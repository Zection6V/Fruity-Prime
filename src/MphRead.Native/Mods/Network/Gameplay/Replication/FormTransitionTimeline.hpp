#pragma once

#include "Mods/Network/Protocol/Packets/FormTransitionPacket.hpp"
#include <optional>

namespace MphRead::Mods::Network
{
    // A bounded history of observations of accepted transitions. Receiving a
    // packet never runs gameplay, audio, or turret lifecycle methods.
    class FormTransitionTimeline final
    {
    public:
        [[nodiscard]] bool Receive(const FormTransitionRecord& record);
        [[nodiscard]] std::optional<float> Sample(std::uint16_t generation, std::uint16_t life,
            double readFrame, ReplicatedFormState::Transition direction) const noexcept;
        void Reset() noexcept { *this = {}; }
    private:
        static constexpr std::size_t Capacity = 64;
        std::array<std::optional<FormTransitionRecord>, Capacity> _history{};
        std::optional<FormTransitionRecord> _latest;
        std::size_t _next = 0;
    };
}
