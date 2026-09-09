#pragma once

#include "Mods/thumbnail_batch.hpp"

#include <filesystem>
#include <span>
#include <string_view>

namespace fruityprime::mods::thumbnail {

// Platform heads install a renderer here. The launcher only asks this seam
// whether previews can be rendered and supplies the missing room list; it
// does not need to know whether the host is a Win32 worker or an Android GL
// thread.
class Host final {
public:
    using Renderer = std::function<RunResult(
        const std::filesystem::path&, std::span<const std::string_view>,
        int, int, const Report&)>;

    void set_renderer(Renderer renderer);

    [[nodiscard]] bool can_render(
        const std::filesystem::path& executable) const noexcept;

    [[nodiscard]] RunResult render_missing(
        const std::filesystem::path& game_root,
        std::span<const std::string_view> rooms, int width, int height,
        const Report& report = {}) const;

private:
    Renderer renderer_;
};

[[nodiscard]] Host& host() noexcept;

} // namespace fruityprime::mods::thumbnail
