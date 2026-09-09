#include "Mods/Launcher/Gui/ui_capture.hpp"

#include <system_error>
#include <utility>

namespace fruityprime::launcher::gui {

std::vector<UiCaptureScreen> UiCapture::screens(
    const std::vector<std::string>& rooms) {
    std::vector<UiCaptureScreen> result;
    result.reserve(rooms.empty() ? 8 : 9);
    result.push_back({"home", WindowSize});
    result.push_back({"settings", WindowSize});
    result.push_back({"settings-credits", WindowSize});
    if (!rooms.empty()) {
        result.push_back({"mappicker", WindowSize});
    }
    result.push_back({"demopicker", WindowSize});
    result.push_back({"demopicker-empty", WindowSize});
    result.push_back({"pausemenu", WindowSize});
    result.push_back({"pausemenu-small", SmallPauseSize});
    result.push_back({"serverbrowser", WindowSize});
    return result;
}

int UiCapture::run(const std::filesystem::path& directory,
                   const std::vector<std::string>& rooms,
                   CaptureHandler capture, bool backend_available) {
    if (!backend_available || !capture) {
        return 1;
    }
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        return 1;
    }
    int written = 0;
    for (const UiCaptureScreen& screen : screens(rooms)) {
        const auto output = directory / (screen.name + ".png");
        if (capture(screen, output)) {
            ++written;
        }
    }
    return written > 0 ? 0 : 1;
}

} // namespace fruityprime::launcher::gui
