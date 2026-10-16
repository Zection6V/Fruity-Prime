#pragma once

#include "Mods/Network/Gameplay/Replication/FormTransitionAuthorityTracker.hpp"
#include "Mods/Network/Server/Core/ServerPeerRegistry.hpp"

namespace MphRead::Mods::Network
{
    class NetTransport;

    // Separate bounded datagrams leave the eight-player snapshot/item budget
    // untouched. Latest records are refreshed every three authority ticks,
    // including terminal records, so loss and late joins recover without ACKs.
    class ServerFormTransitionChannel final
    {
    public:
        void ObserveSnapshot(std::span<const std::uint8_t> snapshot,
            const ServerPeerRegistry& peers, NetTransport* transport);
        void SendLatest(const ServerPeerRegistry::PeerHandle& peer, NetTransport* transport) const;
    private:
        std::array<FormTransitionAuthorityTracker, NetworkSlotCapacity> _trackers{};
        std::array<std::uint8_t, FormTransitionPacket::MaxSize> _buffer{};
        std::size_t _size = 0;
        std::uint16_t _match = 0;
        std::uint64_t _epoch = 0;
        std::uint32_t _sent = 0;
    };
}
