#include "PlayerColors.hpp"

#include "../../Entities/Players/HalfturretEntity.hpp"
#include "NetLog.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace MphRead::Mods::Network::Detail
{
    // Narrow later-owner boundary for GameState.Teams.
    [[nodiscard]] bool PlayerColorsGameStateTeams();

    // C# interpolated strings format these values through the framework.
    [[nodiscard]] std::string PlayerColorsFormatInt32(std::int32_t value);
    [[nodiscard]] std::string PlayerColorsFormatHunter(MphRead::Hunter value);

    // Models the single Console.WriteLine call. The supplied string excludes
    // the line terminator, just as the C# string argument does.
    void PlayerColorsConsoleWriteLine(std::string_view value);
}

namespace MphRead::Mods::Network
{
    std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity> PlayerColors::Choice{};

    std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity> PlayerColors::_applied
        = PlayerColors::CreateApplied();

    std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity> PlayerColors::CreateApplied()
    {
        std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity> applied{};
        applied.fill(-1);
        return applied;
    }

    void PlayerColors::Reset()
    {
        Choice.fill(0);
        _applied.fill(-1);
    }

    std::int32_t PlayerColors::Clamp(std::int32_t color)
    {
        return color < 0 || color >= Count ? 0 : color;
    }

    void PlayerColors::Resolve()
    {
        if (Detail::PlayerColorsGameStateTeams())
        {
            return;
        }
        for (std::int32_t slot = 0;
            slot < Entities::PlayerEntity::MaxPlayers()
                && slot < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size());
            slot++)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players()[static_cast<std::size_t>(slot)];
            if (player == nullptr
                || (player->LoadFlags() & Entities::LoadFlags::Active) == Entities::LoadFlags::None)
            {
                continue;
            }
            const std::int32_t want = Clamp(Choice[static_cast<std::size_t>(slot)]);
            std::int32_t color = want;
            for (std::int32_t step = 0; step < Count; step++)
            {
                color = (want + step) % Count;
                if (!TakenBefore(slot, player->Hunter(), color))
                {
                    break;
                }
            }
            player->SetRecolor(color);
            if (_applied[static_cast<std::size_t>(slot)] != color)
            {
                _applied[static_cast<std::size_t>(slot)] = color;

                std::string consoleMessage = "[net] slot ";
                consoleMessage += Detail::PlayerColorsFormatInt32(slot);
                consoleMessage += " (";
                consoleMessage += Detail::PlayerColorsFormatHunter(player->Hunter());
                consoleMessage += ") wears suit ";
                consoleMessage += Detail::PlayerColorsFormatInt32(color + 1);
                if (color != want)
                {
                    consoleMessage += " -- asked for ";
                    consoleMessage += Detail::PlayerColorsFormatInt32(want + 1);
                }
                Detail::PlayerColorsConsoleWriteLine(consoleMessage);

                std::string logMessage = "slot ";
                logMessage += Detail::PlayerColorsFormatInt32(slot);
                logMessage += " ";
                logMessage += Detail::PlayerColorsFormatHunter(player->Hunter());
                logMessage += " suit ";
                logMessage += Detail::PlayerColorsFormatInt32(color + 1);
                logMessage += " (asked ";
                logMessage += Detail::PlayerColorsFormatInt32(want + 1);
                logMessage += ")";
                NetLog::Event(logMessage);
            }
            if (player->Halfturret() != nullptr)
            {
                player->Halfturret()->SetRecolor(color);
            }
        }
    }

    bool PlayerColors::TakenBefore(
        std::int32_t slot, MphRead::Hunter hunter, std::int32_t color)
    {
        for (std::int32_t i = 0; i < slot; i++)
        {
            std::shared_ptr<Entities::PlayerEntity> other
                = Entities::PlayerEntity::Players()[static_cast<std::size_t>(i)];
            if (other != nullptr
                && (other->LoadFlags() & Entities::LoadFlags::Active) != Entities::LoadFlags::None
                && other->Hunter() == hunter
                && other->Recolor() == color)
            {
                return true;
            }
        }
        return false;
    }
}
