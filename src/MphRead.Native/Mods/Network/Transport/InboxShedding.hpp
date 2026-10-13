#pragma once

#include "Mods/Network/Protocol/NetProtocol.hpp"

#include <cstddef>

namespace MphRead::Mods::Network
{
    // A packet whose loss the next one of its kind makes good: a stream sent
    // every frame (an intent repeats its recent shots and bombs, a snapshot
    // its recent damage), or a measurement that is simply taken again. Every
    // other packet -- a welcome, a map change, a match state, a hit verdict --
    // is said once, and losing it to a full queue loses it for good.
    [[nodiscard]] constexpr bool Superseded(PacketType type) noexcept
    {
        switch (type)
        {
        case PacketType::Intent:
        case PacketType::SlotIntent:
        case PacketType::Snapshot:
        case PacketType::Ping:
        case PacketType::Pong:
        case PacketType::StatusQuery:
        case PacketType::StatusReply:
        case PacketType::MasterHeartbeat:
            return true;
        default:
            return false;
        }
    }

    // Which queued packet to drop when the queue is full, oldest first: the
    // oldest that a newer one supersedes, and only when there is none, the
    // oldest of all. `typeOf` reads a queued packet's type.
    template <typename Queue, typename TypeOf>
    [[nodiscard]] std::size_t ShedIndex(const Queue& queue, TypeOf typeOf)
    {
        std::size_t index = 0;
        for (const auto& packet : queue)
        {
            if (Superseded(typeOf(packet)))
            {
                return index;
            }
            index++;
        }
        return 0;
    }
}
