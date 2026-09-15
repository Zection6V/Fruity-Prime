#include "RespawnChoice.hpp"

#include "../Entities/Players/PlayerEntity.hpp"
#include "../Formats/Types.hpp"
#include "Network/DemoClip.hpp"
#include "Network/NetSession.hpp"
#include "Network/PlayerColors.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace MphRead::Mods::Detail
{
    // Narrow later-owner boundaries. These declarations carry no behavior;
    // their C# owners have not reached Native yet.
    [[nodiscard]] bool RespawnChoiceGameStateMultiplayer();
    [[nodiscard]] MphRead::Hunter RespawnChoiceLauncherResolveHunter(MphRead::Hunter hunter);
    [[nodiscard]] std::int32_t RespawnChoiceLauncherLastColor();
    void RespawnChoiceEndScreenClearReady();
    void RespawnChoicePlayerModSetHunter(
        Entities::PlayerEntity& player, MphRead::Hunter hunter);
}

namespace
{
    [[nodiscard]] MphRead::Entities::PlayerEntity& RequirePlayer(
        const std::shared_ptr<MphRead::Entities::PlayerEntity>& player)
    {
        if (player == nullptr)
        {
            throw System::NullReferenceException();
        }
        return *player;
    }
}

namespace MphRead::Mods
{
    std::optional<MphRead::Hunter> RespawnChoice::_hunter{};
    std::optional<std::int32_t> RespawnChoice::_color{};

    MphRead::Hunter RespawnChoice::Hunter()
    {
        if (_hunter.has_value())
        {
            return _hunter.value();
        }
        std::shared_ptr<Entities::PlayerEntity> player = Entities::PlayerEntity::Main();
        if (player != nullptr)
        {
            return player->Hunter();
        }
        return MphRead::Hunter::Samus;
    }

    std::int32_t RespawnChoice::Color()
    {
        if (_color.has_value())
        {
            return _color.value();
        }

        std::shared_ptr<Entities::PlayerEntity> firstMain = Entities::PlayerEntity::Main();
        if (firstMain != nullptr)
        {
            std::shared_ptr<Entities::PlayerEntity> secondMain = Entities::PlayerEntity::Main();
            if (RequirePlayer(secondMain).SlotIndex() >= 0)
            {
                std::shared_ptr<Entities::PlayerEntity> thirdMain = Entities::PlayerEntity::Main();
                const std::int32_t slot = RequirePlayer(thirdMain).SlotIndex();
                return Network::PlayerColors::Choice.at(static_cast<std::size_t>(slot));
            }
        }
        return Detail::RespawnChoiceLauncherLastColor();
    }

    void RespawnChoice::Reset()
    {
        _hunter.reset();
        _color.reset();
        Detail::RespawnChoiceEndScreenClearReady();
        Network::DemoClip::Purge();
    }

    void RespawnChoice::Request(MphRead::Hunter hunter, std::int32_t color)
    {
        _hunter = Detail::RespawnChoiceLauncherResolveHunter(hunter);
        _color = Network::PlayerColors::Clamp(color);
    }

    void RespawnChoice::ApplyOnSpawn(const std::shared_ptr<Entities::PlayerEntity>& player)
    {
        if (player != Entities::PlayerEntity::Main())
        {
            return;
        }
        if (!Detail::RespawnChoiceGameStateMultiplayer())
        {
            return;
        }
        if (!_hunter.has_value() && !_color.has_value())
        {
            return;
        }

        MphRead::Hunter hunter;
        if (_hunter.has_value())
        {
            hunter = _hunter.value();
        }
        else
        {
            hunter = RequirePlayer(player).Hunter();
        }

        std::int32_t color;
        if (_color.has_value())
        {
            color = _color.value();
        }
        else
        {
            color = Color();
        }

        _hunter.reset();
        _color.reset();

        if (hunter != RequirePlayer(player).Hunter())
        {
            Detail::RespawnChoicePlayerModSetHunter(RequirePlayer(player), hunter);
            RequirePlayer(player).Initialize();
        }
        if (RequirePlayer(player).SlotIndex() >= 0
            && RequirePlayer(player).SlotIndex()
                < static_cast<std::int32_t>(Network::PlayerColors::Choice.size()))
        {
            const std::int32_t slot = RequirePlayer(player).SlotIndex();
            Network::PlayerColors::Choice.at(static_cast<std::size_t>(slot)) = color;
        }
        if (Network::NetSession::Active())
        {
            Network::NetSession::SetLocalHunter(hunter);
            Network::NetSession::SetLocalColor(color);
            Network::NetSession::SendIdentify();
        }
        Network::PlayerColors::Resolve();
    }
}
