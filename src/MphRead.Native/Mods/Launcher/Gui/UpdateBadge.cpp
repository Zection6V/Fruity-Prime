#include "Mods/Launcher/Gui/launcher_widgets.hpp"

#include <algorithm>
#include <utility>

namespace fruityprime::launcher::gui {

void UpdateBadge::show(std::string subtitle) {
    subtitle_ = std::move(subtitle);
    visible_ = true;
}

void UpdateBadge::say(std::string subtitle) {
    subtitle_ = std::move(subtitle);
}

void UpdateBadge::set_click_handler(ClickHandler handler) {
    click_handler_ = std::move(handler);
}

double UpdateBadge::width(const FontMetrics& metrics,
                          double available_width) const {
    const double title = TrackedText::measure(
        title_, TitleSize, Tracking, metrics);
    const double subtitle = subtitle_.empty()
        ? 0.0 : TrackedText::measure(subtitle_, SubtitleSize, 0.0, metrics);
    return std::min(std::max(title, subtitle) + PaddingX * 2.0,
                    available_width);
}

double UpdateBadge::height(const FontMetrics& metrics) const {
    return TrackedText::line_height(TitleSize, metrics) + PaddingY * 2.0
        + (subtitle_.empty() ? 0.0 : SubtitleSize + 5.0);
}

bool UpdateBadge::pointer_release(bool pointer_is_over) {
    const bool activate = pressed_ && pointer_is_over;
    pressed_ = false;
    if (activate && click_handler_) {
        click_handler_();
    }
    return activate;
}

bool UpdateBadge::key_press(WidgetKey key) {
    if (key != WidgetKey::Enter && key != WidgetKey::Space) {
        return false;
    }
    if (click_handler_) {
        click_handler_();
    }
    return true;
}

} // namespace fruityprime::launcher::gui
