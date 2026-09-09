#include "Mods/Launcher/Gui/launcher_rows.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace fruityprime::launcher::gui {
namespace {

std::string upper_invariant(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const unsigned char character : text) {
        result.push_back(static_cast<char>(std::toupper(character)));
    }
    return result;
}

double text_width(std::string_view text, double size, bool bold,
                  const FontMetrics& metrics) {
    if (metrics.measure) {
        return metrics.measure(text, size, bold);
    }
    double width = 0.0;
    for (const char character : text) {
        width += character == ' ' ? size * 0.22 : size * (bold ? 0.62 : 0.58);
    }
    return width;
}

int clamp_index(int index, std::size_t count) noexcept {
    if (count == 0) {
        return 0;
    }
    return std::clamp(index, 0, static_cast<int>(count - 1));
}

} // namespace

Caption::Caption(std::string text) : text_(std::move(text)) {}

std::string Caption::label() const {
    return upper_invariant(text_);
}

double Caption::requested_width(const FontMetrics& metrics,
                                double available_width) const {
    return std::min(text_width(label(), FontSize, true, metrics) + 8.0,
                    available_width);
}

ChoiceRow::ChoiceRow(std::string label, std::vector<std::string> options,
                     int index)
    : label_(std::move(label)), options_(std::move(options)),
      index_(clamp_index(index, options_.size())) {}

void ChoiceRow::set_index(int index) {
    const int clamped = clamp_index(index, options_.size());
    if (clamped == index_) {
        return;
    }
    index_ = clamped;
    notify_changed();
}

std::string_view ChoiceRow::value() const noexcept {
    return options_.empty() ? std::string_view{} : options_[index_];
}

void ChoiceRow::set_items(std::vector<std::string> options, int index) {
    options_ = std::move(options);
    index_ = clamp_index(index, options_.size());
    left_hot_ = false;
    right_hot_ = false;
}

void ChoiceRow::set_changed_handler(ChangedHandler handler) {
    changed_handler_ = std::move(handler);
}

RowRect ChoiceRow::left_arrow(double bounds_width,
                              double bounds_height) const noexcept {
    const double x = std::max(
        MinimumArrowX, bounds_width - ArrowWidth - ValueColumn - ArrowWidth);
    return RowRect{x, 0.0, ArrowWidth, bounds_height};
}

RowRect ChoiceRow::right_arrow(double bounds_width,
                               double bounds_height) const noexcept {
    return RowRect{bounds_width - ArrowWidth, 0.0, ArrowWidth, bounds_height};
}

RowRect ChoiceRow::value_area(double bounds_width,
                             double bounds_height) const noexcept {
    const RowRect left = left_arrow(bounds_width, bounds_height);
    const RowRect right = right_arrow(bounds_width, bounds_height);
    return RowRect{left.right(), 0.0,
                   std::max(0.0, right.x - left.right() - 8.0), bounds_height};
}

double ChoiceRow::value_center(double bounds_width) const noexcept {
    const RowRect left = left_arrow(bounds_width);
    const RowRect right = right_arrow(bounds_width);
    return (left.right() + right.x) / 2.0;
}

void ChoiceRow::pointer_move(double x, double y, double bounds_width,
                             double bounds_height) noexcept {
    left_hot_ = left_arrow(bounds_width, bounds_height).contains(x, y);
    right_hot_ = right_arrow(bounds_width, bounds_height).contains(x, y);
}

void ChoiceRow::pointer_exit() noexcept {
    left_hot_ = false;
    right_hot_ = false;
}

void ChoiceRow::pointer_press(double x, double y, double bounds_width,
                              double bounds_height) {
    if (left_arrow(bounds_width, bounds_height).contains(x, y)) {
        step(-1);
    } else {
        // This intentionally matches Rows.cs: the whole row is a forward
        // step, so a player need not aim precisely at the small arrow.
        step(1);
    }
}

bool ChoiceRow::key_press(WidgetKey key) {
    if (key == WidgetKey::Left) {
        step(-1);
        return true;
    }
    if (key == WidgetKey::Right || key == WidgetKey::Enter
        || key == WidgetKey::Space) {
        step(1);
        return true;
    }
    return false;
}

void ChoiceRow::step(int direction) {
    if (options_.empty()) {
        return;
    }
    const int count = static_cast<int>(options_.size());
    int next = (index_ + direction) % count;
    if (next < 0) {
        next += count;
    }
    index_ = next;
    notify_changed();
}

void ChoiceRow::notify_changed() {
    if (changed_handler_) {
        changed_handler_(index_, value());
    }
}

ToggleRow::ToggleRow(std::string label, bool on)
    : label_(std::move(label)), on_(on) {}

void ToggleRow::set_on(bool on) {
    if (on_ == on) {
        return;
    }
    on_ = on;
    if (changed_handler_) {
        changed_handler_(on_);
    }
}

void ToggleRow::set_changed_handler(ChangedHandler handler) {
    changed_handler_ = std::move(handler);
}

RowRect ToggleRow::track(double bounds_width,
                         double bounds_height) const noexcept {
    return RowRect{bounds_width - TrackWidth - TrackInset,
                   (bounds_height - TrackHeight) / 2.0,
                   TrackWidth, TrackHeight};
}

double ToggleRow::knob_center_x(double bounds_width) const noexcept {
    const RowRect area = track(bounds_width);
    return on_ ? area.right() - TrackHeight / 2.0
               : area.x + TrackHeight / 2.0;
}

void ToggleRow::pointer_press() {
    set_on(!on_);
}

bool ToggleRow::key_press(WidgetKey key) {
    if (key != WidgetKey::Enter && key != WidgetKey::Space
        && key != WidgetKey::Left && key != WidgetKey::Right) {
        return false;
    }
    set_on(!on_);
    return true;
}

FieldRow::FieldRow(std::string label, std::string value, double box_width)
    : label_(std::move(label)), value_(std::move(value)),
      box_width_(box_width) {}

void FieldRow::set_value(std::string value) {
    value_ = std::move(value);
}

RowRect FieldRow::box(double bounds_width,
                      double bounds_height) const noexcept {
    return RowRect{std::max(0.0, bounds_width - box_width_),
                   (bounds_height - Height) / 2.0, box_width_, Height};
}

Note::Note(std::string text, std::optional<Color> color)
    : text_(std::move(text)), color_(color.value_or(palette().text_dim)) {}

} // namespace fruityprime::launcher::gui
