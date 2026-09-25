#include "NetDiagnostics.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Formats/Enums.hpp"
#include "../../GameState.hpp"
#include "NetDamage.hpp"
#include "NetPlayerBridge.hpp"
#include "NetProtocol.hpp"
#include "NetSession.hpp"
#include "../../Formats/Types.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#endif

using ::MphRead::NativeRuntime::EnvironmentGetVariable;
using ::MphRead::NativeRuntime::HasFlag;

namespace
{
    [[nodiscard]] std::string NetRoleToString(MphRead::Mods::Network::NetRole value)
    {
        using MphRead::Mods::Network::NetRole;
        switch (value)
        {
        case NetRole::Offline:
            return "Offline";
        case NetRole::Host:
            return "Host";
        case NetRole::Client:
            return "Client";
        case NetRole::Server:
            return "Server";
        }
        return ::MphRead::NativeRuntime::ToString(static_cast<std::int32_t>(value));
    }

    [[nodiscard]] std::string BeamTypeToString(MphRead::BeamType value)
    {
        switch (value)
        {
        case MphRead::BeamType::None:
            return "None";
        case MphRead::BeamType::PowerBeam:
            return "PowerBeam";
        case MphRead::BeamType::VoltDriver:
            return "VoltDriver";
        case MphRead::BeamType::Missile:
            return "Missile";
        case MphRead::BeamType::Battlehammer:
            return "Battlehammer";
        case MphRead::BeamType::Imperialist:
            return "Imperialist";
        case MphRead::BeamType::Judicator:
            return "Judicator";
        case MphRead::BeamType::Magmaul:
            return "Magmaul";
        case MphRead::BeamType::ShockCoil:
            return "ShockCoil";
        case MphRead::BeamType::OmegaCannon:
            return "OmegaCannon";
        case MphRead::BeamType::Platform:
            return "Platform";
        case MphRead::BeamType::Enemy:
            return "Enemy";
        }
        return ::MphRead::NativeRuntime::ToString(static_cast<std::int32_t>(value));
    }
}

namespace MphRead::Mods::Network
{
    bool NetDiagnostics::Enabled()
    {
        if (!_checked)
        {
            _checked = true;
            _enabled = EnvironmentGetVariable("MPHREAD_NET_DEBUG").has_value();
        }
        return _enabled;
    }

    void NetDiagnostics::SetEnabled(bool value) noexcept
    {
        _checked = true;
        _enabled = value;
    }

