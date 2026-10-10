#pragma once

#include "NetProtocol.hpp"

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
    // firing order. The player's copy fires each as it arrives -- with the
    // event's weapon and charge -- and fires nothing without one.
    class RemoteShotQueue final
    {
    public:
        static constexpr std::size_t Capacity = 8;
        // How long after its frame (the sender's clock) an event may still
        // be fired: past it, the copy could not fire it (morphed, holding
        // on to an enemy) and a late shot is worse than none.
        static constexpr std::uint32_t FreshFrames = 30;

        void Receive(const IntentPacket& intent) noexcept;
        // The oldest unspent event, after dropping stale ones.
        [[nodiscard]] std::optional<IntentPacket::ShotEvent> Next() noexcept;
        void Consume() noexcept;
        // Whether this player sends shot events at all: a slot that never
        // sent an intent (a bot) keeps firing from its own controls.
        [[nodiscard]] bool Active() const noexcept { return _active; }
        void Reset() noexcept { *this = RemoteShotQueue{}; }

    private:
        void PopFront() noexcept;

        std::array<IntentPacket::ShotEvent, Capacity> _queue{};
        std::size_t _count = 0;
        std::uint32_t _lastSequence = 0;
        std::uint32_t _newestIntentFrame = 0;
        bool _active = false;
    };
}
