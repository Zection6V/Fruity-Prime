#pragma once

#include <cstdint>

namespace MphRead::Mods::Launcher::Gui
{
    // What -tapcheck runs: Tap on its own, against gestures written down
    // rather than performed.
    class TapCheck final
    {
    public:
        TapCheck() = delete;

        [[nodiscard]] static std::int32_t Run();
    };
}
