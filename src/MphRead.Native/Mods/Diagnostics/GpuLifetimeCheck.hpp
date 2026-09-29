#pragma once

#include <cstdint>
#include <string>

namespace MphRead::Mods::Diagnostics
{
    // -gpulifetime ["ROOM"] [-cycles N] [-frames N]: in one window, create a
    // scene, load the room with a full house, draw it, release it, and do it
    // again, N times. After every release the device's live objects are
    // counted; a scene that leaks a texture, a framebuffer, a shader or a
    // program shows as a count that climbs, and anything still retired after
    // the release's wait for idle is a lifetime bug. The counts must match
    // from the first cycle on (the device keeps the map thumbnails' reserved
    // textures across scenes, by design). Exit code 0 is a pass.
    class GpuLifetimeCheck final
    {
    public:
        GpuLifetimeCheck() = delete;

        [[nodiscard]] static std::int32_t Run(const std::string& room, std::int32_t cycles, std::int32_t frames);
    };
}
