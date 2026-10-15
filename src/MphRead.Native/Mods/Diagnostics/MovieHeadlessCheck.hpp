#pragma once

#include <cstdint>

namespace MphRead::Mods::Diagnostics
{
    class MovieHeadlessCheck final
    {
    public:
        MovieHeadlessCheck() = delete;

        [[nodiscard]] static std::int32_t Run();
    };
}
