#include "Formats/launcher_theme.hpp"

#include <algorithm>
#include <cmath>

namespace fruityprime::launcher::gui {
namespace {

constexpr Color rgb(std::uint8_t red, std::uint8_t green,
                    std::uint8_t blue) noexcept {
    return Color{red, green, blue, 255};
}

} // namespace

const Palette& palette() noexcept {
    static constexpr Palette value{
        rgb(10, 12, 16),
        rgb(18, 21, 28),
        rgb(26, 31, 41),
        rgb(38, 46, 60),
        rgb(230, 234, 242),
        rgb(138, 147, 166),
        rgb(41, 197, 255),
        rgb(255, 179, 71),
        rgb(110, 231, 135),
        rgb(255, 107, 107),
        Color{10, 12, 16, 196}
    };
    return value;
}

Color shade(Color color, double amount) noexcept {
    const double t = std::abs(amount);
    const double target = amount >= 0.0 ? 255.0 : 0.0;
    const auto blend = [t, target](std::uint8_t channel) {
        const double value = static_cast<double>(channel)
            + (target - static_cast<double>(channel)) * t;
        return static_cast<std::uint8_t>(std::clamp(value, 0.0, 255.0));
    };
    return Color{blend(color.red), blend(color.green), blend(color.blue),
                 color.alpha};
}

} // namespace fruityprime::launcher::gui
