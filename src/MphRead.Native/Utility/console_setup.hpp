#pragma once

#include <filesystem>

namespace fruityprime::utility::console {

// Matches Utility.ConsoleSetup.Run: remember the directory from which the
// process was launched, move path-based runtime work beside the executable,
// and put the terminal in invariant/VT mode where the platform supports it.
void run() noexcept;

[[nodiscard]] const std::filesystem::path& launch_directory() noexcept;

[[nodiscard]] std::filesystem::path resolve_launch_path(
    const std::filesystem::path& value);

} // namespace fruityprime::utility::console

namespace fruityprime {

class ConsoleSetup final {
public:
    static void Run() noexcept;
    [[nodiscard]] static const std::filesystem::path& LaunchDirectory()
        noexcept;
    [[nodiscard]] static std::filesystem::path ResolveLaunchPath(
        const std::filesystem::path& value);
};

} // namespace fruityprime

namespace MphReadNative {
using ConsoleSetup = ::fruityprime::ConsoleSetup;
}
