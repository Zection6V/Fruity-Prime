#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::launcher::gui {

struct UiCaptureSize final {
    int width = 940;
    int height = 560;

    [[nodiscard]] bool operator==(const UiCaptureSize&) const noexcept =
        default;
};

struct UiCaptureScreen final {
    std::string name;
    UiCaptureSize size{};

    [[nodiscard]] bool operator==(const UiCaptureScreen&) const noexcept =
        default;
};

// The native head owns actual text/image rendering. This callback is the
// equivalent of Avalonia's RenderTargetBitmap step and keeps -uishot's
// deterministic screen list and output/error contract testable without a
// window toolkit.
class UiCapture final {
public:
    static constexpr UiCaptureSize WindowSize{940, 560};
    static constexpr UiCaptureSize SmallPauseSize{560, 320};
    using CaptureHandler = std::function<bool(
        const UiCaptureScreen&, const std::filesystem::path&)>;

    [[nodiscard]] static std::vector<UiCaptureScreen> screens(
        const std::vector<std::string>& rooms);

    // Returns 0 when at least one screen was written, 1 when the backend is
    // unavailable, the output directory cannot be created, or every capture
    // failed. A failed individual screen never increments the result.
    [[nodiscard]] static int run(const std::filesystem::path& directory,
                                 const std::vector<std::string>& rooms,
                                 CaptureHandler capture,
                                 bool backend_available = true);
};

} // namespace fruityprime::launcher::gui
