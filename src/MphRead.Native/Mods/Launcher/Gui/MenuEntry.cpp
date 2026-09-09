#include "Mods/Launcher/Gui/launcher_widgets.hpp"

#include <algorithm>
#include <utility>

namespace fruityprime::launcher::gui {

MenuEntry::MenuEntry(std::string title, std::string subtitle,
                     double title_size)
    : title_(std::move(title)), subtitle_(std::move(subtitle)),
      title_size_(title_size) {}

void MenuEntry::set_click_handler(ClickHandler handler) {
    click_handler_ = std::move(handler);
}

void MenuEntry::set_title(std::string title) {
    title_ = std::move(title);
}

void MenuEntry::set_subtitle(std::string subtitle) {
    subtitle_ = std::move(subtitle);
}

double MenuEntry::height() const noexcept {
    return subtitle_.empty() ? PlainHeight : SubtitledHeight;
}

double MenuEntry::requested_width(const FontMetrics& metrics,
                                  double available_width) const {
    const double tracking = primary_ ? 2.0 : 1.0;
    double width = TrackedText::measure(title_, title_size_, tracking, metrics);
    if (!subtitle_.empty()) {
        width = std::max(width,
                         TrackedText::measure(subtitle_, 12.0, 0.0, metrics));
    }
    constexpr double padding = 3.0 + 14.0 + 14.0;
    return std::min(width + padding, available_width);
}

void MenuEntry::pointer_press() noexcept {
    if (enabled_) {
        pressed_ = true;
    }
}

bool MenuEntry::pointer_release(double x, double y,
                                double width) {
    const bool was_pressed = pressed_;
    pressed_ = false;
    const bool inside = x >= 0.0 && y >= 0.0 && x <= width && y <= height();
    const bool activate = was_pressed && enabled_ && inside;
    if (activate && click_handler_) {
        click_handler_();
    }
    return activate;
}

bool MenuEntry::key_press(WidgetKey key) {
    if (key != WidgetKey::Enter && key != WidgetKey::Space) {
        return false;
    }
    if (!enabled_) {
        return false;
    }
    if (click_handler_) {
        click_handler_();
    }
    return true;
}

} // namespace fruityprime::launcher::gui
