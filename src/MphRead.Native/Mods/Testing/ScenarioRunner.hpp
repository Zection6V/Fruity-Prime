#pragma once

#include "../Launcher/Shell.hpp"

namespace MphRead::Mods::Testing
{
    class ScenarioRunner final
    {
    public:
        ScenarioRunner() = delete;

        static void RunInShell(const char* name, Launcher::Gui::Shell::StartCallback start);
    };
}
