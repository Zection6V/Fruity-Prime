#include "Mods/Launcher/Gui/splash_view.hpp"

#include "Mods/thumbnail_generator.hpp"

#include <algorithm>
#include <cmath>
#include <system_error>
#include <utility>

namespace fruityprime::launcher::gui {
namespace {

bool default_image_probe(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

bool same_image(SplashImageKind left_kind,
                const std::filesystem::path& left_path,
                SplashImageKind right_kind,
                const std::filesystem::path& right_path) {
    return left_kind == right_kind && left_path == right_path;
}

} // namespace

SplashView::SplashView(std::filesystem::path game_root,
                       std::filesystem::path executable_directory,
                       ImageProbe image_probe)
    : game_root_(std::move(game_root)),
      executable_directory_(std::move(executable_directory)),
      image_probe_(std::move(image_probe)) {
    load_custom();
}

void SplashView::set_bottom_inset(double inset) noexcept {
    if (std::abs(bottom_inset_ - inset) > 0.5) {
        bottom_inset_ = inset;
    }
}

void SplashView::show_room(std::string_view room_key) {
    room_key_.assign(room_key);
    if (!room_key.empty()) {
        const auto room_path = mods::thumbnail::path_for(game_root_, room_key);
        if (probe(room_path)) {
            if (!same_image(image_kind_, image_path_, SplashImageKind::Room,
                            room_path)) {
                set_image(SplashImageKind::Room, room_path);
            }
            return;
        }
    }
    load_custom();
}

std::vector<std::filesystem::path> SplashView::custom_candidates() const {
    return {executable_directory_ / "splash.png",
            executable_directory_ / "splash.jpg",
            executable_directory_ / "splash.jpeg"};
}

RowRect SplashView::bottom_wash(double body_width,
                                double body_height) const noexcept {
    return RowRect{0.0, body_height - 90.0 - bottom_inset_, body_width,
                   90.0 + bottom_inset_};
}

RowRect SplashView::cover_destination(double body_width, double body_height,
                                      double image_width,
                                      double image_height) noexcept {
    if (image_width <= 0.0 || image_height <= 0.0) {
        return {};
    }
    const double scale = std::max(body_width / image_width,
                                  body_height / image_height);
    const double width = image_width * scale;
    const double height = image_height * scale;
    return RowRect{(body_width - width) / 2.0,
                   (body_height - height) / 2.0, width, height};
}

RowRect SplashView::brand_destination(double body_width, double body_height,
                                      double brand_width, double brand_height,
                                      double bottom_inset) noexcept {
    if (body_width < 80.0 || brand_width <= 0.0 || brand_height <= 0.0) {
        return {};
    }
    const double width = std::min({body_width - 48.0, 320.0, brand_width});
    if (width <= 0.0) {
        return {};
    }
    const double height = width * brand_height / brand_width;
    return RowRect{24.0, body_height - 26.0 - height - bottom_inset, width,
                   height};
}

RowRect SplashView::title_brand_destination(double body_width,
                                            double body_height,
                                            double brand_width,
                                            double brand_height) noexcept {
    if (brand_width <= 0.0 || brand_height <= 0.0) {
        return {};
    }
    const double width = std::min(body_width * 0.72, brand_width);
    if (width <= 0.0) {
        return {};
    }
    const double height = width * brand_height / brand_width;
    return RowRect{body_width / 2.0 - width / 2.0,
                   body_height / 2.0 - 20.0 - height / 2.0, width, height};
}

bool SplashView::probe(const std::filesystem::path& path) const {
    if (image_probe_) {
        try {
            return image_probe_(path);
        } catch (...) {
            return false;
        }
    }
    return default_image_probe(path);
}

void SplashView::load_custom() {
    for (const auto& candidate : custom_candidates()) {
        if (probe(candidate)) {
            if (!same_image(image_kind_, image_path_,
                            SplashImageKind::Custom, candidate)) {
                set_image(SplashImageKind::Custom, candidate);
            }
            return;
        }
    }
    set_image(SplashImageKind::None, {});
}

void SplashView::set_image(SplashImageKind kind,
                           std::filesystem::path path) {
    image_kind_ = kind;
    image_path_ = std::move(path);
}

} // namespace fruityprime::launcher::gui
