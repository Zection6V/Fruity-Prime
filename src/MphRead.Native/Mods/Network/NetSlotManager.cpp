#include "NetSlotManager.hpp"

#include "NetDamage.hpp"
#include "NetHitPrediction.hpp"
#include "NetLog.hpp"
#include "NetPlayerBridge.hpp"
#include "NetScoreboard.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

namespace MphRead::Mods::Network::Detail
{
    // NetSession is being ported independently. These declarations are the
    // narrow reads/writes this C# file performs and do not introduce session
    // state or substitute behavior.
    [[nodiscard]] bool NetSlotManagerSessionActive();
    [[nodiscard]] std::int32_t NetSlotManagerSessionLocalSlot();
    [[nodiscard]] bool NetSlotManagerSessionIsServer();
    [[nodiscard]] std::int32_t NetSlotManagerSlotOccupiedLength();
    [[nodiscard]] bool NetSlotManagerSlotOccupied(std::int32_t slot);
    [[nodiscard]] MphRead::Hunter NetSlotManagerSlotHunter(std::int32_t slot);
    void NetSlotManagerSessionForgetSlot(std::int32_t slot);

    // Direct C# dependencies whose Native owners are outside this slice.
    [[nodiscard]] bool NetSlotManagerWeaponsCurrentIsNull();
    [[nodiscard]] bool NetSlotManagerGameStateTeams();
    [[nodiscard]] std::string NetSlotManagerNicknameText(std::int32_t slot);

    // PlayerEntity has direct Native storage for these values, but the two
    // mutators below belong to contributor-owned slices rather than this file.
    void NetSlotManagerSetBotLevel(Entities::PlayerEntity& player, std::int32_t value);
    void NetSlotManagerSetHunter(Entities::PlayerEntity& player, MphRead::Hunter hunter);

    // Preserve the managed null-dereference failure without inventing a Native
    // exception type in this slice.
    [[noreturn]] void NetSlotManagerThrowNullReference();

    // C# interpolation formats numeric and enum values through the managed
    // formatting rules before Console.WriteLine/NetLog receive the string.
    [[nodiscard]] std::string NetSlotManagerFormatInt32(std::int32_t value);
    [[nodiscard]] std::string NetSlotManagerFormatHunter(MphRead::Hunter hunter);
    void NetSlotManagerConsoleWriteLine(std::string_view value);
}

namespace
{
    template <typename TEnum>
    [[nodiscard]] bool TestFlag(TEnum value, TEnum flag) noexcept
    {
        using Underlying = std::underlying_type_t<TEnum>;
        return (static_cast<Underlying>(value) & static_cast<Underlying>(flag)) != 0;
    }

    [[nodiscard]] MphRead::Entities::PlayerEntity& RequirePlayer(
        const std::shared_ptr<MphRead::Entities::PlayerEntity>& player)
    {
        if (!player)
        {
            MphRead::Mods::Network::Detail::NetSlotManagerThrowNullReference();
        }
        return *player;
    }
}

namespace MphRead::Mods::Network
{
    std::array<bool, Entities::PlayerEntity::SlotCapacity> NetSlotManager::_activated{};

    void NetSlotManager::Reset()
    {
        _activated.fill(false);
    }