    void NetDiagnostics::Report(double time)
    {
        if (!Enabled() || !NetSession::Active())
        {
            return;
        }
        if (time - _lastReport < 1.0)
        {
            return;
        }
        _lastReport = time;

        std::string line;
        line += "[netdbg] role=";
        line += NetRoleToString(NetSession::Role());
        line += " slot=";
        line += ::MphRead::NativeRuntime::ToString(NetSession::LocalSlot());

        std::int32_t active = 0;
        std::int32_t created = 0;
        line += " slots=[";
        for (std::int32_t i = 0; i < Entities::PlayerEntity::MaxPlayers(); i++)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i));
            if (player == nullptr)
            {
                line.push_back('-');
                continue;
            }
            created++;
            const bool isActive = HasFlag(player->LoadFlags(), Entities::LoadFlags::Active);
            if (isActive)
            {
                active++;
            }
            line.push_back(isActive
                ? (player->IsBot() ? 'B' : 'A')
                : HasFlag(player->LoadFlags(), Entities::LoadFlags::SlotActive) ? 's' : '.');
        }
        line += "] active=";
        line += ::MphRead::NativeRuntime::ToString(active);
        line += " scoreboard=";
        line += ::MphRead::NativeRuntime::ToString(GameState::ActivePlayers());
        line += " created=";
        line += ::MphRead::NativeRuntime::ToString(created);

        line += " remoteState=[";
        for (std::size_t i = 0; i < NetSession::RemoteStateValid.size(); i++)
        {
            line.push_back(NetSession::RemoteStateValid[i] ? 'y' : 'n');
        }
        line += "] remoteIntent=[";
        for (std::size_t i = 0; i < NetSession::RemoteIntentValid.size(); i++)
        {
            line.push_back(NetSession::RemoteIntentValid[i] ? 'y' : 'n');
        }
        line.push_back(']');

        std::int32_t botRemotes = 0;
        for (std::int32_t i = 0; i < Entities::PlayerEntity::MaxPlayers(); i++)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i));
            if (player != nullptr && i != NetSession::LocalSlot() && player->IsBot()
                && HasFlag(player->LoadFlags(), Entities::LoadFlags::Active))
            {
                botRemotes++;
            }
        }
        if (botRemotes > 0)
        {
            line += "  !! ";
            line += ::MphRead::NativeRuntime::ToString(botRemotes);
            line += " remote slot(s) still AI-driven";
        }

        line += " team=[";
        for (std::int32_t i = 0; i < Entities::PlayerEntity::MaxPlayers(); i++)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i));
            if (i > 0)
            {
                line.push_back(',');
            }
            line += player == nullptr
                ? "-"
                : ::MphRead::NativeRuntime::ToString(player->TeamIndex());
        }
        line.push_back(']');

        if (!GameState::Teams())
        {
            std::int32_t shared = 0;
            for (std::int32_t i = 0; i < Entities::PlayerEntity::MaxPlayers(); i++)
            {
                std::shared_ptr<Entities::PlayerEntity> a
                    = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i));
                if (a == nullptr
                    || !HasFlag(a->LoadFlags(), Entities::LoadFlags::Active))
                {
                    continue;
                }
                for (std::int32_t j = i + 1; j < Entities::PlayerEntity::MaxPlayers(); j++)
                {
                    std::shared_ptr<Entities::PlayerEntity> b
                        = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(j));
                    if (b != nullptr
                        && HasFlag(b->LoadFlags(), Entities::LoadFlags::Active)
                        && a->TeamIndex() == b->TeamIndex())
                    {
                        shared++;
                    }
                }
            }
            if (shared > 0)
            {
                line += "  !! ";
                line += ::MphRead::NativeRuntime::ToString(shared);
                line += " pair(s) share a team index in a free-for-all: "
                    "bombs and life drain will do nothing between them";
            }
        }

        line += " alt=[";
        for (std::int32_t i = 0; i < Entities::PlayerEntity::MaxPlayers(); i++)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i));
            line.push_back(player == nullptr
                    || !HasFlag(player->LoadFlags(), Entities::LoadFlags::Active)
                ? '-'
                : player->IsAltForm() ? 'A'
                : player->IsMorphing() ? 'm'
                : player->IsUnmorphing() ? 'u'
                : 'b');
        }
        line += "] altSaid=[";
        line += NetPlayerBridge::FormSaidByAuthority();
        line.push_back(']');
        line += " shockcoil=";
        line += ::MphRead::NativeRuntime::ToString(NetDamage::ShockCoilAcquired);
        line.push_back('/');
        line += ::MphRead::NativeRuntime::ToString(NetDamage::ShockCoilSpawned);
        line += " bomb=";
        line += ::MphRead::NativeRuntime::ToString(NetDamage::BombHits);
        line.push_back('/');
        line += ::MphRead::NativeRuntime::ToString(NetDamage::BombPlayerChecks);
        line += " bombTeamSkips=";
        line += ::MphRead::NativeRuntime::ToString(NetDamage::BombTeamSkips);

        line += " dmg[";
        bool first = true;
        for (std::size_t i = 0; i < NetDamage::HitsByBeam.size(); i++)
        {
            if (NetDamage::HitsByBeam[i] == 0)
            {
                continue;
            }
            if (!first)
            {
                line.push_back(' ');
            }
            first = false;
            line += BeamTypeToString(static_cast<MphRead::BeamType>(i));
            line.push_back('=');
            line += ::MphRead::NativeRuntime::ToString(NetDamage::DamageByBeam[i]);
            line.push_back('/');
            line += ::MphRead::NativeRuntime::ToString(NetDamage::HitsByBeam[i]);
        }
        if (NetDamage::BombDamageHits > 0)
        {
            if (!first)
            {
                line.push_back(' ');
            }
            line += "Bomb=";
            line += ::MphRead::NativeRuntime::ToString(NetDamage::BombDamageDealt);
            line.push_back('/');
            line += ::MphRead::NativeRuntime::ToString(NetDamage::BombDamageHits);
        }
        line.push_back(']');

        line += " bombSpawn=";
        line += ::MphRead::NativeRuntime::ToString(NetDamage::BombSpawnMade);
        line.push_back('/');
        line += ::MphRead::NativeRuntime::ToString(NetDamage::BombSpawnCalls);
        line += " det=";
        line += ::MphRead::NativeRuntime::ToString(NetDamage::BombSpawnDetonated);
        line += " stale=";
        line += ::MphRead::NativeRuntime::ToString(NetDamage::BombSpawnStaleCount);
        line += " poolEmpty=";
        line += ::MphRead::NativeRuntime::ToString(NetDamage::BombSpawnPoolEmpty);
        line += " bombNearest=";
        line += NetDamage::BombNearest == std::numeric_limits<float>::max()
            ? "n/a"
            : ::MphRead::NativeRuntime::ToString(NetDamage::BombNearest, "0.00");
        line += " bombRadius=";
        line += ::MphRead::NativeRuntime::ToString(NetDamage::BombRadiusSeen, "0.00");

        if (NetPlayerBridge::PlacementsRefused > 0)
        {
            line += " placementsRefused=";
            line += ::MphRead::NativeRuntime::ToString(NetPlayerBridge::PlacementsRefused);
        }

        const std::optional<MatchStatePacket> match = NetSession::ServerMatch();
        if (match.has_value())
        {
            line += " serverMap=";
            if (match->RoomKey.has_value())
            {
                line += *match->RoomKey;
            }
            line += " serverPlayers=";
            line += ::MphRead::NativeRuntime::ToString(match->PlayerCount);
        }
        std::cout << line << '\n';
    }
}
