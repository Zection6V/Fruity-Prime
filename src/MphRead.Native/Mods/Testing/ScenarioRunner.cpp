#include "ScenarioRunner.hpp"

#include "../Launcher/GuiLauncher.hpp"

#include <iostream>
#include <utility>

namespace MphRead::Mods::Testing
{
    void ScenarioRunner::RunInShell(const char* name, Launcher::Gui::Shell::StartCallback start)
    {
#if defined(MPHREAD_SHELL)
        if (!Launcher::Gui::GuiLauncher::EnsureSetup())
        {
            std::cout << "[test] " << name << ": the game shell could not start" << std::endl;
            return;
        }
        Launcher::Gui::Shell::LaunchOnStart(std::move(start));
        if (!Launcher::Gui::GuiLauncher::TryRun())
        {
            Launcher::Gui::Shell::LaunchOnStart(Launcher::Gui::Shell::StartCallback{});
            std::cout << "[test] " << name << ": the game shell could not start" << std::endl;
        }
#else
        (void)start;
        std::cout << "[test] " << name << ": this build has no game shell" << std::endl;
#endif
    }
}
