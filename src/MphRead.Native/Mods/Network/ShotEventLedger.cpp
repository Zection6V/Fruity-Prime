#include "ShotEventLedger.hpp"

namespace MphRead::Mods::Network
{
    void ShotEventLedger::Note(const IntentPacket::ShotEvent& event) noexcept
    {
        if (event.Sequence != 0)
        {
            _events[event.Sequence % Capacity] = event;
        }
    }

    std::optional<IntentPacket::ShotEvent> ShotEventLedger::Find(std::uint32_t sequence) const noexcept
    {
        if (sequence == 0)
        {
            return std::nullopt;
        }
        const IntentPacket::ShotEvent& event = _events[sequence % Capacity];
        if (event.Sequence != sequence)
        {
            return std::nullopt;
        }
        return event;
    }
}
