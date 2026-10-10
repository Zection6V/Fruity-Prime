#pragma once

#include "NetProtocol.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace MphRead::Mods::Network
{
    // What became of a remote player's shot events on this machine. Kept
    // across lives: it describes the connection.
    struct ShotQueueStats final
    {
        // Events queued to be fired.
        std::uint64_t Queued = 0;
        // Sequences skipped by an event that arrived ahead of them: shots
        // possibly lost, possibly only late.
        std::uint64_t Gaps = 0;
        // Of those, the ones a later (reordered) intent still delivered.
        std::uint64_t Recovered = 0;
        // Of those, the ones that never came.
        std::uint64_t Lost = 0;
        // Queued events never fired: too old by the time they could be, or
        // pushed out of a full queue.
        std::uint64_t Stale = 0;
        std::uint64_t Overflow = 0;

        ShotQueueStats& operator+=(const ShotQueueStats& other) noexcept;
    };

    // One remote player's shot events, as this machine received them.
    //
    // Every intent repeats the sender's last few events (IntentPacket::
    // ShotHistory), so the same event arrives many times and an intent that
    // is lost or refused costs nothing. Each is queued once, by sequence, in
    // firing order -- a sequence skipped and then delivered by a reordered
    // intent too. The player's copy fires each as it arrives, with the
    // event's weapon and charge, and fires nothing without one.
    class RemoteShotQueue final
    {
    public:
        static constexpr std::size_t Capacity = 8;
        // How long after its frame (the sender's clock, against the newest
        // intent received) an event may still be fired: past it, the copy
        // could not fire it (morphed, holding on to an enemy) and a late shot
        // is worse than none.
        static constexpr std::uint32_t FreshFrames = 30;
        // How far behind the newest sequence a skipped one is still awaited:
        // past it, no intent that could carry it is still in flight.
        static constexpr std::uint32_t MissingWindow = 16;

        void Receive(const IntentPacket& intent) noexcept;
        // The oldest unspent event, after dropping stale ones.
        [[nodiscard]] std::optional<IntentPacket::ShotEvent> Next() noexcept;
        void Consume() noexcept;
        // Whether this player sends shot events at all: a slot that never
        // sent an intent (a bot) keeps firing from its own controls.
        [[nodiscard]] bool Active() const noexcept { return _active; }
        // A new life: what was pending is forgotten, the statistics stay.
        void Reset() noexcept;
        [[nodiscard]] const ShotQueueStats& Stats() const noexcept { return _stats; }

    private:
        void Enqueue(const IntentPacket::ShotEvent& event) noexcept;
        void PopFront() noexcept;
        void AwaitSkipped(std::uint32_t from, std::uint32_t to) noexcept;
        [[nodiscard]] bool TakeSkipped(std::uint32_t sequence) noexcept;
        void ExpireSkipped() noexcept;

        std::array<IntentPacket::ShotEvent, Capacity> _queue{};
        std::size_t _count = 0;
        std::array<std::uint32_t, MissingWindow> _skipped{};
        std::size_t _skippedCount = 0;
        std::uint32_t _lastSequence = 0;
        std::uint32_t _newestIntentFrame = 0;
        bool _active = false;
        ShotQueueStats _stats{};
    };
}
