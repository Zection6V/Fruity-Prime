#pragma once

#include <functional>
#include <memory>

namespace OpenTK::Mathematics
{
    struct Vector2i;
}

namespace MphRead
{
    class Scene;
}

namespace MphRead::Mods::Launcher
{
    class LaunchPlan;
}

namespace MphRead::Droid
{
    class AndroidInput;

    class AndroidMatch final
    {
    public:
        AndroidMatch() = delete;

        [[nodiscard]] static std::unique_ptr<MphRead::Scene> Build(
            AndroidInput& input,
            OpenTK::Mathematics::Vector2i size,
            MphRead::Mods::Launcher::LaunchPlan plan,
            std::function<void()> close);

        static void Finish();

    private:
        [[nodiscard]] static std::unique_ptr<MphRead::Scene> BuildDemo(
            AndroidInput& input,
            OpenTK::Mathematics::Vector2i size,
            MphRead::Mods::Launcher::LaunchPlan plan,
            std::function<void()> close);

        [[nodiscard]] static std::unique_ptr<MphRead::Scene> BuildAdventure(
            AndroidInput& input,
            OpenTK::Mathematics::Vector2i size,
            MphRead::Mods::Launcher::LaunchPlan plan,
            std::function<void()> close);

        static void BuildNetworkedMatch(
            MphRead::Scene& scene,
            MphRead::Mods::Launcher::LaunchPlan plan);

        static void AddLocalPlayers(
            MphRead::Scene& scene,
            MphRead::Mods::Launcher::LaunchPlan plan,
            bool teamPlay);
    };
}
