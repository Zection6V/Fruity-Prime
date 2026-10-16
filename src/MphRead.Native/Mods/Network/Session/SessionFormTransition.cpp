#include "SessionFormTransition.hpp"
#include "Mods/Network/Gameplay/Player/NetPlayerLifecycle.hpp"
#include "Mods/Network/Session/NetSession.hpp"
#include "Mods/Network/Diagnostics/NetLog.hpp"
#include "Mods/Network/Diagnostics/FormRequestTrace.hpp"

namespace MphRead::Mods::Network
{
    bool SessionFormTransition::_enabled = true;
    std::array<FormTransitionTimeline, NetworkSlotCapacity> SessionFormTransition::_timelines{};

    void SessionFormTransition::Receive(std::span<const std::uint8_t> payload)
    {
        if (NetSession::IsAuthority() || NetSession::FreezeGameplay()) return;
        FormTransitionPacket packet{};
        if (!FormTransitionPacket::TryRead(payload, packet)
            || !NetSession::MatchesStream(packet.Match, packet.Epoch)) return;
        for (std::size_t i = 0; i < packet.Count; ++i)
        {
            const auto& record = packet.Records[i];
            if (!NetPlayerLifecycle::Matches(record.Slot, record.Generation, record.Life)) continue;
            if (!NetSession::HasRemoteState(record.Slot)) continue;
            const auto& state = NetSession::RemoteState(record.Slot);
            if (state.Health == 0 || (state.Flags & PlayerState::FlagSpawned) == 0
                || (state.Flags & PlayerState::FlagSpectating) != 0) continue;
            if (_timelines[record.Slot].Receive(record) && FormTransitionTrace::Enabled() && NetLog::Enabled())
                NetLog::Event("[form-event] received match=" + std::to_string(packet.Match)
                    + " epoch=" + std::to_string(packet.Epoch) + " slot=" + std::to_string(record.Slot)
                    + " generation=" + std::to_string(record.Generation)
                    + " life=" + std::to_string(record.Life) + " id=" + std::to_string(record.Id)
                    + " start=" + std::to_string(record.StartFrame) + " frame=" + std::to_string(record.StampFrame)
                    + " direction=" + std::to_string(static_cast<int>(record.Direction))
                    + " status=" + std::to_string(static_cast<int>(record.Status))
                    + " anchor=" + std::to_string(record.AnchorFrame) + " clip=" + std::to_string(record.ClipFrame)
                    + " frames=" + std::to_string(record.ClipFrames) + " paused=" + std::to_string(record.Paused));
        }
    }

    std::optional<float> SessionFormTransition::Sample(std::int32_t slot, double readFrame,
        ReplicatedFormState::Transition direction)
    {
        if (!_enabled || slot < 0 || slot >= static_cast<std::int32_t>(_timelines.size())
            || slot == NetSession::LocalSlot() || NetSession::IsAuthority()) return std::nullopt;
        return _timelines[slot].Sample(NetPlayerLifecycle::Generation(slot), NetPlayerLifecycle::Get(slot), readFrame, direction);
    }

    void SessionFormTransition::Reset() { _timelines.fill(FormTransitionTimeline{}); }
    void SessionFormTransition::ResetSlot(std::int32_t slot)
    {
        if (slot >= 0 && slot < static_cast<std::int32_t>(_timelines.size())) _timelines[slot].Reset();
    }
}
