#include "Mods/Launcher/Gui/launcher_widgets.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace fruityprime::launcher::gui {

SliderRow::SliderRow(std::string label, int value, Format format,
                     double label_width)
    : label_(std::move(label)), label_width_(label_width),
      format_(std::move(format)) {
    set_value(std::clamp(value, 0, 100));
}

void SliderRow::set_value(int value) {
    const int clamped = std::clamp(value, 0, 100);
    if (clamped == value_) {
        return;
    }
    value_ = clamped;
    if (value_changed_handler_) {
        value_changed_handler_(value_);
    }
}

void SliderRow::set_value_changed_handler(ValueChangedHandler handler) {
    value_changed_handler_ = std::move(handler);
}

std::string SliderRow::formatted_value() const {
    if (format_) {
        return format_(value_);
    }
    return std::to_string(value_) + "%";
}

SliderTrack SliderRow::track(double bounds_width,
                             double bounds_height) const noexcept {
    return SliderTrack{
        label_width_,
        bounds_height / 2.0 - 2.0,
        std::max(40.0, bounds_width - label_width_ - 64.0),
        4.0,
    };
}

void SliderRow::set_from_pointer(double x, double bounds_width,
                                 double bounds_height) {
    const SliderTrack slider_track = track(bounds_width, bounds_height);
    const double fraction = std::clamp(
        (x - slider_track.x) / std::max(1.0, slider_track.width), 0.0, 1.0);
    const double scaled = fraction * 100.0;
    const double lower = std::floor(scaled);
    const double remainder = scaled - lower;
    const double rounded = remainder > 0.5
        || (remainder == 0.5 && std::fmod(lower, 2.0) != 0.0)
        ? lower + 1.0 : lower;
    set_value(static_cast<int>(rounded));
}

void SliderRow::pointer_press(double x, double bounds_width,
                              double bounds_height) {
    if (!enabled_ || x < label_width_) {
        return;
    }
    dragging_ = true;
    set_from_pointer(x, bounds_width, bounds_height);
}

void SliderRow::pointer_move(double x, double bounds_width,
                             double bounds_height) {
    const bool hot = x >= label_width_;
    hot_ = hot;
    if (dragging_) {
        set_from_pointer(x, bounds_width, bounds_height);
    }
}

bool SliderRow::key_press(WidgetKey key) {
    if (!enabled_) {
        return false;
    }
    if (key == WidgetKey::Left) {
        set_value(value_ - 5);
        return true;
    }
    if (key == WidgetKey::Right) {
        set_value(value_ + 5);
        return true;
    }
    return false;
}

} // namespace fruityprime::launcher::gui
