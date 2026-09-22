#include "NetPlayerSetup.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "NetSession.hpp"

#include <cstdint>
#include <string>
#include <string_view>

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
        if (_applied || !NetSession::Active())
        {
            return;
        }
        if (NetSession::LocalSlot() < 0 && !NetSession::IsServer())
        {
            return;
        }
        _applied = true;

        const std::int32_t local = NetSession::LocalSlot();
        if (local >= 0 && local < Entities::PlayerEntity::MaxPlayers())
        {
            Entities::PlayerEntity::SetMainPlayerIndex(local);
        }
        for (std::int32_t slot = 0; slot < Entities::PlayerEntity::MaxPlayers(); slot++)
        {
            const std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(slot));
            if (player == nullptr)
            {
                continue;
            }
            if (slot == local)
            {
                player->SetIsBot(false);
                continue;
            }
            player->SetIsBot(false);
            player->SetBotLevel(0);
        }

        std::string message = "[net] player slots prepared -- local slot ";
        message += NativeRuntime::Int32ToString(local);
        message += ", ";
        message += NativeRuntime::Int32ToString(CountActive());
        message += " active, AI disabled on remote slots";
        NativeRuntime::ConsoleWriteLine(message);
    }

    std::int32_t NetPlayerSetup::CountActive()
    {
        std::int32_t count = 0;
        for (std::int32_t i = 0; i < Entities::PlayerEntity::MaxPlayers(); i++)
        {
            const std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i));
            if (player != nullptr
                && (static_cast<std::uint8_t>(player->LoadFlags()) & LoadFlagsActive) != 0)
            {
                count++;
            }
        }
        return count;
    }
}
