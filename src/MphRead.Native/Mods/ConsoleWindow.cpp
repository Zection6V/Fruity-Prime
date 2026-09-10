#include "Mods/console_window.hpp"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <cctype>
#include <iostream>
#include <string_view>

namespace fruityprime::mods::console {
namespace {

constexpr int AttachParentProcess = -1;

[[nodiscard]] bool redirected(HANDLE handle) noexcept {
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    const DWORD type = GetFileType(handle);
    return type == FILE_TYPE_PIPE || type == FILE_TYPE_DISK;
}

[[nodiscard]] bool output_redirected() noexcept {
    return redirected(GetStdHandle(STD_OUTPUT_HANDLE));
}

[[nodiscard]] bool streams_redirected() noexcept {
    return output_redirected()
        || redirected(GetStdHandle(STD_INPUT_HANDLE));
}

[[nodiscard]] bool has_flag(std::span<const std::string> args,
                            std::string_view name) noexcept {
    for (const std::string& argument : args) {
        std::size_t first = 0;
        while (first < argument.size() && argument[first] == '-') {
            ++first;
        }
        if (argument.size() - first != name.size()) {
            continue;
        }
        bool equal = true;
        for (std::size_t index = 0; index < name.size(); ++index) {
            const auto left = static_cast<unsigned char>(argument[first + index]);
            const auto right = static_cast<unsigned char>(name[index]);
            if (std::tolower(left) != std::tolower(right)) {
                equal = false;
                break;
            }
        }
        if (equal) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool rebind() noexcept {
    FILE* stream = nullptr;
    if (freopen_s(&stream, "CONOUT$", "w", stdout) != 0
        || freopen_s(&stream, "CONOUT$", "w", stderr) != 0
        || freopen_s(&stream, "CONIN$", "r", stdin) != 0) {
        return false;
    }
    // The streams were created before the console existed when this is a GUI
    // binary. Resynchronize iostreams after replacing their FILEs.
    std::ios::sync_with_stdio(true);

    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (output != nullptr && output != INVALID_HANDLE_VALUE
        && GetConsoleMode(output, &mode) != FALSE) {
        static_cast<void>(SetConsoleMode(
            output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING));
    }
    return true;
}

} // namespace

bool owns_its_console() noexcept {
    if (output_redirected()) {
        return false;
    }
    // A one-process console is the one AllocConsole created for a
    // double-click. An attached terminal also contains its shell process.
    DWORD processes[4]{};
    return GetConsoleProcessList(processes, 4) == 1;
}

void prepare(std::span<const std::string> args) noexcept {
    const bool forced = has_flag(args, "console");
    const bool gui_only = args.empty() || has_flag(args, "launcher");
    if (gui_only && !forced) {
        return;
    }
    static_cast<void>(show());
}

ShowResult show() noexcept {
    // This is the exact early exit used by the managed implementation: a
    // parent pipe already owns the output, so creating a window is harmful.
    if (streams_redirected()) {
        return {true, false};
    }
    if (const HWND window = GetConsoleWindow(); window != nullptr) {
        static_cast<void>(ShowWindow(window, SW_SHOW));
        return {true, owns_its_console()};
    }
    if (AttachConsole(AttachParentProcess) == FALSE
        && AllocConsole() == FALSE) {
        return {false, false};
    }
    if (!rebind()) {
        return {false, owns_its_console()};
    }
    return {true, owns_its_console()};
}

} // namespace fruityprime::mods::console

#else

namespace fruityprime::mods::console {

bool owns_its_console() noexcept {
    return false;
}

ShowResult show() noexcept {
    // Non-Windows targets already use a normal console or terminal.
    return {true, false};
}

void prepare(std::span<const std::string>) noexcept {
    // Non-Windows targets already have a normal console or terminal.
}

} // namespace fruityprime::mods::console

#endif