    void NetSlotManager::Sync()
    {
        if (!Detail::NetSlotManagerSessionActive()
            || (Detail::NetSlotManagerSessionLocalSlot() < 0
                && !Detail::NetSlotManagerSessionIsServer()))
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

            const bool occupied = slot == Detail::NetSlotManagerSessionLocalSlot()
                || (slot < Detail::NetSlotManagerSlotOccupiedLength()
                    && Detail::NetSlotManagerSlotOccupied(slot));

            if (occupied && !_activated.at(static_cast<std::size_t>(slot)))
            {
                if (Detail::NetSlotManagerWeaponsCurrentIsNull())
                {
                    continue;
                }
                Activate(*player, slot);
            }
            else if (occupied && slot != Detail::NetSlotManagerSessionLocalSlot())
            {
                const MphRead::Hunter rosterHunter
                    = Detail::NetSlotManagerSlotHunter(slot);
                const MphRead::Hunter playerHunter = player->Hunter();
                if (rosterHunter != playerHunter)
                {
                    Detail::NetSlotManagerSetHunter(
                        *player, Detail::NetSlotManagerSlotHunter(slot));
                    player->Initialize();

                    std::string consoleMessage = "[net] slot ";
                    consoleMessage += Detail::NetSlotManagerFormatInt32(slot);
                    consoleMessage += " is playing ";
                    consoleMessage += Detail::NetSlotManagerFormatHunter(player->Hunter());
                    Detail::NetSlotManagerConsoleWriteLine(consoleMessage);

                    std::string logMessage = "slot ";
                    logMessage += Detail::NetSlotManagerFormatInt32(slot);
                    logMessage += " is playing ";
                    logMessage += Detail::NetSlotManagerFormatHunter(player->Hunter());
                    NetLog::Event(logMessage);
                }
            }
            else if (!occupied
                && _activated.at(static_cast<std::size_t>(slot))
                && slot != Detail::NetSlotManagerSessionLocalSlot())
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
        Detail::NetSlotManagerSessionForgetSlot(slot);
        NetScoreboard::ForgetSlot(slot);
        NetHitPrediction::ForgetSlot(slot);

        player.SetLoadFlags(player.LoadFlags() | Entities::LoadFlags::SlotActive);
        player.SetLoadFlags(player.LoadFlags() | Entities::LoadFlags::Active);
        player.SetLoadFlags(player.LoadFlags() | Entities::LoadFlags::Initial);
        player.SetIsBot(false);
        Detail::NetSlotManagerSetBotLevel(player, 0);

        const std::int32_t wanted = Detail::NetSlotManagerGameStateTeams()
            ? slot % 2
            : slot;
        if (player.TeamIndex() != wanted
            && (Detail::NetSlotManagerGameStateTeams()
                ? player.TeamIndex() < 0 || player.TeamIndex() > 1
                : player.TeamIndex() < 0
                    || player.TeamIndex() >= Entities::PlayerEntity::MaxPlayers()
                    || TeamIndexTaken(player.TeamIndex(), slot)))
        {
            player.SetTeamIndex(wanted);
            player.SetTeam(player.TeamIndex() % 2 == 0 ? Team::Orange : Team::Green);
        }

        if (slot != Detail::NetSlotManagerSessionLocalSlot())
        {
            const MphRead::Hunter rosterHunter = Detail::NetSlotManagerSlotHunter(slot);
            const MphRead::Hunter playerHunter = player.Hunter();
            if (rosterHunter != playerHunter)
            {
                Detail::NetSlotManagerSetHunter(
                    player, Detail::NetSlotManagerSlotHunter(slot));
            }
        }

        player.Initialize();
        Entities::PlayerEntity::SetPlayerCount(CountActive());

        std::string consoleMessage = "[net] slot ";
        consoleMessage += Detail::NetSlotManagerFormatInt32(slot);
        consoleMessage += " activated (";
        consoleMessage += Detail::NetSlotManagerNicknameText(slot);
        consoleMessage += ") -- ";
        consoleMessage += Detail::NetSlotManagerFormatInt32(
            Entities::PlayerEntity::PlayerCount());
        consoleMessage += " player(s) in scene";
        Detail::NetSlotManagerConsoleWriteLine(consoleMessage);

        std::string logMessage = "slot ";
        logMessage += Detail::NetSlotManagerFormatInt32(slot);
        logMessage += " activated (";
        logMessage += Detail::NetSlotManagerNicknameText(slot);
        logMessage += "), ";
        logMessage += Detail::NetSlotManagerFormatInt32(
            Entities::PlayerEntity::PlayerCount());
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
        Deactivate(RequirePlayer(
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
            if (RequirePlayer(
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
        Detail::NetSlotManagerSessionForgetSlot(slot);
        NetHitPrediction::ForgetSlot(slot);
        NetScoreboard::ForgetSlot(slot);

        player.SetLoadFlags(player.LoadFlags() & ~Entities::LoadFlags::Active);
        player.SetLoadFlags(player.LoadFlags() & ~Entities::LoadFlags::Spawned);
        player.SetHealth(0);
        Entities::PlayerEntity::SetPlayerCount(std::max(CountActive(), 1));

        std::string consoleMessage = "[net] slot ";
        consoleMessage += Detail::NetSlotManagerFormatInt32(slot);
        consoleMessage += " deactivated -- player left";
        Detail::NetSlotManagerConsoleWriteLine(consoleMessage);

        std::string logMessage = "slot ";
        logMessage += Detail::NetSlotManagerFormatInt32(slot);
        logMessage += " deactivated";
        NetLog::Event(logMessage);
    }
}
