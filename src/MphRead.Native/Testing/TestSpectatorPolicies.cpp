#include "Mods/Network/Gameplay/Player/PlayerCombatPolicy.hpp"
#include "Mods/SpectatorTargetPolicy.hpp"

#include <array>
#include <iostream>
#include <span>
#include <string_view>

namespace
{
    bool Check(bool condition, std::string_view name)
    {
        if (!condition)
        {
            std::cerr << "FAIL " << name << '\n';
            return false;
        }
        return true;
    }
}

int main()
{
    using namespace MphRead::Mods;
    using MphRead::Mods::Network::PlayerCombatPolicy;
    bool passed = true;

    constexpr SpectatorSlotSnapshot alive{true, true, true, false, true, 100, 4, 8};
    passed &= Check(SpectatorTargetPolicy::IsEligible(alive, 1, 0, true),
        "an occupied, live network player can be followed");
    passed &= Check(!SpectatorTargetPolicy::IsEligible(alive, 0, 0, true),
        "the local player is never selected as a remote target");

    auto invalid = alive;
    invalid.Active = false;
    passed &= Check(!SpectatorTargetPolicy::IsEligible(invalid, 1, 0, true),
        "inactive slots cannot be followed");
    invalid = alive;
    invalid.Spawned = false;
    passed &= Check(!SpectatorTargetPolicy::IsEligible(invalid, 1, 0, true),
        "unspawned slots cannot be followed");
    invalid = alive;
    invalid.Health = 0;
    passed &= Check(!SpectatorTargetPolicy::IsEligible(invalid, 1, 0, true),
        "dead players cannot be followed");
    invalid = alive;
    invalid.Spectating = true;
    passed &= Check(!SpectatorTargetPolicy::IsEligible(invalid, 1, 0, true),
        "another spectator is not a combat target");
    invalid = alive;
    invalid.RosterOccupied = false;
    passed &= Check(!SpectatorTargetPolicy::IsEligible(invalid, 1, 0, true),
        "a stale entity outside the current roster cannot be followed");
    invalid = alive;
    invalid.Generation = 0;
    passed &= Check(!SpectatorTargetPolicy::IsEligible(invalid, 1, 0, true),
        "an unassigned network generation cannot be followed");
    invalid = alive;
    invalid.LifeId = 0;
    passed &= Check(!SpectatorTargetPolicy::IsEligible(invalid, 1, 0, true),
        "a network slot without a current life cannot be followed");
    invalid = alive;
    invalid.RosterOccupied = false;
    passed &= Check(SpectatorTargetPolicy::IsEligible(invalid, 1, 0, false),
        "offline multiplayer does not require a network roster");

    constexpr std::array<SpectatorSlotSnapshot, 4> slots{{
        {}, alive, {}, SpectatorSlotSnapshot{true, true, true, false, true, 100, 9, 2}}};
    constexpr std::array<SpectatorSlotSnapshot, 4> onlyCurrent{{{}, alive, {}, {}}};
    passed &= Check(SpectatorTargetPolicy::Find({}, 0, 0, true, true) == -1,
        "empty player lists are safe");
    passed &= Check(SpectatorTargetPolicy::Find(slots, 1, 0, true, true) == 3,
        "next-target search wraps past the current slot");
    passed &= Check(SpectatorTargetPolicy::Find(slots, 3, 0, true, false) == 1,
        "previous-target search wraps past the current slot");
    passed &= Check(SpectatorTargetPolicy::Find(onlyCurrent, 1, 0, true, false) == -1,
        "search does not reselect its only current target");
    passed &= Check(SpectatorTargetPolicy::Find(slots, -1, 0, true, true) == 1,
        "negative next-search origins normalize safely");
    passed &= Check(SpectatorTargetPolicy::Find(slots, 99, 0, true, false) == 3,
        "out-of-range previous-search origins normalize safely");

    constexpr SpectatorTargetIdentity identity{1, 4, 8};
    passed &= Check(SpectatorTargetPolicy::Matches(identity, alive, 1, 0, true),
        "the tracked target matches its current roster generation and life");
    auto reused = alive;
    reused.Generation = 5;
    passed &= Check(!SpectatorTargetPolicy::Matches(identity, reused, 1, 0, true),
        "a reused slot does not revive an old target generation");
    reused = alive;
    reused.LifeId = 9;
    passed &= Check(!SpectatorTargetPolicy::Matches(identity, reused, 1, 0, true),
        "a respawn does not revive an old target life");

    passed &= Check(PlayerCombatPolicy::IsInPlay(true, true, 100, false),
        "active spawned living players participate in combat");
    passed &= Check(!PlayerCombatPolicy::IsInPlay(false, true, 100, false)
        && !PlayerCombatPolicy::IsInPlay(true, false, 100, false)
        && !PlayerCombatPolicy::IsInPlay(true, true, 0, false)
        && !PlayerCombatPolicy::IsInPlay(true, true, 100, true),
        "inactive, unspawned, dead, and spectating players are outside combat");
    passed &= Check(!PlayerCombatPolicy::IntentIsInPlay(true, true)
        && !PlayerCombatPolicy::IntentIsInPlay(false, false)
        && PlayerCombatPolicy::IntentIsInPlay(true, false),
        "spectating input safely overrides a contradictory in-play bit");
    passed &= Check(!PlayerCombatPolicy::CanReceiveDamage(true)
        && PlayerCombatPolicy::CanReceiveDamage(false),
        "spectating players reject damage at the shared final gate");

    return passed ? 0 : 1;
}
