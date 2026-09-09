#include "Mods/Launcher/Gui/launcher_widgets.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace fruityprime::launcher::gui {

void ProgressRow::set(double fraction, std::string stage) {
    fraction_ = std::clamp(fraction, 0.0, 1.0);
    stage_ = std::move(stage);
    visible_ = true;
}

void ProgressRow::reset() noexcept {
    fraction_ = 0.0;
    stage_.clear();
    visible_ = false;
}

int ProgressRow::percent() const noexcept {
    // .NET Math.Round(double) uses midpoint-to-even.  Spell that out instead
    // of using std::round, whose midpoint rule is away from zero.
    const double scaled = fraction_ * 100.0;
    const double lower = std::floor(scaled);
    const double remainder = scaled - lower;
    double rounded = lower;
    if (remainder > 0.5
        || (remainder == 0.5
            && std::fmod(lower, 2.0) != 0.0)) {
        rounded = lower + 1.0;
    }
    return static_cast<int>(std::clamp(rounded, 0.0, 100.0));
}

double ProgressRow::filled_width(double width) const noexcept {
    const double filled = std::max(0.0, width) * fraction_;
    return filled > 1.0 ? std::max(filled, BarHeight) : 0.0;
}

} // namespace fruityprime::launcher::gui
