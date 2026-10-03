#pragma once

#include <atomic>

namespace MphRead::Mods::Launcher::Gui
{
    class GuiLauncher final
    {
    public:
        GuiLauncher() = delete;

        [[nodiscard]] static bool TryRun();

        // C# internal: used by diagnostics and by pause-menu requests.
        [[nodiscard]] static bool EnsureSetup(bool requireDisplay = true);

    private:
        static void SayWhyOnLinux();
        [[nodiscard]] static bool Probe();

        static std::atomic_bool _setUp;
        static std::atomic_bool _failed;
    };
}
