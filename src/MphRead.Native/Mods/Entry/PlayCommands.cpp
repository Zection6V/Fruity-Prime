#include "PlayCommands.hpp"

#include "CommandLine.hpp"
#include "../Testing/GoreaBattle.hpp"

namespace MphRead::Mods::Entry
{
    bool TryRunPlayCommands(const std::vector<std::string>& args)
    {
        if (!HasFlag(args, "goreabattle"))
        {
            return false;
        }
        Testing::GoreaBattle::Run();
        return true;
    }
}
