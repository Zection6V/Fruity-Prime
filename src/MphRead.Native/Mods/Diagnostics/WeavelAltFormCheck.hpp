#pragma once

#include <cstdint>
#include <string>

namespace MphRead::Mods::Diagnostics
{
    class WeavelAltFormCheck final
    {
    public:
        static std::int32_t Run(const std::string& room);
    };
}
