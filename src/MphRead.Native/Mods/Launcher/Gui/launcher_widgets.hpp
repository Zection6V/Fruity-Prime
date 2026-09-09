#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>

namespace fruityprime::launcher::gui {

// The Avalonia controls use these keys for activation and adjustment.  A
// native head translates its toolkit's key enum once at the edge and keeps
// the widget behavior identical on Win32, Android, and a future desktop head.
enum class WidgetKey : std::uint8_t {
    Enter,
    Space,
    Left,
    Right,
    Escape,
};

struct FontMetrics final {
    // Return the rendered width of text at `size`.  The callback is optional:
    // a head without a font yet still gets stable proportional layout values.
    std::function<double(std::string_view, double, bool)> measure;
    std::function<double(double)> line_height;
};

class TrackedText final {
public:
    [[nodiscard]] static double space_width(
        double size, const FontMetrics& metrics = {});

    [[nodiscard]] static double measure(
        std::string_view text, double size, double tracking,
        const FontMetrics& metrics = {});

    [[nodiscard]] static double line_height(
        double size, const FontMetrics& metrics = {});
};

// Toolkit-neutral state for the custom-drawn MenuEntry.  The platform head
// remains responsible for painting the marker bar and tracked glyphs.
class MenuEntry final {
public:
    static constexpr double PlainHeight = 42.0;
    static constexpr double SubtitledHeight = 54.0;

    explicit MenuEntry(std::string title, std::string subtitle = {},
                       double title_size = 21.0);

    using ClickHandler = std::function<void()>;
    void set_click_handler(ClickHandler handler);

    [[nodiscard]] const std::string& title() const noexcept { return title_; }
    [[nodiscard]] const std::string& subtitle() const noexcept {
        return subtitle_;
    }
    void set_title(std::string title);
    void set_subtitle(std::string subtitle);

    [[nodiscard]] double title_size() const noexcept { return title_size_; }
    [[nodiscard]] double height() const noexcept;
    [[nodiscard]] double requested_width(
        const FontMetrics& metrics = {},
        double available_width = std::numeric_limits<double>::infinity()) const;

    void set_enabled(bool enabled) noexcept { enabled_ = enabled; }
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    void set_primary(bool primary) noexcept { primary_ = primary; }
    [[nodiscard]] bool primary() const noexcept { return primary_; }
    void set_selected(bool selected) noexcept { selected_ = selected; }
    [[nodiscard]] bool selected() const noexcept { return selected_; }
    [[nodiscard]] bool pressed() const noexcept { return pressed_; }
    [[nodiscard]] bool pointer_over() const noexcept { return pointer_over_; }
    [[nodiscard]] bool focused() const noexcept { return focused_; }

    void pointer_enter() noexcept { pointer_over_ = true; }
    void pointer_exit() noexcept {
        pointer_over_ = false;
        pressed_ = false;
    }
    void focus() noexcept { focused_ = true; }
    void blur() noexcept {
        focused_ = false;
        pressed_ = false;
    }
    void pointer_press() noexcept;
    [[nodiscard]] bool pointer_release(double x, double y,
                                       double width);
    [[nodiscard]] bool key_press(WidgetKey key);

private:
    std::string title_;
    std::string subtitle_;
    double title_size_ = 21.0;
    bool enabled_ = true;
    bool primary_ = false;
    bool selected_ = false;
    bool pressed_ = false;
    bool pointer_over_ = false;
    bool focused_ = false;
    ClickHandler click_handler_;
};

class ProgressRow final {
public:
    static constexpr double Height = 44.0;
    static constexpr double BarHeight = 8.0;

    void set(double fraction, std::string stage);
    void reset() noexcept;

    [[nodiscard]] bool visible() const noexcept { return visible_; }
    [[nodiscard]] double fraction() const noexcept { return fraction_; }
    [[nodiscard]] const std::string& stage() const noexcept { return stage_; }
    [[nodiscard]] int percent() const noexcept;

