#pragma once

#include <cstdint>

namespace fruityprime::launcher::gui {

// The toolkit-neutral part of Mods/Launcher/Gui/GuiTheme.cs.  Avalonia's
// brushes, font and icon remain in the GUI head; these values are the shared
// palette contract that a native front end can use without depending on a UI
// toolkit.
struct Color final {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t alpha = 255;

    friend constexpr bool operator==(const Color&, const Color&) = default;
};

struct Palette final {
    Color ink;
    Color panel;
    Color panel_light;
    Color edge;
    Color text;
    Color text_dim;
    Color accent;
    Color warm;
    Color good;
    Color bad;
    Color scrim;
};

[[nodiscard]] const Palette& palette() noexcept;

// Blend towards white for a positive amount and black for a negative amount,
// preserving alpha as GuiTheme.Shade does.
[[nodiscard]] Color shade(Color color, double amount) noexcept;

} // namespace fruityprime::launcher::gui
