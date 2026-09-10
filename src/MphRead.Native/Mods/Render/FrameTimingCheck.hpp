#pragma once

#include <cstdint>

namespace MphRead::Mods::Render
{
    class FrameTimingCheck final
    {
    public:
        FrameTimingCheck() = delete;

        static std::int32_t Run();

    private:
        class Case;

        static bool RunCase(const Case& test);
        static bool RunStallCase();
    };
}
