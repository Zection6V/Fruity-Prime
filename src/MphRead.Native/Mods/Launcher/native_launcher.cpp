#include "Mods/Launcher/native_launcher.hpp"

#include "Mods/Launcher/Gui/launcher_gui.hpp"
#include "Mods/Launcher/Gui/win32_home_window.hpp"
#include "Mods/Launcher/Portable/launcher_prefs.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

namespace fruityprime::launcher {
namespace {

void load_selection_preferences(
    const std::filesystem::path& executable_directory,
    Selection& selection) {
    const Preferences preferences = load_preferences(executable_directory);
    if (!preferences.rom_path.empty()) {
        selection.rom_path = preferences.rom_path;
    }
    if (!preferences.room_name.empty()) {
        selection.room_name = preferences.room_name;
    }
    if (!preferences.player_name.empty()) {
        selection.player_name = preferences.player_name;
    }
    selection.hunter = static_cast<std::uint8_t>(preferences.last_hunter);
    selection.bots = std::clamp(preferences.bots, 0, 7);
    selection.bot_level = std::clamp(preferences.bot_level, 0, 2);
    selection.mode = preferences.mode;
    selection.time_limit_seconds = preferences.time_limit_seconds;
    selection.point_goal = preferences.point_goal;
    selection.friendly_fire = preferences.friendly_fire;
}

void save_selection_preferences(
    const std::filesystem::path& executable_directory,
    const Selection& selection) {
    Preferences preferences = load_preferences(executable_directory);
    preferences.rom_path = selection.rom_path;
    preferences.room_name = selection.room_name;
    preferences.player_name = selection.player_name;
    preferences.last_hunter = static_cast<metadata::Hunter>(selection.hunter);
    preferences.bots = selection.bots;
    preferences.bot_level = std::clamp(selection.bot_level, 0, 2);
    preferences.mode = selection.mode;
    preferences.time_limit_seconds = selection.time_limit_seconds;
    preferences.point_goal = selection.point_goal;
    preferences.friendly_fire = selection.friendly_fire;
    (void)save_preferences(executable_directory, preferences);
}

#ifdef _WIN32
[[nodiscard]] std::string pick_rom() {
    std::array<char, MAX_PATH * 4> buffer{};
    OPENFILENAMEA dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter =
        "Nintendo DS ROM (*.nds)\0*.nds\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
        | OFN_HIDEREADONLY;
    dialog.lpstrTitle = "Select your Metroid Prime Hunters ROM";
    if (GetOpenFileNameA(&dialog) == FALSE) {
        return {};
    }
    return buffer.data();
}

enum LauncherControl : int {
    RomPathControl = 100,
    RoomControl = 101,
    PlayerNameControl = 102,
    HunterControl = 103,
    BotsControl = 104,
    SelectRomControl = 105,
    StartControl = 106,
    CancelControl = 107,
    ModeControl = 108,
    TimeLimitControl = 109,
    PointGoalControl = 110,
    FriendlyFireControl = 111,
    BotLevelControl = 112
};

struct LauncherDialog {
    std::filesystem::path directory;
    Selection selection;
    HWND window = nullptr;
    HWND rom_path = nullptr;
    HWND room = nullptr;
    HWND player_name = nullptr;
    HWND hunter = nullptr;
    HWND bots = nullptr;
    HWND bot_level = nullptr;
    HWND mode = nullptr;
    HWND time_limit = nullptr;
    HWND point_goal = nullptr;
    HWND friendly_fire = nullptr;
    bool accepted = false;
};

HWND create_control(const char* type, const char* text, DWORD style,
                    int x, int y, int width, int height, HWND parent, int id) {
    const HWND control = CreateWindowExA(
        std::string_view(type) == "EDIT" ? WS_EX_CLIENTEDGE : 0,
        type, text, style, x, y, width, height, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleA(nullptr), nullptr);
    if (control != nullptr) {
        const auto font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        SendMessageA(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    return control;
}

void set_control_text(HWND control, const std::string& text) {
    if (control != nullptr) {
        SetWindowTextA(control, text.c_str());
    }
}

[[nodiscard]] std::string control_text(HWND control) {
    if (control == nullptr) {
        return {};
    }
    const int length = GetWindowTextLengthA(control);
    std::string value(static_cast<std::size_t>(length) + 1, '\0');
    GetWindowTextA(control, value.data(), length + 1);
    value.resize(std::strlen(value.c_str()));
    return value;
}

void create_launcher_controls(LauncherDialog& dialog) {
    constexpr DWORD LabelStyle = WS_CHILD | WS_VISIBLE;
    constexpr DWORD EditStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP
        | ES_AUTOHSCROLL;
    create_control("STATIC", "Nintendo DS ROM", LabelStyle,
                   20, 18, 180, 20, dialog.window, 0);
    dialog.rom_path = create_control(
        "EDIT", "", EditStyle | ES_READONLY, 20, 40, 430, 24,
        dialog.window, RomPathControl);
    create_control("BUTTON", "Select...", WS_CHILD | WS_VISIBLE | WS_TABSTOP
                       | BS_PUSHBUTTON,
                   460, 40, 80, 24, dialog.window, SelectRomControl);

    create_control("STATIC", "Room", LabelStyle, 20, 80, 180, 20,
                   dialog.window, 0);
    dialog.room = create_control(
        "EDIT", "", EditStyle, 20, 102, 520, 24, dialog.window,
        RoomControl);
    create_control("STATIC", "Player name", LabelStyle, 20, 142, 180, 20,
                   dialog.window, 0);
    dialog.player_name = create_control(
        "EDIT", "", EditStyle, 20, 164, 520, 24, dialog.window,
        PlayerNameControl);
    create_control("STATIC", "Hunter (0-255)", LabelStyle, 20, 204, 180,
                   20, dialog.window, 0);
    dialog.hunter = create_control(
        "EDIT", "", EditStyle | ES_NUMBER, 20, 226, 150, 24,
        dialog.window, HunterControl);
    create_control("STATIC", "Offline bots (0-7)", LabelStyle,
                   210, 204, 180, 20, dialog.window, 0);
    dialog.bots = create_control(
        "EDIT", "", EditStyle | ES_NUMBER, 210, 226, 150, 24,
        dialog.window, BotsControl);
    create_control("STATIC", "Bot skill (0-2)", LabelStyle,
                   400, 204, 120, 20, dialog.window, 0);
    dialog.bot_level = create_control(
        "EDIT", "", EditStyle | ES_NUMBER, 400, 226, 120, 24,
        dialog.window, BotLevelControl);
    create_control("STATIC", "Mode (3 Battle, 4 Teams)", LabelStyle,
                   20, 264, 180, 20, dialog.window, 0);
    dialog.mode = create_control(
        "EDIT", "", EditStyle | ES_NUMBER, 20, 286, 150, 24,
        dialog.window, ModeControl);
    create_control("STATIC", "Time limit (seconds)", LabelStyle,
                   210, 264, 180, 20, dialog.window, 0);
    dialog.time_limit = create_control(
        "EDIT", "", EditStyle, 210, 286, 150, 24,
        dialog.window, TimeLimitControl);
    create_control("STATIC", "Point goal", LabelStyle,
                   400, 264, 120, 20, dialog.window, 0);
    dialog.point_goal = create_control(
        "EDIT", "", EditStyle | ES_NUMBER, 400, 286, 120, 24,
        dialog.window, PointGoalControl);
    dialog.friendly_fire = create_control(
        "BUTTON", "Friendly fire", WS_CHILD | WS_VISIBLE | WS_TABSTOP
            | BS_AUTOCHECKBOX,
        20, 326, 180, 24, dialog.window, FriendlyFireControl);
    create_control("BUTTON", "Start", WS_CHILD | WS_VISIBLE | WS_TABSTOP
                       | BS_DEFPUSHBUTTON,
                   350, 370, 90, 30, dialog.window, StartControl);
    create_control("BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP
                       | BS_PUSHBUTTON,
                   450, 370, 90, 30, dialog.window, CancelControl);

    set_control_text(dialog.rom_path, dialog.selection.rom_path);
    set_control_text(dialog.room, dialog.selection.room_name);
    set_control_text(dialog.player_name, dialog.selection.player_name);
    set_control_text(dialog.hunter, std::to_string(dialog.selection.hunter));
    set_control_text(dialog.bots, std::to_string(dialog.selection.bots));
    set_control_text(dialog.bot_level,
                     std::to_string(dialog.selection.bot_level));
    set_control_text(dialog.mode, std::to_string(dialog.selection.mode));
    set_control_text(dialog.time_limit,
                     std::to_string(dialog.selection.time_limit_seconds));
    set_control_text(dialog.point_goal,
                     std::to_string(dialog.selection.point_goal));
    SendMessageA(dialog.friendly_fire, BM_SETCHECK,
                 dialog.selection.friendly_fire ? BST_CHECKED : BST_UNCHECKED,
                 0);
}

[[nodiscard]] bool read_launcher_controls(LauncherDialog& dialog) {
    const std::string rom_path = control_text(dialog.rom_path);
    std::error_code file_error;
    if (rom_path.empty()
        || !std::filesystem::is_regular_file(rom_path, file_error)) {
        MessageBoxA(dialog.window, "Select a valid .nds ROM first.",
                    "Fruity Prime", MB_OK | MB_ICONWARNING);
        return false;
    }
    const std::string room = control_text(dialog.room);
    if (room.empty()) {
        MessageBoxA(dialog.window, "Room cannot be empty.", "Fruity Prime",
                    MB_OK | MB_ICONWARNING);
        return false;
    }
    int hunter = 0;
    int bots = 0;
    int bot_level = 1;
    int mode = 0;
    float time_limit = 0.0F;
    int point_goal = 0;
    try {
        hunter = std::stoi(control_text(dialog.hunter));
        bots = std::stoi(control_text(dialog.bots));
        bot_level = std::stoi(control_text(dialog.bot_level));
        mode = std::stoi(control_text(dialog.mode));
        time_limit = std::stof(control_text(dialog.time_limit));
        point_goal = std::stoi(control_text(dialog.point_goal));
    } catch (...) {
        MessageBoxA(dialog.window,
                    "Hunter, bot, mode, time, and goal values must be numbers.",
                    "Fruity Prime", MB_OK | MB_ICONWARNING);
        return false;
    }
    if (bot_level < 0 || bot_level > 2 || mode < 0 || mode > 255
        || !std::isfinite(time_limit)
        || time_limit < 0.0F || point_goal < 0 || point_goal > 65'535) {
        MessageBoxA(dialog.window, "Match values are outside their valid range.",
                    "Fruity Prime", MB_OK | MB_ICONWARNING);
        return false;
    }
    dialog.selection.rom_path = rom_path;
    dialog.selection.room_name = room;
    dialog.selection.player_name = control_text(dialog.player_name);
    if (dialog.selection.player_name.empty()) {
        dialog.selection.player_name = "Player";
    }
    dialog.selection.hunter = static_cast<std::uint8_t>(
        std::clamp(hunter, 0, 255));
    dialog.selection.bots = std::clamp(bots, 0, 7);
    dialog.selection.bot_level = bot_level;
    dialog.selection.mode = static_cast<std::uint8_t>(mode);
    dialog.selection.time_limit_seconds = time_limit;
    dialog.selection.point_goal = static_cast<std::uint16_t>(point_goal);
    dialog.selection.friendly_fire = SendMessageA(
        dialog.friendly_fire, BM_GETCHECK, 0, 0) == BST_CHECKED;
    return true;
}

LRESULT CALLBACK launcher_window_proc(HWND window, UINT message,
                                      WPARAM wparam, LPARAM lparam) {
    auto* dialog = reinterpret_cast<LauncherDialog*>(
        GetWindowLongPtrA(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTA*>(lparam);
        dialog = static_cast<LauncherDialog*>(create->lpCreateParams);
        SetWindowLongPtrA(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(dialog));
        dialog->window = window;
    }
    if (message == WM_CREATE && dialog != nullptr) {
        create_launcher_controls(*dialog);
        return 0;
    }
    if (message == WM_COMMAND && dialog != nullptr
        && HIWORD(wparam) == BN_CLICKED) {
        switch (LOWORD(wparam)) {
        case SelectRomControl: {
            const std::string path = pick_rom();
            if (!path.empty()) {
                set_control_text(dialog->rom_path, path);
            }
            return 0;
        }
        case StartControl:
            if (read_launcher_controls(*dialog)) {
                save_selection_preferences(dialog->directory, dialog->selection);
                dialog->accepted = true;
                DestroyWindow(window);
            }
            return 0;
        case CancelControl:
            DestroyWindow(window);
            return 0;
        default:
            break;
        }
    }
    if (message == WM_CLOSE) {
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

[[nodiscard]] bool run_launcher_dialog(
    const std::filesystem::path& directory, Selection& selection) {
    constexpr char ClassName[] = "FruityPrimeNativeLauncher";
    WNDCLASSEXA window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = launcher_window_proc;
    window_class.hInstance = GetModuleHandleA(nullptr);
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(
        static_cast<INT_PTR>(COLOR_WINDOW + 1));
    window_class.lpszClassName = ClassName;
    if (RegisterClassExA(&window_class) == 0) {
        return false;
    }

    LauncherDialog dialog{directory, selection};
    const int width = 560;
    const int height = 450;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    const HWND window = CreateWindowExA(
        WS_EX_DLGMODALFRAME, ClassName, "Fruity Prime", WS_OVERLAPPED
            | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, width, height, nullptr, nullptr, window_class.hInstance,
        &dialog);
    if (window == nullptr) {
        UnregisterClassA(ClassName, window_class.hInstance);
        return false;
    }
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageA(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    UnregisterClassA(ClassName, window_class.hInstance);
    if (dialog.accepted) {
        selection = dialog.selection;
    }
    return dialog.accepted;
}
#endif

} // namespace

std::optional<Selection> choose_game(
    const std::filesystem::path& executable_directory,
    std::string_view default_room, bool interactive, Selection defaults) {
    Selection selection = std::move(defaults);
    if (selection.room_name.empty()) {
        selection.room_name = std::string(default_room);
    }
    load_selection_preferences(executable_directory, selection);
#ifdef _WIN32
    if (interactive) {
        // The front screen first, exactly as the managed launcher opens on
        // it: with no cartridge dump it is the setup card and there is nothing
        // else to reach, and with one it is the home card.  Closing it is
        // "quit" rather than "start with nothing", which is what running the
        // program used to do -- an empty room with a placeholder triangle in
        // it, and no way to say where the .nds was.
        bool open_details = false;
        if (!gui::win32::run_front_screen(executable_directory, selection,
                                          &open_details)) {
            return std::nullopt;
        }
        save_selection_preferences(executable_directory, selection);
        // The host, join, demo and settings cards are all drawn by the front
        // screen now, so pressing Start there is the last thing a player does:
        // the older dialog opens only if that screen asks for it.
        if (open_details
            && !run_launcher_dialog(executable_directory, selection)) {
            return std::nullopt;
        }
        return selection;
    }
#else
    if (interactive) {
        return std::nullopt;
    }
#endif
    std::error_code file_error;
    if (selection.rom_path.empty()
        || !std::filesystem::is_regular_file(selection.rom_path, file_error)) {
#ifdef _WIN32
        return std::nullopt;
#else
        return std::nullopt;
#endif
    }
    if (selection.rom_path.empty()) {
        return std::nullopt;
    }
    gui::Model frontend(selection);
    frontend.set_game_files_ready(true);
    if (!frontend.can_start()) {
        return std::nullopt;
    }
    save_selection_preferences(executable_directory, selection);
    return selection;
}

} // namespace fruityprime::launcher
