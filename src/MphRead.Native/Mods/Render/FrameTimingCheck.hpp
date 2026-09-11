#pragma once

#include <cstdint>

namespace MphRead::Mods::Render
{
    class FrameTimingCheck final
    {
    public:
        FrameTimingCheck() = delete;
        FrameTimingCheck(const FrameTimingCheck&) = delete;
        FrameTimingCheck& operator=(const FrameTimingCheck&) = delete;
        FrameTimingCheck(FrameTimingCheck&&) = delete;
        FrameTimingCheck& operator=(FrameTimingCheck&&) = delete;

        static std::int32_t Run();

    private:
        class Case;

        static bool RunCase(const Case& test);
        static bool RunStallCase();
    };
}
