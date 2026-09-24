#include "NetHooks.hpp"

#include "DemoPlayback.hpp"
#include "MapAudit.hpp"
#include "NetDiagnostics.hpp"
#include "NetLog.hpp"
#include "NetMatchEnd.hpp"
#include "NetMatchSync.hpp"
#include "NetPlayerBridge.hpp"
#include "NetPlayerSetup.hpp"
#include "NetProtocol.hpp"
#include "NetRoomChange.hpp"
#include "NetSession.hpp"
#include "NetSlotManager.hpp"
#include "NetTestScript.hpp"
#include "PlayerColors.hpp"
#include "../SpectatorMode.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Scene.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::TestFlag;

namespace
{
}

namespace MphRead::Mods::Network
{
    std::int32_t NetHooks::LocalSlot()
    {
        if (DemoPlayback::IsActive() || NetSession::Role() == NetRole::Server)
        {
            return -1;
        }
        if (NetSession::Active() && NetSession::LocalSlot() >= 0)
        {
            return NetSession::LocalSlot();
        }
        return 0;
    }

    bool NetHooks::IsPuppet(Entities::PlayerEntity& player)
    {
        return (NetSession::Active() || DemoPlayback::IsActive())
            && player.SlotIndex() != LocalSlot();
    }

    bool NetHooks::KeepSlotAlive(Entities::PlayerEntity&)
    {
        return NetSession::Active();
    }

    void NetHooks::AfterRemoteMovement(Entities::PlayerEntity& player)
    {
        if (!NetSession::Active() || !NetSession::IsAuthority()
            || player.SlotIndex() == NetSession::LocalSlot() || NetRoomChange::Settling())
        {
            return;
        }
        const std::int32_t slot = player.SlotIndex();
        if (slot < 0
            || static_cast<std::size_t>(slot) >= NetSession::RemoteIntents.size()
            || !NetSession::RemoteIntentValid.at(static_cast<std::size_t>(slot)))
        {
            return;
        }
        if (!TestFlag(player.LoadFlags(), Entities::LoadFlags::Spawned) || player.Health() <= 0)
        {
            return;
        }
        NetPlayerBridge::RestoreReportedPosition(
            player, NetSession::RemoteIntents.at(static_cast<std::size_t>(slot)));
    }

    OpenTK::Mathematics::Vector3 NetHooks::RemoteShotOrigin(
        Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 current)
    {
        if (!NetSession::IsAuthority() || player.SlotIndex() == NetSession::LocalSlot()
            || player.SlotIndex() < 0
            || static_cast<std::size_t>(player.SlotIndex()) >= NetSession::RemoteIntents.size())
        {
            return current;
        }
        return current
            + NetSession::RemoteIntents.at(static_cast<std::size_t>(player.SlotIndex())).Position
            - player.Position;
    }

    OpenTK::Mathematics::Vector3 NetHooks::RemoteShotDirection(
        Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 current)
    {
        if (NetSession::IsAuthority() && player.SlotIndex() != NetSession::LocalSlot()
            && player.SlotIndex() >= 0
            && static_cast<std::size_t>(player.SlotIndex()) < NetSession::RemoteIntents.size())
        {
            const OpenTK::Mathematics::Vector3 aim
                = NetSession::RemoteIntents.at(
                    static_cast<std::size_t>(player.SlotIndex())).Aim;
            const float lengthSquared = aim.X * aim.X + aim.Y * aim.Y + aim.Z * aim.Z;
            if (lengthSquared > 0.0001F)
            {
                return aim.Normalized();
            }
        }
        return current;
    }

    bool NetHooks::TryApplyRemoteInput(
        Entities::PlayerEntity& player, std::int32_t slot)
    {
        if (!NetSession::Active() || slot == LocalSlot())
        {
            return false;
        }
        if (TestFlag(player.LoadFlags(), Entities::LoadFlags::Active)
            && NetSession::RemoteIntentValid.at(static_cast<std::size_t>(slot)))
        {
            if (TestFlag(player.LoadFlags(), Entities::LoadFlags::Spawned)
                && player.Health() > 0
                && !NetRoomChange::Settling()
                && NetSession::RemoteIntentAge(slot) <= StaleIntentFrames)
            {
                NetPlayerBridge::ApplyReportedPosition(
                    player, NetSession::RemoteIntents.at(static_cast<std::size_t>(slot)));
            }
            NetPlayerBridge::ApplyIntent(
                player, NetSession::RemoteIntents.at(static_cast<std::size_t>(slot)));
        }
        return true;
    }

