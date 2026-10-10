#pragma once

#include "NetProtocol.hpp"
#include "../../Formats/Enums.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace MphRead::Mods::Network
{
    // One remote player's shot events, as this machine received them.
    //
    // Every intent repeats the sender's last few events (IntentPacket::
    // ShotHistory), so the same event arrives many times and an intent that
    // is lost or refused costs nothing. Each is queued once, by sequence, in
    // firing order; the player's copy reads the oldest as it fires and spends
    // it with the shot. The weapon an event names is the weapon that copy
    // fires -- never the one it happens to hold.
    class RemoteShotQueue final
    {
    public:
        static constexpr std::size_t Capacity = 8;
        // How long after its frame an event may still pick a shot's weapon:
        // past it, the trigger it belonged to never fired here.
        static constexpr std::uint32_t FreshFrames = 30;

        void Receive(const IntentPacket& intent) noexcept;
        // The weapon of the oldest unspent event, after dropping stale ones.
        [[nodiscard]] std::optional<::MphRead::BeamType> Next() noexcept;
        void Consume() noexcept;
        void Reset() noexcept { *this = RemoteShotQueue{}; }

    private:
        void PopFront() noexcept;

        std::array<IntentPacket::ShotEvent, Capacity> _queue{};
        std::size_t _count = 0;
        std::uint32_t _lastSequence = 0;
        std::uint32_t _newestIntentFrame = 0;
    };
}
