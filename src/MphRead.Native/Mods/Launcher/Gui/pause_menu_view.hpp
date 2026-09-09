#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace fruityprime::launcher::gui {

class PauseMenuView final {
public:
    enum class EntryKind {
        Resume,
        WindowMode,
        Settings,
        Spectate,
        Rejoin,
        RecordToggle,
        Leave,
        Quit,
    };

    struct Options final {
        bool offer_window_mode = false;
        bool demo_playback_active = false;
        bool spectator = false;
        bool can_spectate = false;
        bool network_active = false;
        bool recording = false;
        bool fullscreen = false;
    };

    struct Entry final {
        EntryKind kind = EntryKind::Resume;
        std::string title;
        double title_size = 17.0;
    };

    using ActionHandler = std::function<void(EntryKind)>;

    explicit PauseMenuView(Options options);

    [[nodiscard]] const std::vector<Entry>& entries() const noexcept {
        return entries_;
    }
    [[nodiscard]] const Options& options() const noexcept { return options_; }
    [[nodiscard]] double panel_width() const noexcept { return 420.0; }
    [[nodiscard]] double caption_height() const noexcept { return 34.0; }
    [[nodiscard]] double needed_height() const noexcept {
        return needed_height_;
    }
    [[nodiscard]] double scale() const noexcept { return scale_; }
    [[nodiscard]] std::size_t focused_index() const noexcept {
        return focused_index_;
    }

    void set_action_handler(ActionHandler handler);
    void set_fullscreen(bool fullscreen);
    void focus_resume() noexcept { focused_index_ = 0; }
    void fit_to_host(double height) noexcept;

    [[nodiscard]] bool activate(std::size_t index);

private:
    void add(EntryKind kind, std::string title);

    Options options_;
    std::vector<Entry> entries_;
    ActionHandler action_handler_;
    double needed_height_ = 0.0;
    double scale_ = 1.0;
    std::size_t focused_index_ = 0;
};

} // namespace fruityprime::launcher::gui
