#include "ServerFormTransitionChannel.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "Mods/Network/Diagnostics/NetLog.hpp"
#include "Mods/Network/Diagnostics/FormRequestTrace.hpp"
#include "Mods/Network/Protocol/Packets/RealtimePackets.hpp"
#include "Mods/Network/Replay/DemoRecorder.hpp"
#include "Mods/Network/Transport/NetTransport.hpp"

namespace MphRead::Mods::Network
{
    void ServerFormTransitionChannel::ObserveSnapshot(std::span<const std::uint8_t> snapshot,
        const ServerPeerRegistry& peers, NetTransport* transport)
    {
        if (snapshot.size() < SnapshotHeader::Size) return;
        const auto header = SnapshotHeader::Read(snapshot);
        if (header.PlayerCount > NetworkSlotCapacity
            || snapshot.size() < SnapshotHeader::Size + header.PlayerCount * PlayerState::Size) return;
        if (_match != header.MatchId || _epoch != header.AuthorityEpoch)
        {
            _trackers.fill(FormTransitionAuthorityTracker{});
            _match = header.MatchId;
            _epoch = header.AuthorityEpoch;
            _sent = 0;
            _size = 0;
        }
        bool changed = false;
        std::uint32_t occupied = 0;
        for (std::size_t i = 0; i < header.PlayerCount; ++i)
        {
            const auto state = PlayerState::Read(snapshot.subspan(SnapshotHeader::Size + i * PlayerState::Size));
            if (state.SlotIndex >= NetworkSlotCapacity) continue;
            occupied |= 1U << state.SlotIndex;
            const auto& player = Entities::PlayerEntity::Players()[state.SlotIndex];
            if (player == nullptr) continue;
            ReplicatedFormState form{};
            if (!ReplicatedFormState::TryCreate((state.Flags & PlayerState::FlagAltForm) != 0,
                (state.WeavelFlags & PlayerState::WeavelFlagMorphing) != 0,
                (state.WeavelFlags & PlayerState::WeavelFlagUnmorphing) != 0,
                state.FormTransitionProgress(), form)) continue;
            const bool live = state.Health > 0 && (state.Flags & PlayerState::FlagSpawned) != 0
                && (state.Flags & PlayerState::FlagSpectating) == 0;
            const bool edge = _trackers[state.SlotIndex].Observe(header.Frame, state.SlotIndex,
                state.SlotGeneration, state.LifeId, live, form, player->ModObserveFormClip(header.Frame));
            changed = changed || edge;
            if (edge && FormTransitionTrace::Enabled() && NetLog::Enabled())
            {
                const auto& record = _trackers[state.SlotIndex].Latest();
                if (record) NetLog::Event("[form-event] authority match=" + std::to_string(_match)
                    + " epoch=" + std::to_string(_epoch) + " slot=" + std::to_string(record->Slot)
                    + " generation=" + std::to_string(record->Generation)
                    + " life=" + std::to_string(record->Life) + " id=" + std::to_string(record->Id)
                    + " start=" + std::to_string(record->StartFrame) + " frame=" + std::to_string(header.Frame)
                    + " direction=" + std::to_string(static_cast<int>(record->Direction))
                    + " status=" + std::to_string(static_cast<int>(record->Status)));
            }
        }
        for (std::size_t slot = 0; slot < _trackers.size(); ++slot)
            if ((occupied & (1U << slot)) == 0) _trackers[slot].Reset();
        if (!changed && _sent != 0 && header.Frame - _sent < 3) return;
        FormTransitionPacket packet{};
        packet.Match = _match;
        packet.Epoch = _epoch;
        packet.Frame = header.Frame;
        for (const auto& tracker : _trackers)
            if (tracker.Latest() && tracker.Latest()->Valid()) packet.Records[packet.Count++] = *tracker.Latest();
        _size = packet.Write(_buffer);
        _sent = header.Frame;
        if (_size == 0 || packet.Count == 0) return;
        const auto payload = std::span<const std::uint8_t>(_buffer).first(_size);
        DemoRecorder::RecordOwnPacket(PacketType::FormTransition, payload);
        for (const auto& peer : peers.All()) SendLatest(peer, transport);
    }

    void ServerFormTransitionChannel::SendLatest(const ServerPeerRegistry::PeerHandle& peer,
        NetTransport* transport) const
    {
        if (_size != 0 && peer != nullptr && transport != nullptr)
            transport->Send(peer->EndPoint, PacketType::FormTransition,
                std::span<const std::uint8_t>(_buffer).first(_size));
    }
}
