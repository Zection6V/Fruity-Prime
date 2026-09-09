#include "Mods/Launcher/Portable/launch_plan.hpp"

namespace fruityprime::launcher {

metadata::Hunter Hunters::resolve(metadata::Hunter hunter) noexcept {
    if (hunter != metadata::Hunter::Random) {
        return hunter;
    }
    if (rolled_ == metadata::Hunter::Random) {
        rolled_ = static_cast<metadata::Hunter>(metadata::roll_hunter(seed_));
        // Make repeated calls deterministic for a single launch while still
        // ensuring the next launch with the same resolver seed can be rerolled.
    }
    return rolled_;
}

} // namespace fruityprime::launcher
