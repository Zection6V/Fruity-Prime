// GameState.cs is a C# partial type; this is GameStateTeams.cs's half of it.
// GameState.hpp is the canonical declaration owner and declares these members.

#include "../../GameState.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace
{
    using MphRead::Entities::LoadFlags;
    using MphRead::Entities::PlayerEntity;

    template <typename T>
    [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename TContainer>
    [[nodiscard]] decltype(auto) ManagedAt(TContainer&& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return std::forward<TContainer>(values)[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] bool TestFlag(LoadFlags value, LoadFlags flag) noexcept
    {
        using U = std::underlying_type_t<LoadFlags>;
        return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
    }

    // int.CompareTo(int).
    [[nodiscard]] constexpr std::int32_t CompareTo(
        std::int32_t left, std::int32_t right) noexcept
    {
        return left < right ? -1 : (left > right ? 1 : 0);
    }

    [[nodiscard]] PlayerEntity& PlayerAt(std::int32_t slot)
    {
        return RequireReference(ManagedAt(PlayerEntity::Players(), slot));
    }
}

namespace MphRead
{
    bool GameState::IsResultTie()
    {
        if (ActivePlayers() == 0)
        {
            return false;
        }
        const std::int32_t leader = ManagedAt(ResultSlots(), 0);
        for (std::int32_t i = 1; i < ActivePlayers(); ++i)
        {
            const std::int32_t slot = ManagedAt(ResultSlots(), i);
            if (ManagedAt(Standings(), slot) == 0
                && (!Teams() || PlayerAt(slot).TeamIndex() != PlayerAt(leader).TeamIndex()))
            {
                return true;
            }
        }
        return false;
    }

    void GameState::UpdateStandings()
    {
        ActivePlayers(0);
        std::array<bool, 4> represented{};
        for (std::int32_t slot = 0; slot < PlayerEntity::SlotCapacity; ++slot)
        {
            ManagedAt(Standings(), slot) = PlayerEntity::SlotCapacity - 1;
            ManagedAt(TeamStandings(), slot) = PlayerEntity::SlotCapacity - 1;
            PlayerEntity& player = PlayerAt(slot);
            if (!TestFlag(player.LoadFlags(), LoadFlags::Active)
                || static_cast<std::uint32_t>(player.TeamIndex())
                    >= static_cast<std::uint32_t>(
                        Teams() ? TeamCount() : PlayerEntity::SlotCapacity))
            {
                continue;
            }
            ManagedAt(ResultSlots(), ActivePlayers()) = slot;
            ActivePlayers(ActivePlayers() + 1);
            if (Teams())
            {
                represented[static_cast<std::size_t>(player.TeamIndex())] = true;
            }
        }
        for (std::int32_t i = 0; i < ActivePlayers(); ++i)
        {
            for (std::int32_t j = i + 1; j < ActivePlayers(); ++j)
            {
                const std::int32_t first = ManagedAt(ResultSlots(), i);
                const std::int32_t second = ManagedAt(ResultSlots(), j);
                const std::int32_t firstTeam = PlayerAt(first).TeamIndex();
                const std::int32_t secondTeam = PlayerAt(second).TeamIndex();
                std::int32_t compare = 0;
                if (Teams() && firstTeam != secondTeam)
                {
                    compare = CompareTeams(firstTeam, secondTeam);
                    if (compare == 0)
                    {
                        compare = CompareTo(secondTeam, firstTeam);
                    }
                }
                else
                {
                    compare = ComparePlayers(first, second);
                    if (compare == 0)
                    {
                        compare = CompareTo(second, first);
                    }
                }
                if (compare < 0)
                {
                    ManagedAt(ResultSlots(), i) = second;
                    ManagedAt(ResultSlots(), j) = first;
                }
            }
        }
        for (std::int32_t i = 0; i < ActivePlayers(); ++i)
        {
            const std::int32_t slot = ManagedAt(ResultSlots(), i);
            const std::int32_t team = PlayerAt(slot).TeamIndex();
            std::int32_t rank = 0;
            std::int32_t memberRank = 0;
            if (Teams())
            {
                for (std::int32_t other = 0; other < TeamCount(); ++other)
                {
                    if (represented[static_cast<std::size_t>(other)]
                        && CompareTeams(other, team) > 0)
                    {
                        ++rank;
                    }
                }
            }
            for (std::int32_t j = 0; j < ActivePlayers(); ++j)
            {
                const std::int32_t other = ManagedAt(ResultSlots(), j);
                if (ComparePlayers(other, slot) <= 0)
                {
                    continue;
                }
                if (!Teams())
                {
                    ++rank;
                }
                else if (PlayerAt(other).TeamIndex() == team)
                {
                    ++memberRank;
                }
            }
            ManagedAt(Standings(), slot) = rank;
            ManagedAt(TeamStandings(), slot) = memberRank;
        }
    }
}
