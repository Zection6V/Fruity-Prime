#include "LauncherWindowCheck.hpp"

#if defined(__ANDROID__)

namespace MphRead::Mods::Diagnostics
{
    bool LauncherWindowCheck::_active = false;
    bool LauncherWindowCheck::_passed = false;
    int LauncherWindowCheck::_frames = 0;

    int LauncherWindowCheck::Run()
    {
        return 1;
    }

    void LauncherWindowCheck::AfterDraw(MphRead::RenderWindow&)
    {
    }

    void LauncherWindowCheck::Link(const std::string&, const std::string&)
    {
    }
}

#endif
