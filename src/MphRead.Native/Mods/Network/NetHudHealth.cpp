#include "NetHudHealth.hpp"

#include "DemoPlayback.hpp"
#include "NetPlayerLifecycle.hpp"
#include "NetProtocol.hpp"
#include "NetSession.hpp"

#include "../SpectatorMode.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"

namespace MphRead::Mods::Network
{
    bool NetHudHealth::HideOpponents()
    {
        // NetSession.ServerSession is { Match.HideOpponentHealth: true }.
        return NetSession::Active() && NetSession::ServerSession().has_value()
            && NetSession::ServerSession()->Match.HideOpponentHealth;
    }

    bool NetHudHealth::Visible(std::int32_t slot)
    {
        return !HideOpponents()
            || (slot == NetSession::LocalSlot() && !SpectatorMode::IsSpectating() && !DemoPlayback::IsActive());
    }

    HudHealthSample NetHudHealth::Sample(const ::MphRead::Entities::PlayerEntity& player)
    {
        const std::int32_t slot = player.SlotIndex();
        if (NetSession::Active() && !NetSession::IsAuthority() && !NetSession::IsHost()
            && slot >= 0 && slot < static_cast<std::int32_t>(NetSession::RemoteStates.size())
            && NetSession::RemoteStateValid[static_cast<std::size_t>(slot)])
        {
            const PlayerState& state = NetSession::RemoteStates[static_cast<std::size_t>(slot)];
            if (state.SlotIndex == slot && state.LifeId != 0
                && NetPlayerLifecycle::Matches(slot, state.SlotGeneration, state.LifeId))
            {
                return HudHealthSample{state.Health, NetSession::LastSnapshotFrame(), state.LifeId, true};
            }
        }
        const bool authoritative = !NetSession::Active() || NetSession::IsAuthority() || NetSession::IsHost();
        return HudHealthSample{player.Health(), authoritative ? NetSession::NetFrame() : 0U,
            NetPlayerLifecycle::Get(slot), authoritative};
    }
}
