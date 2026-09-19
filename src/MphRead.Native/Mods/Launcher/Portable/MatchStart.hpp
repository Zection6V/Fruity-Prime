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
            const std::shared_ptr<MphRead::MenuSettings>& settings,
            const LaunchPlan& plan);
        static void CommitAdventureSave();

    private:
        static void LaunchAdventure(const LaunchPlan& plan);
        static void LaunchDemo(const LaunchPlan& plan);
        static void AddLocalPlayers(
            MphRead::RenderWindow& renderer,
            const LaunchPlan& plan,
            bool teamPlay);
    };
}
