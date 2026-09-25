#include "NetSlotManager.hpp"

#include "../../GameState.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "NetDamage.hpp"
#include "NetHitPrediction.hpp"
#include "NetLog.hpp"
#include "NetPlayerBridge.hpp"
#include "NetScoreboard.hpp"
#include "NetSession.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::TestFlag;

namespace MphRead::Mods::Network
{
    std::array<bool, Entities::PlayerEntity::SlotCapacity> NetSlotManager::_activated{};

    void NetSlotManager::Reset()
    {
        _activated.fill(false);
    }

    void NetSlotManager::Sync()
    {
        if (!NetSession::Active()
            || (NetSession::LocalSlot() < 0
                && !NetSession::IsServer()))
        {
            return;
        }

        for (std::int32_t slot = 0; slot < Entities::PlayerEntity::MaxPlayers(); slot++)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = slot < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size())
                ? Entities::PlayerEntity::Players().at(static_cast<std::size_t>(slot))
                : nullptr;
            if (player == nullptr)
            {
                continue;
            }

            const bool occupied = slot == NetSession::LocalSlot()
                || (slot < static_cast<std::int32_t>(NetSession::SlotOccupied.size())
                    && NetSession::SlotOccupied[slot]);

            if (occupied && !_activated.at(static_cast<std::size_t>(slot)))
            {
                if ((Weapons::Current == nullptr))
                {
                    continue;
                }
                Activate(*player, slot);
            }
            else if (occupied && slot != NetSession::LocalSlot())
            {
                const MphRead::Hunter rosterHunter
                    = NetSession::SlotHunter[slot];
                const MphRead::Hunter playerHunter = player->Hunter();
                if (rosterHunter != playerHunter)
                {
                    player->ModSetHunter(NetSession::SlotHunter[slot]);
                    player->Initialize();

                    std::string consoleMessage = "[net] slot ";
                    consoleMessage += ::MphRead::NativeRuntime::ToString(slot);
                    consoleMessage += " is playing ";
                    consoleMessage += ::MphRead::ToString(player->Hunter());
                    NativeRuntime::ConsoleWriteLine(consoleMessage);

                    std::string logMessage = "slot ";
                    logMessage += ::MphRead::NativeRuntime::ToString(slot);
                    logMessage += " is playing ";
                    logMessage += ::MphRead::ToString(player->Hunter());
                    NetLog::Event(logMessage);
                }
            }
            else if (!occupied
                && _activated.at(static_cast<std::size_t>(slot))
                && slot != NetSession::LocalSlot())
            {
                Deactivate(*player, slot);
            }
        }
    }

    void NetSlotManager::Activate(Entities::PlayerEntity& player, std::int32_t slot)
    {
        _activated.at(static_cast<std::size_t>(slot)) = true;

        NetPlayerBridge::ForgetSlot(slot);
        NetDamage::ForgetSlot(slot);
        NetSession::ForgetSlot(slot);
        NetScoreboard::ForgetSlot(slot);
        NetHitPrediction::ForgetSlot(slot);

        player.SetLoadFlags(player.LoadFlags() | Entities::LoadFlags::SlotActive);
        player.SetLoadFlags(player.LoadFlags() | Entities::LoadFlags::Active);
        player.SetLoadFlags(player.LoadFlags() | Entities::LoadFlags::Initial);
        player.SetIsBot(false);
        player.SetBotLevel(0);

        const std::int32_t wanted = GameState::Teams()
            ? slot % 2
            : slot;
        if (player.TeamIndex() != wanted
            && (GameState::Teams()
                ? player.TeamIndex() < 0 || player.TeamIndex() > 1
                : player.TeamIndex() < 0
                    || player.TeamIndex() >= Entities::PlayerEntity::MaxPlayers()
                    || TeamIndexTaken(player.TeamIndex(), slot)))
        {
            player.SetTeamIndex(wanted);
            player.SetTeam(player.TeamIndex() % 2 == 0 ? Team::Orange : Team::Green);
        }

        if (slot != NetSession::LocalSlot())
        {
            const MphRead::Hunter rosterHunter = NetSession::SlotHunter[slot];
            const MphRead::Hunter playerHunter = player.Hunter();
            if (rosterHunter != playerHunter)
            {
                player.ModSetHunter(NetSession::SlotHunter[slot]);
            }
        }

        player.Initialize();
        Entities::PlayerEntity::SetPlayerCount(CountActive());

        std::string consoleMessage = "[net] slot ";
        consoleMessage += ::MphRead::NativeRuntime::ToString(slot);
        consoleMessage += " activated (";
        consoleMessage += GameState::Nicknames()[slot];
        consoleMessage += ") -- ";
        consoleMessage += ::MphRead::NativeRuntime::ToString(Entities::PlayerEntity::PlayerCount());
        consoleMessage += " player(s) in scene";
        NativeRuntime::ConsoleWriteLine(consoleMessage);

        std::string logMessage = "slot ";
        logMessage += ::MphRead::NativeRuntime::ToString(slot);
        logMessage += " activated (";
        logMessage += GameState::Nicknames()[slot];
        logMessage += "), ";
        logMessage += ::MphRead::NativeRuntime::ToString(Entities::PlayerEntity::PlayerCount());
        logMessage += " player(s) in scene";
        NetLog::Event(logMessage);
    }

    std::int32_t NetSlotManager::CountActive()
    {
        std::int32_t count = 0;
        for (std::int32_t i = 0; i < Entities::PlayerEntity::MaxPlayers(); i++)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = i < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size())
                ? Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i))
                : nullptr;
            if (player != nullptr
                && TestFlag(player->LoadFlags(), Entities::LoadFlags::Active))
            {
                count++;
            }
        }
        return count;
    }

    void NetSlotManager::ReleaseSlot(std::int32_t slot)
    {
        if (slot < 0
            || slot >= Entities::PlayerEntity::SlotCapacity
            || slot >= static_cast<std::int32_t>(Entities::PlayerEntity::Players().size())
            || !_activated.at(static_cast<std::size_t>(slot)))
        {
            return;
        }
        Deactivate(RequireReference(
            Entities::PlayerEntity::Players().at(static_cast<std::size_t>(slot))), slot);
    }

    bool NetSlotManager::TeamIndexTaken(std::int32_t teamIndex, std::int32_t slot)
    {
        for (std::int32_t i = 0;
            i < Entities::PlayerEntity::MaxPlayers()
                && i < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size());
            i++)
        {
            if (i == slot || !_activated.at(static_cast<std::size_t>(i)))
            {
                continue;
            }
            if (RequireReference(
                Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i))).TeamIndex()
                == teamIndex)
            {
                return true;
            }
        }
        return false;
    }

    void NetSlotManager::Deactivate(Entities::PlayerEntity& player, std::int32_t slot)
    {
        _activated.at(static_cast<std::size_t>(slot)) = false;

        NetPlayerBridge::ForgetSlot(slot);
        NetDamage::ForgetSlot(slot);
        NetSession::ForgetSlot(slot);
        NetHitPrediction::ForgetSlot(slot);
        NetScoreboard::ForgetSlot(slot);

        player.SetLoadFlags(player.LoadFlags() & ~Entities::LoadFlags::Active);
        player.SetLoadFlags(player.LoadFlags() & ~Entities::LoadFlags::Spawned);
        player.SetHealth(0);
        Entities::PlayerEntity::SetPlayerCount(std::max(CountActive(), 1));

        std::string consoleMessage = "[net] slot ";
        consoleMessage += ::MphRead::NativeRuntime::ToString(slot);
        consoleMessage += " deactivated -- player left";
        NativeRuntime::ConsoleWriteLine(consoleMessage);

        std::string logMessage = "slot ";
        logMessage += ::MphRead::NativeRuntime::ToString(slot);
        logMessage += " deactivated";
        NetLog::Event(logMessage);
    }
}
