#pragma once

#include <functional>
#include <string_view>

namespace MphRead::Mods::Multiplayer
{
    // Asset-free scoring regressions; rendered combat/objectives still require
    // map tests.
    class TeamGameplayTest final
    {
    public:
        TeamGameplayTest() = delete;
        ~TeamGameplayTest() = delete;
        TeamGameplayTest(const TeamGameplayTest&) = delete;
        TeamGameplayTest& operator=(const TeamGameplayTest&) = delete;
        TeamGameplayTest(TeamGameplayTest&&) = delete;
        TeamGameplayTest& operator=(TeamGameplayTest&&) = delete;

        static void Run(const std::function<void(bool, std::string_view)>& check);
    };
}
