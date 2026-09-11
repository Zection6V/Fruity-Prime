#include "NetPlayerSetup.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace MphRead::Mods::Network::Detail
{
    // Narrow integration boundary for the C# state this isolated port touches.
    // PlayerAt must model PlayerEntity.Players[slot] exactly: return nullptr for
    // a null slot and preserve the backing array's out-of-range failure rather
    // than clamping or synthesizing a slot. The returned opaque handle denotes
    // that one PlayerEntity reference for the remainder of the iteration.
    struct NetPlayerSetupPlayer;

    bool NetPlayerSetupSessionActive();
    std::int32_t NetPlayerSetupLocalSlot();
    bool NetPlayerSetupIsServer();

    std::int32_t NetPlayerSetupMaxPlayers();
    void NetPlayerSetupSetMainPlayerIndex(std::int32_t value);
    NetPlayerSetupPlayer* NetPlayerSetupPlayerAt(std::int32_t slot);
    void NetPlayerSetupSetIsBot(NetPlayerSetupPlayer& player, bool value);
    void NetPlayerSetupSetBotLevel(NetPlayerSetupPlayer& player, std::int32_t value);
    std::uint8_t NetPlayerSetupLoadFlags(const NetPlayerSetupPlayer& player);

    // Models the single Console.WriteLine call; the supplied string excludes
    // the line terminator, just as the C# string argument does.
    void NetPlayerSetupConsoleWriteLine(std::string_view value);
}

namespace
{
    constexpr std::uint8_t LoadFlagsActive = 0x20;
}

namespace MphRead::Mods::Network
{
    bool NetPlayerSetup::_applied = false;

    void NetPlayerSetup::Reset()
    {
        _applied = false;
    }

    void NetPlayerSetup::ApplyOnce()
    {
        if (_applied || !Detail::NetPlayerSetupSessionActive())
        {
            return;
        }
        if (Detail::NetPlayerSetupLocalSlot() < 0 && !Detail::NetPlayerSetupIsServer())
        {
            return;
        }
        _applied = true;

        const std::int32_t local = Detail::NetPlayerSetupLocalSlot();
        if (local >= 0 && local < Detail::NetPlayerSetupMaxPlayers())
        {
            Detail::NetPlayerSetupSetMainPlayerIndex(local);
        }
        for (std::int32_t slot = 0; slot < Detail::NetPlayerSetupMaxPlayers(); slot++)
        {
            Detail::NetPlayerSetupPlayer* player = Detail::NetPlayerSetupPlayerAt(slot);
            if (player == nullptr)
            {
                continue;
            }
            if (slot == local)
            {
                Detail::NetPlayerSetupSetIsBot(*player, false);
                continue;
            }
            Detail::NetPlayerSetupSetIsBot(*player, false);
            Detail::NetPlayerSetupSetBotLevel(*player, 0);
        }

        std::string message = "[net] player slots prepared -- local slot ";
        message += std::to_string(local);
        message += ", ";
        message += std::to_string(CountActive());
        message += " active, AI disabled on remote slots";
        Detail::NetPlayerSetupConsoleWriteLine(message);
    }

    std::int32_t NetPlayerSetup::CountActive()
    {
        std::int32_t count = 0;
        for (std::int32_t i = 0; i < Detail::NetPlayerSetupMaxPlayers(); i++)
        {
            Detail::NetPlayerSetupPlayer* player = Detail::NetPlayerSetupPlayerAt(i);
            if (player != nullptr
                && (Detail::NetPlayerSetupLoadFlags(*player) & LoadFlagsActive) != 0)
            {
                count++;
            }
        }
        return count;
    }
}
