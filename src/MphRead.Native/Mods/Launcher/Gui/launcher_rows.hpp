#pragma once

#include "Formats/launcher_theme.hpp"
#include "Mods/Launcher/Gui/launcher_widgets.hpp"

#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::launcher::gui {

// The managed controls receive Avalonia Bounds and pointer coordinates.  A
// native head exposes the same small geometry contract to its own painter and
// translates its toolkit events at the edge.
struct RowRect final {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    [[nodiscard]] double right() const noexcept { return x + width; }
    [[nodiscard]] double bottom() const noexcept { return y + height; }
    [[nodiscard]] bool contains(double point_x, double point_y) const noexcept {
        return point_x >= x && point_y >= y && point_x <= right()
            && point_y <= bottom();
    }
};

class Caption final {
public:
    static constexpr double Height = 26.0;
    static constexpr double FontSize = 11.0;
    static constexpr double BottomPadding = 4.0;
    static constexpr double DividerOffset = 2.0;

    explicit Caption(std::string text);

    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    [[nodiscard]] std::string label() const;
    [[nodiscard]] double height() const noexcept { return Height; }
    [[nodiscard]] double requested_width(
        const FontMetrics& metrics = {},
        double available_width = std::numeric_limits<double>::infinity()) const;
    [[nodiscard]] double label_y(double bounds_height = Height) const noexcept {
        return bounds_height - BottomPadding;
    }
    [[nodiscard]] double divider_y(
        double bounds_height = Height) const noexcept {
        return bounds_height - DividerOffset;
    }

private:
    std::string text_;
};

class ChoiceRow final {
public:
    static constexpr double Height = 34.0;
    static constexpr double ArrowWidth = 28.0;
    static constexpr double ValueColumn = 180.0;
    static constexpr double MinimumArrowX = 110.0;

    using ChangedHandler = std::function<void(int, std::string_view)>;

    ChoiceRow(std::string label, std::vector<std::string> options,
              int index = 0);

    [[nodiscard]] const std::string& label() const noexcept { return label_; }
    [[nodiscard]] const std::vector<std::string>& options() const noexcept {
        return options_;
    }
    [[nodiscard]] int index() const noexcept { return index_; }
    void set_index(int index);
    [[nodiscard]] std::string_view value() const noexcept;

    void set_items(std::vector<std::string> options, int index = 0);
    void set_changed_handler(ChangedHandler handler);

    [[nodiscard]] RowRect left_arrow(
        double bounds_width, double bounds_height = Height) const noexcept;
    [[nodiscard]] RowRect right_arrow(
        double bounds_width, double bounds_height = Height) const noexcept;
    [[nodiscard]] RowRect value_area(
        double bounds_width, double bounds_height = Height) const noexcept;
    [[nodiscard]] double value_center(double bounds_width) const noexcept;

    [[nodiscard]] bool left_hot() const noexcept { return left_hot_; }
    [[nodiscard]] bool right_hot() const noexcept { return right_hot_; }
    void pointer_move(double x, double y, double bounds_width,
                      double bounds_height = Height) noexcept;
    void pointer_exit() noexcept;
    void pointer_press(double x, double y, double bounds_width,
                       double bounds_height = Height);
    [[nodiscard]] bool key_press(WidgetKey key);

private:
    void step(int direction);
    void notify_changed();

    std::string label_;
    std::vector<std::string> options_;
    int index_ = 0;
    bool left_hot_ = false;
    bool right_hot_ = false;
    ChangedHandler changed_handler_;
};

class ToggleRow final {
public:
    static constexpr double Height = 34.0;
    static constexpr double TrackWidth = 40.0;
    static constexpr double TrackHeight = 20.0;
    static constexpr double TrackInset = 4.0;

    using ChangedHandler = std::function<void(bool)>;

    ToggleRow(std::string label, bool on);

    [[nodiscard]] const std::string& label() const noexcept { return label_; }
    [[nodiscard]] bool on() const noexcept { return on_; }
    void set_on(bool on);
    void set_changed_handler(ChangedHandler handler);
    [[nodiscard]] RowRect track(double bounds_width,
                                 double bounds_height = Height) const noexcept;
    [[nodiscard]] double knob_center_x(double bounds_width) const noexcept;

    void pointer_press();
    [[nodiscard]] bool key_press(WidgetKey key);

private:
    std::string label_;
    bool on_ = false;
    ChangedHandler changed_handler_;
};

class FieldRow final {
public:
    static constexpr double Height = 36.0;
    static constexpr double DefaultBoxWidth = 150.0;
    static constexpr double FontSize = 13.0;
    static constexpr double LabelMarginLeft = 4.0;

    FieldRow(std::string label, std::string value,
             double box_width = DefaultBoxWidth);

    [[nodiscard]] const std::string& label() const noexcept { return label_; }
    [[nodiscard]] const std::string& value() const noexcept { return value_; }
    void set_value(std::string value);
    [[nodiscard]] double box_width() const noexcept { return box_width_; }
    [[nodiscard]] RowRect box(double bounds_width,
                              double bounds_height = Height) const noexcept;

private:
    std::string label_;
    std::string value_;
    double box_width_ = DefaultBoxWidth;
};

class Note final {
public:
    static constexpr double FontSize = 12.0;
    static constexpr double Margin = 4.0;

    explicit Note(std::string text,
                  std::optional<Color> color = std::nullopt);

    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    [[nodiscard]] Color color() const noexcept { return color_; }
    [[nodiscard]] bool wraps() const noexcept { return true; }
    [[nodiscard]] double font_size() const noexcept { return FontSize; }

private:
    std::string text_;
    Color color_{};
};

} // namespace fruityprime::launcher::gui
