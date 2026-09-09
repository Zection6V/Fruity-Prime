#include "Mods/thumbnail_host.hpp"

#include "Mods/thumbnail_generator.hpp"

#include <utility>

namespace fruityprime::mods::thumbnail {

void Host::set_renderer(Renderer renderer) {
    renderer_ = std::move(renderer);
}

bool Host::can_render(
    const std::filesystem::path& executable) const noexcept {
    return static_cast<bool>(renderer_) || can_run(executable);
}

RunResult Host::render_missing(
    const std::filesystem::path& game_root,
    std::span<const std::string_view> rooms, int width, int height,
    const Report& report) const {
    const auto missing_rooms = missing(game_root, rooms);
    std::vector<std::string_view> missing_views;
    missing_views.reserve(missing_rooms.size());
    for (const std::string& room : missing_rooms) {
        missing_views.push_back(room);
    }
    if (missing_views.empty()) {
        return {};
    }
    if (!renderer_) {
        // A host without a renderer cannot safely touch the GL context. The
        // managed host makes the same choice on Android when no GL head has
        // installed itself: return zero rather than blocking the launcher on
        // work that can never produce a file.
        return RunResult{0, missing_rooms};
    }
    return renderer_(game_root, missing_views, width, height, report);
}

Host& host() noexcept {
    static Host value;
    return value;
}

} // namespace fruityprime::mods::thumbnail
