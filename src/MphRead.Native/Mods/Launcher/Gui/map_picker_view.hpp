#pragma once

#include "Mods/Launcher/Gui/launcher_rows.hpp"
#include "Mods/Launcher/Gui/launcher_widgets.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::launcher::gui {

class MapTile final {
public:
    static constexpr double DefaultWidth = 248.0;
    static constexpr double DefaultHeight = 168.0;
    static constexpr double Margin = 8.0;
    static constexpr double CaptionHeight = 26.0;

    using ImageProbe = std::function<bool(const std::filesystem::path&)>;
    using ClickedHandler = std::function<void()>;

    MapTile(std::string room_key, std::filesystem::path game_root,
            ImageProbe image_probe = {}, double width = DefaultWidth,
            double height = DefaultHeight);

    [[nodiscard]] const std::string& room_key() const noexcept {
        return room_key_;
    }
    [[nodiscard]] const std::string& caption() const noexcept {
        return caption_;
    }
    [[nodiscard]] const std::filesystem::path& image_path() const noexcept {
        return image_path_;
    }
    [[nodiscard]] bool has_image() const noexcept { return has_image_; }
    [[nodiscard]] double width() const noexcept { return width_; }
    [[nodiscard]] double height() const noexcept { return height_; }
    [[nodiscard]] RowRect image_area() const noexcept;
    [[nodiscard]] RowRect caption_area() const noexcept;
    [[nodiscard]] bool selected() const noexcept { return selected_; }
    [[nodiscard]] bool hover() const noexcept { return hover_; }
    [[nodiscard]] bool focused() const noexcept { return focused_; }

    void set_selected(bool selected) noexcept { selected_ = selected; }
    void set_clicked_handler(ClickedHandler handler);
    void pointer_enter() noexcept { hover_ = true; }
    void pointer_exit() noexcept { hover_ = false; }
    void focus() noexcept {
        focused_ = true;
        hover_ = true;
    }
    void blur() noexcept {
        focused_ = false;
        hover_ = false;
    }
    [[nodiscard]] bool pointer_release(double x, double y);
    [[nodiscard]] bool key_press(WidgetKey key);

private:
    [[nodiscard]] bool probe(const std::filesystem::path& path) const;

    std::string room_key_;
    std::string caption_;
    std::filesystem::path image_path_;
    ImageProbe image_probe_;
    ClickedHandler clicked_handler_;
    double width_ = DefaultWidth;
    double height_ = DefaultHeight;
    bool has_image_ = false;
    bool selected_ = false;
    bool hover_ = false;
    bool focused_ = false;
};

class MapPickerView final {
public:
    using ImageProbe = MapTile::ImageProbe;
    using ClosedHandler = std::function<void()>;

    MapPickerView(std::vector<std::string> rooms, std::string current,
                  std::filesystem::path game_root, ImageProbe image_probe = {});

    [[nodiscard]] const std::vector<MapTile>& tiles() const noexcept {
        return tiles_;
    }
    [[nodiscard]] const std::optional<std::string>& room_key() const noexcept {
        return room_key_;
    }
    [[nodiscard]] bool closed() const noexcept { return closed_; }
    [[nodiscard]] std::size_t first_focus_index() const noexcept {
        return first_focus_index_;
    }

    void set_closed_handler(ClosedHandler handler);
    [[nodiscard]] bool activate(std::size_t index);
    [[nodiscard]] bool key_press(WidgetKey key);
    void close();

private:
    void finish();

    std::vector<MapTile> tiles_;
    std::optional<std::string> room_key_;
    std::size_t first_focus_index_ = 0;
    ClosedHandler closed_handler_;
    bool closed_ = false;
};

class MapPickerWindow final {
public:
    static constexpr double Width = 1120.0;
    static constexpr double Height = 720.0;
    static constexpr double MinWidth = 560.0;
    static constexpr double MinHeight = 420.0;

    explicit MapPickerWindow(MapPickerView view);

    [[nodiscard]] const MapPickerView& view() const noexcept { return view_; }
    [[nodiscard]] MapPickerView& view() noexcept { return view_; }
    [[nodiscard]] const std::string& title() const noexcept { return title_; }
    [[nodiscard]] bool open() const noexcept { return open_; }
    [[nodiscard]] bool centered_on_owner() const noexcept { return true; }
    [[nodiscard]] bool decorated() const noexcept { return true; }

    void show() noexcept { open_ = true; }
    void close() noexcept { open_ = false; }

private:
    MapPickerView view_;
    std::string title_ = "Choose a map";
    bool open_ = false;
};

} // namespace fruityprime::launcher::gui
