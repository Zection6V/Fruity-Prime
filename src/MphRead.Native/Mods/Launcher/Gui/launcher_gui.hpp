#pragma once

#include "Mods/Launcher/native_launcher.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fruityprime::launcher::gui {

// Platform-neutral state shared by the launcher front ends. It mirrors the
// managed GuiLauncher page boundary without making the portable launcher
// depend on Avalonia or Win32 controls.
enum class Page : std::uint8_t {
    Splash,
    Home,
    GameFiles,
    MapPicker,
    DemoPicker,
    Settings,
    Pause,
};

enum class Action : std::uint8_t {
    FinishSplash,
    OpenHome,
    OpenGameFiles,
    OpenMapPicker,
    OpenDemoPicker,
    OpenSettings,
    OpenPause,
    Back,
};

[[nodiscard]] std::string_view page_name(Page page) noexcept;

class Model final {
public:
    explicit Model(Selection selection = {});

    [[nodiscard]] Page page() const noexcept { return page_; }
    [[nodiscard]] const Selection& selection() const noexcept {
        return selection_;
    }
    void set_selection(Selection selection) noexcept;
    void set_rom_path(std::string path);
    void set_room_name(std::string room);

    void set_game_files_ready(bool ready) noexcept;
    [[nodiscard]] bool game_files_ready() const noexcept {
        return game_files_ready_;
    }
    [[nodiscard]] bool can_start() const noexcept;

    // Returns false when a page is not reachable in the current first-run or
    // navigation state. Successful transitions are retained for Back().
    [[nodiscard]] bool navigate(Page page) noexcept;
    [[nodiscard]] bool dispatch(Action action) noexcept;

private:
    Selection selection_;
    Page page_ = Page::Splash;
    bool game_files_ready_ = false;
    std::vector<Page> history_;
};

// Portable counterpart of the managed GuiLauncher static lifecycle.  The
// actual window toolkit is initialized by a platform head; this object keeps
// the once-per-process setup, display probe, fallback, and nested-pump state
// identical for Win32, desktop POSIX, and Android callers.
struct Environment final {
    bool windows = false;
    bool macos = false;
    bool android = false;
    bool display = false;
    bool wayland = false;

    [[nodiscard]] bool has_display() const noexcept {
        return windows || macos || display || wayland;
    }
};

class GuiLauncher final {
public:
    using RunHandler = std::function<void()>;

    [[nodiscard]] bool ensure_setup(Environment environment) noexcept;
    [[nodiscard]] bool try_run(Environment environment,
                                RunHandler run = {});
    void mark_setup_failed() noexcept;
    void pump() noexcept;
    void reset() noexcept;

    [[nodiscard]] bool set_up() const noexcept { return set_up_; }
    [[nodiscard]] bool failed() const noexcept { return failed_; }
    [[nodiscard]] std::size_t pump_count() const noexcept {
        return pump_count_;
    }

private:
    bool set_up_ = false;
    bool failed_ = false;
    std::size_t pump_count_ = 0;
};

} // namespace fruityprime::launcher::gui
