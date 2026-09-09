#include "Mods/Launcher/Gui/map_picker_view.hpp"

#include "Entities/room_catalog.hpp"
#include "Mods/thumbnail_generator.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

namespace fruityprime::launcher::gui {
namespace {

bool default_image_probe(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

} // namespace

MapTile::MapTile(std::string room_key, std::filesystem::path game_root,
                 ImageProbe image_probe, double width, double height)
    : room_key_(std::move(room_key)), image_probe_(std::move(image_probe)),
      width_(width), height_(height) {
    const scene::RoomCatalogEntry* metadata = scene::find_room(room_key_);
    caption_ = metadata != nullptr && !metadata->in_game_name.empty()
        ? metadata->in_game_name : room_key_;
    image_path_ = mods::thumbnail::path_for(game_root, room_key_);
    has_image_ = probe(image_path_);
}

RowRect MapTile::image_area() const noexcept {
    return RowRect{0.0, 0.0, width_,
                   std::max(0.0, height_ - CaptionHeight)};
}

RowRect MapTile::caption_area() const noexcept {
    return RowRect{0.0, std::max(0.0, height_ - CaptionHeight), width_,
                   CaptionHeight};
}

void MapTile::set_clicked_handler(ClickedHandler handler) {
    clicked_handler_ = std::move(handler);
}

bool MapTile::pointer_release(double x, double y) {
    if (x < 0.0 || y < 0.0 || x > width_ || y > height_) {
        return false;
    }
    if (clicked_handler_) {
        clicked_handler_();
    }
    return true;
}

bool MapTile::key_press(WidgetKey key) {
    if (key != WidgetKey::Enter && key != WidgetKey::Space) {
        return false;
    }
    if (clicked_handler_) {
        clicked_handler_();
    }
    return true;
}

bool MapTile::probe(const std::filesystem::path& path) const {
    if (image_probe_) {
        try {
            return image_probe_(path);
        } catch (...) {
            return false;
        }
    }
    return default_image_probe(path);
}

MapPickerView::MapPickerView(std::vector<std::string> rooms,
                             std::string current,
                             std::filesystem::path game_root,
                             ImageProbe image_probe) {
    tiles_.reserve(rooms.size());
    for (const auto& room : rooms) {
        tiles_.emplace_back(room, game_root, image_probe);
        tiles_.back().set_selected(room == current);
        if (room == current) {
            first_focus_index_ = tiles_.size() - 1;
        }
    }
    if (tiles_.empty()) {
        first_focus_index_ = 0;
    }
}

void MapPickerView::set_closed_handler(ClosedHandler handler) {
    closed_handler_ = std::move(handler);
}

bool MapPickerView::activate(std::size_t index) {
    if (closed_ || index >= tiles_.size()) {
        return false;
    }
    room_key_ = tiles_[index].room_key();
    finish();
    return true;
}

bool MapPickerView::key_press(WidgetKey key) {
    if (closed_ || key != WidgetKey::Escape) {
        return false;
    }
    finish();
    return true;
}

void MapPickerView::close() {
    finish();
}

void MapPickerView::finish() {
    if (closed_) {
        return;
    }
    closed_ = true;
    if (closed_handler_) {
        closed_handler_();
    }
}

MapPickerWindow::MapPickerWindow(MapPickerView view)
    : view_(std::move(view)) {
    view_.set_closed_handler([this] { close(); });
}

} // namespace fruityprime::launcher::gui
