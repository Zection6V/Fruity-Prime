#include "Mods/Launcher/Gui/pause_menu_view.hpp"

#include <algorithm>
#include <utility>

namespace fruityprime::launcher::gui {

PauseMenuView::PauseMenuView(Options options) : options_(options) {
    entries_.reserve(8);
    add(EntryKind::Resume, "Resume");
    if (options_.offer_window_mode) {
        add(EntryKind::WindowMode,
            options_.fullscreen ? "Windowed" : "Fullscreen");
    }
    add(EntryKind::Settings, "Settings");
    if (!options_.demo_playback_active) {
        if (options_.spectator) {
            add(EntryKind::Rejoin, "Rejoin match");
        } else if (options_.can_spectate) {
            add(EntryKind::Spectate, "Spectate");
        }
        if (options_.network_active) {
            add(EntryKind::RecordToggle,
                options_.recording ? "Stop recording" : "Record demo");
        }
    }
    add(EntryKind::Leave, "Leave match");
    add(EntryKind::Quit, "Quit");

    constexpr double panel_padding = 18.0 + 18.0 + 12.0 + 12.0;
    constexpr double stack_spacing = 4.0;
    constexpr double entry_height = 42.0;
    needed_height_ = panel_padding + caption_height() + entry_height
        * static_cast<double>(entries_.size())
        + stack_spacing * static_cast<double>(entries_.size() + 1);
}

void PauseMenuView::set_action_handler(ActionHandler handler) {
    action_handler_ = std::move(handler);
}

void PauseMenuView::set_fullscreen(bool fullscreen) {
    options_.fullscreen = fullscreen;
    for (auto& entry : entries_) {
        if (entry.kind == EntryKind::WindowMode) {
            entry.title = fullscreen ? "Windowed" : "Fullscreen";
            break;
        }
    }
}

void PauseMenuView::fit_to_host(double height) noexcept {
    if (height <= 0.0 || needed_height_ <= 0.0) {
        return;
    }
    scale_ = std::clamp(height / needed_height_, 0.5, 1.0);
}

bool PauseMenuView::activate(std::size_t index) {
    if (index >= entries_.size()) {
        return false;
    }
    if (action_handler_) {
        action_handler_(entries_[index].kind);
    }
    return true;
}

void PauseMenuView::add(EntryKind kind, std::string title) {
    entries_.push_back(Entry{kind, std::move(title), 17.0});
}

} // namespace fruityprime::launcher::gui
