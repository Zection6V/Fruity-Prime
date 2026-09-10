#pragma once

#include <string>
#include <vector>

namespace MphRead
{
    namespace Mods
    {
        class ModEntry final
        {
        public:
            [[nodiscard]] static bool TryHandleHeadless(const std::vector<std::string>& args);
            [[nodiscard]] static bool TryHandle(const std::vector<std::string>& args);

            ModEntry() = delete;
            ModEntry(const ModEntry&) = delete;
            ModEntry& operator=(const ModEntry&) = delete;
        };
    }
}
