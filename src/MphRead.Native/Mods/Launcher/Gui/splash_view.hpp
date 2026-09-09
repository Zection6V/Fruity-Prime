#pragma once

#include "Mods/Launcher/Gui/launcher_rows.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::launcher::gui {

enum class SplashImageKind {
    None,
    Room,
    Custom,
};

// Portable state and layout for the launcher splash.  A platform head owns
// the actual bitmap decoder and painter; this class keeps the managed
// SplashView's image-selection and geometry rules in one toolkit-neutral
// place.
class SplashView final {
public:
    using ImageProbe = std::function<bool(const std::filesystem::path&)>;

    SplashView(std::filesystem::path game_root,
               std::filesystem::path executable_directory,
               ImageProbe image_probe = {});

    void set_bottom_inset(double inset) noexcept;
    [[nodiscard]] double bottom_inset() const noexcept {
        return bottom_inset_;
    }

    // Select a generated room thumbnail when it is available, otherwise use
    // splash.png, splash.jpg, or splash.jpeg beside the executable.
    void show_room(std::string_view room_key);

    [[nodiscard]] SplashImageKind image_kind() const noexcept {
        return image_kind_;
    }
    [[nodiscard]] bool has_image() const noexcept {
        return image_kind_ != SplashImageKind::None;
    }
    [[nodiscard]] const std::filesystem::path& image_path() const noexcept {
        return image_path_;
    }
    [[nodiscard]] const std::string& room_key() const noexcept {
        return room_key_;
    }
    [[nodiscard]] const std::filesystem::path& game_root() const noexcept {
        return game_root_;
    }
    [[nodiscard]] const std::filesystem::path& executable_directory()
        const noexcept {
        return executable_directory_;
    }

    [[nodiscard]] std::vector<std::filesystem::path> custom_candidates()
        const;

    // These are the exact destination rectangles used by the managed painter.
    // A native head supplies decoded source dimensions and paints the returned
    // rectangles with its own graphics API.
    [[nodiscard]] RowRect bottom_wash(double body_width,
                                      double body_height) const noexcept;
    [[nodiscard]] static RowRect cover_destination(double body_width,
                                                   double body_height,
                                                   double image_width,
                                                   double image_height) noexcept;
    [[nodiscard]] static RowRect brand_destination(double body_width,
                                                   double body_height,
                                                   double brand_width,
                                                   double brand_height,
                                                   double bottom_inset) noexcept;
    [[nodiscard]] static RowRect title_brand_destination(
        double body_width, double body_height, double brand_width,
        double brand_height) noexcept;

private:
    [[nodiscard]] bool probe(const std::filesystem::path& path) const;
    void load_custom();
    void set_image(SplashImageKind kind, std::filesystem::path path);

    std::filesystem::path game_root_;
    std::filesystem::path executable_directory_;
    ImageProbe image_probe_;
    SplashImageKind image_kind_ = SplashImageKind::None;
    std::filesystem::path image_path_;
    std::string room_key_;
    double bottom_inset_ = 0.0;
};

} // namespace fruityprime::launcher::gui
