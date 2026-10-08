#include "SyluxMuzzleGuard.hpp"

#include <cstdlib>
#include <string_view>

namespace MphRead::Mods::Combat
{
    bool SyluxMuzzleGuard::Enabled = []
    {
        const char* setting = std::getenv("FRUITY_SYLUX_MUZZLE_GUARD");
        return setting == nullptr || std::string_view(setting) != "0";
    }();
    bool SyluxMuzzleGuardMetrics::Enabled = []
    {
        const char* setting = std::getenv("FRUITY_SYLUX_MUZZLE_GUARD_METRICS");
        return setting != nullptr && std::string_view(setting) == "1";
    }();
    SyluxMuzzleGuardMetrics::Counts SyluxMuzzleGuardMetrics::Counters{};
}
