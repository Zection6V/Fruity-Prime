#pragma once

#include <memory>

namespace MphRead
{
    class MenuSettings;
    class RenderWindow;
}

namespace MphRead::Mods::Launcher
{
    class LaunchPlan;

    class MatchStart final
    {
    public:
        MatchStart() = delete;

        static void Launch(
            std::shared_ptr<MphRead::MenuSettings> settings,
            LaunchPlan plan);
        static void CommitAdventureSave();

    private:
        static void LaunchAdventure(LaunchPlan plan);
        static void LaunchDemo(LaunchPlan plan);
        static void AddLocalPlayers(
            MphRead::RenderWindow& renderer,
            LaunchPlan plan,
            bool teamPlay);
    };
}
