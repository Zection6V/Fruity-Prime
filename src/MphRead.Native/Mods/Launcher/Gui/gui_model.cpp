#include "Mods/Launcher/Gui/launcher_gui.hpp"

#include <utility>

namespace fruityprime::launcher::gui {

std::string_view page_name(Page page) noexcept {
    switch (page) {
    case Page::Splash:
        return "splash";
    case Page::Home:
        return "home";
    case Page::GameFiles:
        return "game-files";
    case Page::MapPicker:
        return "map-picker";
    case Page::DemoPicker:
        return "demo-picker";
    case Page::Settings:
        return "settings";
    case Page::Pause:
        return "pause";
    }
    return "unknown";
}

Model::Model(Selection selection) : selection_(std::move(selection)) {}

void Model::set_selection(Selection selection) noexcept {
    selection_ = std::move(selection);
}

void Model::set_rom_path(std::string path) {
    selection_.rom_path = std::move(path);
}

void Model::set_room_name(std::string room) {
    selection_.room_name = std::move(room);
}

void Model::set_game_files_ready(bool ready) noexcept {
    game_files_ready_ = ready;
    if (!ready && page_ != Page::Splash && page_ != Page::GameFiles) {
        history_.clear();
        page_ = Page::GameFiles;
    }
}

bool Model::can_start() const noexcept {
    return game_files_ready_ && !selection_.rom_path.empty()
        && !selection_.room_name.empty();
}

bool Model::navigate(Page page) noexcept {
    if (page == Page::Splash || page == page_) {
        return page == page_;
    }
    if (!game_files_ready_ && page != Page::GameFiles) {
        return false;
    }
    history_.push_back(page_);
    page_ = page;
    return true;
}

bool Model::dispatch(Action action) noexcept {
    switch (action) {
    case Action::FinishSplash:
        return navigate(game_files_ready_ ? Page::Home : Page::GameFiles);
    case Action::OpenHome:
        return navigate(Page::Home);
    case Action::OpenGameFiles:
        return navigate(Page::GameFiles);
    case Action::OpenMapPicker:
        return navigate(Page::MapPicker);
    case Action::OpenDemoPicker:
        return navigate(Page::DemoPicker);
    case Action::OpenSettings:
        return navigate(Page::Settings);
    case Action::OpenPause:
        return navigate(Page::Pause);
    case Action::Back:
        if (history_.empty()) {
            return false;
        }
        page_ = history_.back();
        history_.pop_back();
        return true;
    }
    return false;
}

} // namespace fruityprime::launcher::gui
