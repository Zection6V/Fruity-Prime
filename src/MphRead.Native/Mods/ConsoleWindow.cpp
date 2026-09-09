#include "Mods/console_window.hpp"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <iostream>

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

[[nodiscard]] bool streams_redirected() noexcept {
    return redirected(GetStdHandle(STD_OUTPUT_HANDLE))
        || redirected(GetStdHandle(STD_INPUT_HANDLE));
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
    if (GetConsoleWindow() == nullptr || streams_redirected()) {
        return false;
    }
    // A one-process console is the one AllocConsole created for a
    // double-click. An attached terminal also contains its shell process.
    DWORD processes[4]{};
    return GetConsoleProcessList(processes, 4) == 1;
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

} // namespace fruityprime::mods::console

#endif
