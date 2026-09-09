#pragma once

#include <cstdint>
#include <filesystem>
#include <span>

namespace fruityprime::mods::screen_capture {

// OpenGL readback is RGB and bottom-up.  The frontend owns the actual
// glReadPixels call; this module owns the managed ScreenCapture policy and
// PNG hand-off so the same validation works for a window or an offscreen
// scene target.
[[nodiscard]] double lit_fraction(
    std::span<const std::uint8_t> rgb) noexcept;

// Save a bottom-up RGB8 image, refusing frames that are effectively black.
// Returning false matches ScreenCapture.Save's failure contract: callers do
// not count a file that was not a meaningful rendered frame.
[[nodiscard]] bool save_rgb(const std::filesystem::path& output,
                            int width, int height,
                            std::span<const std::uint8_t> bottom_up_rgb);

} // namespace fruityprime::mods::screen_capture