    bool NetHooks::ForceSpawn(Entities::PlayerEntity& player)
    {
        if (MapAudit::ForceEveryone())
        {
            return true;
        }
        if (!NetSession::Active())
        {
            return false;
        }
        const std::int32_t slot = player.SlotIndex();
        if (slot < 0 || static_cast<std::size_t>(slot) >= NetSession::SlotOccupied.size())
        {
            return false;
        }
        if (!NetSession::IsHost() && !NetSession::IsAuthority())
        {
            return false;
        }
        return slot == NetSession::LocalSlot()
            || NetSession::SlotOccupied.at(static_cast<std::size_t>(slot));
    }

    void NetHooks::AfterInput(MphRead::Scene& scene)
    {
        if (!NetSession::Active())
        {
            return;
        }

        NetMatchEnd::Sync();
        NetRoomChange::Sync(scene);
        NetDiagnostics::Report(static_cast<double>(NetSession::NetFrame()) / 60.0);
        NetPlayerSetup::ApplyOnce();
        NetMatchSync::Apply();
        NetSlotManager::Sync();
        PlayerColors::Resolve();
        NetLog::Snapshot(static_cast<double>(NetSession::NetFrame()) / 60.0, scene);

        if (NetSession::IsAuthority() && NetSession::ConsumeAuthorityStateSync())
        {
            ApplyRemoteStates();
        }
        if (NetSession::LocalSlot() < 0 || !NetSession::IsClient())
        {
            return;
        }

        const std::int32_t local = NetSession::LocalSlot();
        const std::shared_ptr<Entities::PlayerEntity> player
            = local < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size())
            ? Entities::PlayerEntity::Players().at(static_cast<std::size_t>(local))
            : nullptr;
        if (player != nullptr
            && TestFlag(player->LoadFlags(), Entities::LoadFlags::Active))
        {
            if (Mods::SpectatorMode::IsSpectating())
            {
                player->ModSetSpectating(true);
            }
            if (!Mods::SpectatorMode::IsSpectating())
            {
                NetTestScript::Apply(player);
            }
            NetPlayerBridge::RecordPresses(*player);
            if (NetSession::NetFrame() % NetConfig::IntentSendInterval == 0)
            {
                NetSession::SendIntent(NetPlayerBridge::CaptureIntent(*player));
            }
        }
        else
        {
            NetSession::SendIntent(IntentPacket{});
        }
    }

    void NetHooks::AfterSimulation()
    {
        if (!NetSession::Active())
        {
            return;
        }

        for (std::int32_t i = 0;
            i < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size());
            ++i)
        {
            Entities::PlayerEntity& player = RequireReference(
                Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i)));
            if (TestFlag(player.LoadFlags(), Entities::LoadFlags::Active))
            {
                player.ModRecordNetworkPosition(NetSession::NetFrame());
                player.ModRepairVectors();
            }
        }

        if (NetSession::IsHost() || NetSession::IsAuthority())
        {
            NetSession::BroadcastSnapshot();
        }
        else if (NetSession::IsClient())
        {
            ApplyRemoteStates();
        }
    }

    void NetHooks::ApplyRemoteStates()
    {
        for (std::int32_t i = 0;
            i < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size());
            ++i)
        {
            if (!NetSession::RemoteStateValid.at(static_cast<std::size_t>(i)))
            {
                continue;
            }
            Entities::PlayerEntity& player = RequireReference(
                Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i)));
            if (TestFlag(player.LoadFlags(), Entities::LoadFlags::Active))
            {
                NetPlayerBridge::ApplyState(
                    player,
                    NetSession::RemoteStates.at(static_cast<std::size_t>(i)),
                    i == NetSession::LocalSlot());
            }
        }
        NetSession::NoteStatesApplied();
    }
}
