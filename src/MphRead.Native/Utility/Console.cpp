#include "Utility/console_setup.hpp"

#include <array>
#include <clocale>
#include <iostream>
#include <locale>
#include <system_error>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#endif

namespace fruityprime::utility::console {
namespace {

std::filesystem::path current_launch_directory() noexcept {
    std::error_code error;
    const auto result = std::filesystem::current_path(error);
    return error ? std::filesystem::path{} : result;
}

std::filesystem::path g_launch_directory = current_launch_directory();

[[nodiscard]] std::filesystem::path executable_directory() noexcept {
#ifdef _WIN32
    std::array<wchar_t, 32'768> buffer{};
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length != 0 && length < buffer.size()) {
        const std::filesystem::path executable(
            std::wstring(buffer.data(), length));
        if (executable.has_parent_path()) {
            return executable.parent_path();
        }
    }
#else
    std::error_code error;
    const auto executable = std::filesystem::read_symlink(
        "/proc/self/exe", error);
    if (!error && executable.has_parent_path()) {
        return executable.parent_path();
    }
#endif
    return current_launch_directory();
}

#ifdef _WIN32

void enable_virtual_terminal() noexcept {
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output == nullptr || output == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD mode = 0;
    if (GetConsoleMode(output, &mode) != FALSE) {
        static_cast<void>(SetConsoleMode(
            output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING));
    }
}

#endif

} // namespace

void run() noexcept {
    // C#'s CultureInfo.InvariantCulture is the process's formatting contract
    // for command-line tools. The catch keeps startup usable if a host has
    // already replaced one of the standard stream locales.
    try {
        const auto invariant = std::locale::classic();
        std::locale::global(invariant);
        std::cin.imbue(invariant);
        std::cout.imbue(invariant);
        std::cerr.imbue(invariant);
    } catch (...) {
    }

    g_launch_directory = current_launch_directory();
    std::error_code error;
    const auto base = executable_directory();
    if (!base.empty()) {
        std::filesystem::current_path(base, error);
    }

#ifdef _WIN32
    enable_virtual_terminal();
#endif
}

const std::filesystem::path& launch_directory() noexcept {
    return g_launch_directory;
}

std::filesystem::path resolve_launch_path(
    const std::filesystem::path& value) {
    if (value.is_absolute() || g_launch_directory.empty()) {
        return value;
    }
    return (g_launch_directory / value).lexically_normal();
}

} // namespace fruityprime::utility::console

namespace fruityprime {

void ConsoleSetup::Run() noexcept {
    utility::console::run();
}

const std::filesystem::path& ConsoleSetup::LaunchDirectory() noexcept {
    return utility::console::launch_directory();
}

std::filesystem::path ConsoleSetup::ResolveLaunchPath(
    const std::filesystem::path& value) {
    return utility::console::resolve_launch_path(value);
}

} // namespace fruityprime
