#include "Mods/Launcher/Gui/launcher_widgets.hpp"

#include <algorithm>
#include <cmath>

namespace fruityprime::launcher::gui {
namespace {

double fallback_width(std::string_view text, double size, bool bold) noexcept {
    const double glyph = size * (bold ? 0.62 : 0.58);
    double width = 0.0;
    for (const char character : text) {
        width += character == ' ' ? size * 0.22 : glyph;
    }
    return width;
}

double raw_width(std::string_view text, double size, bool bold,
                 const FontMetrics& metrics) {
    if (metrics.measure) {
        return metrics.measure(text, size, bold);
    }
    return fallback_width(text, size, bold);
}

} // namespace

double TrackedText::space_width(double size, const FontMetrics& metrics) {
    const double pair = raw_width("nn", size, true, metrics);
    const double spaced = raw_width("n n", size, true, metrics);
    return std::max(spaced - pair, size * 0.22);
}

double TrackedText::measure(std::string_view text, double size,
                            double tracking,
                            const FontMetrics& metrics) {
    const double space = space_width(size, metrics);
    double width = 0.0;
    for (const char character : text) {
        width += (character == ' '
                      ? space
                      : raw_width(std::string_view(&character, 1), size,
                                  true, metrics))
            + tracking;
    }
    return width;
}

double TrackedText::line_height(double size,
                                const FontMetrics& metrics) {
    if (metrics.line_height) {
        return metrics.line_height(size);
    }
    return size * 1.2;
}

} // namespace fruityprime::launcher::gui