    // Width of the filled rounded bar used by the platform painter.  A
    // non-zero fraction gets at least one bar-height of width, matching the
    // managed control's small-progress visual.
    [[nodiscard]] double filled_width(double width) const noexcept;

private:
    double fraction_ = 0.0;
    std::string stage_;
    bool visible_ = false;
};

struct SliderTrack final {
    double x = 0.0;
    double y = 0.0;
    double width = 40.0;
    double height = 4.0;
};

class SliderRow final {
public:
    static constexpr double Height = 34.0;

    using ValueChangedHandler = std::function<void(int)>;
    using Format = std::function<std::string(int)>;

    explicit SliderRow(std::string label, int value, Format format = {},
                       double label_width = 120.0);

    [[nodiscard]] const std::string& label() const noexcept { return label_; }
    [[nodiscard]] int value() const noexcept { return value_; }
    void set_value(int value);
    void set_value_changed_handler(ValueChangedHandler handler);
    [[nodiscard]] std::string formatted_value() const;

    [[nodiscard]] double label_width() const noexcept { return label_width_; }
    [[nodiscard]] SliderTrack track(double bounds_width,
                                    double bounds_height) const noexcept;
    void set_enabled(bool enabled) noexcept { enabled_ = enabled; }
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] bool dragging() const noexcept { return dragging_; }
    [[nodiscard]] bool hot() const noexcept { return hot_; }
    [[nodiscard]] bool focused() const noexcept { return focused_; }

    void focus() noexcept { focused_ = true; }
    void blur() noexcept { focused_ = false; }
    void pointer_press(double x, double bounds_width, double bounds_height);
    void pointer_move(double x, double bounds_width, double bounds_height);
    void pointer_release() noexcept { dragging_ = false; }
    [[nodiscard]] bool key_press(WidgetKey key);

private:
    void set_from_pointer(double x, double bounds_width, double bounds_height);

    std::string label_;
    double label_width_ = 120.0;
    Format format_;
    ValueChangedHandler value_changed_handler_;
    int value_ = 0;
    bool enabled_ = true;
    bool dragging_ = false;
    bool hot_ = false;
    bool focused_ = false;
};

class UpdateBadge final {
public:
    static constexpr double TitleSize = 13.0;
    static constexpr double SubtitleSize = 11.0;
    static constexpr double Tracking = 1.5;
    static constexpr double PaddingX = 14.0;
    static constexpr double PaddingY = 9.0;

    using ClickHandler = std::function<void()>;

    UpdateBadge() = default;

    void show(std::string subtitle);
    void say(std::string subtitle);
    void set_click_handler(ClickHandler handler);

    [[nodiscard]] bool visible() const noexcept { return visible_; }
    [[nodiscard]] const std::string& title() const noexcept { return title_; }
    [[nodiscard]] const std::string& subtitle() const noexcept {
        return subtitle_;
    }
    [[nodiscard]] double width(
        const FontMetrics& metrics = {},
        double available_width = std::numeric_limits<double>::infinity()) const;
    [[nodiscard]] double height(const FontMetrics& metrics = {}) const;

    [[nodiscard]] bool pressed() const noexcept { return pressed_; }
    [[nodiscard]] bool pointer_over() const noexcept { return pointer_over_; }
    [[nodiscard]] bool focused() const noexcept { return focused_; }
    void pointer_enter() noexcept { pointer_over_ = true; }
    void pointer_exit() noexcept {
        pointer_over_ = false;
        pressed_ = false;
    }
    void focus() noexcept { focused_ = true; }
    void blur() noexcept {
        focused_ = false;
        pressed_ = false;
    }
    void pointer_press() noexcept { pressed_ = true; }
    [[nodiscard]] bool pointer_release(bool pointer_is_over);
    [[nodiscard]] bool key_press(WidgetKey key);

private:
    std::string title_ = "Update now";
    std::string subtitle_;
    bool visible_ = false;
    bool pressed_ = false;
    bool pointer_over_ = false;
    bool focused_ = false;
    ClickHandler click_handler_;
};

} // namespace fruityprime::launcher::gui
