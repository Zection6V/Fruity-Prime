#pragma once

#include "NetProtocol.hpp"
#include "../../Formats/Enums.hpp"

#include <array>
#include <cstdint>

namespace MphRead::Mods::Network
{
    // This machine's own shot events: a sequence number per shot and the
    // weapon it left, taken the moment it is fired. The last few ride every
    // intent (IntentPacket::ShotHistory), newest last. Continuous fire makes
    // no events -- a Shock Coil would push every other shot out each frame.
    class LocalShotLog final
    {
    public:
        void Record(std::uint32_t frame, ::MphRead::BeamType weapon, std::uint16_t charge) noexcept;
        // The history, and -- when the intent carries this frame's ray --
        // that shot's sequence and weapon.
        void Fill(IntentPacket& intent, std::uint32_t frame) const noexcept;
        void Reset() noexcept;

    private:
        std::array<IntentPacket::ShotEvent, IntentPacket::ShotHistoryCount> _history{};
        std::uint8_t _length = 0;
        // Not reset with the history: a sequence is never reused, so a copy
        // that kept an older one cannot mistake a new shot for it.
        std::uint32_t _sequence = 0;
    };
}
