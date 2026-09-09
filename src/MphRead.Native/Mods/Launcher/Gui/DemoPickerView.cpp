#include "Mods/Launcher/Gui/demo_picker_view.hpp"

#include <utility>

namespace fruityprime::launcher::gui {

DemoPickerView::DemoPickerView(std::vector<demo::Recording> demos,
                               std::filesystem::path directory)
    : directory_(std::move(directory)) {
    entries_.reserve(demos.size() + 2);
    demo_paths_.reserve(demos.size());
    for (const auto& recording : demos) {
        demo_paths_.push_back(recording.path);
        entries_.push_back(Entry{
            EntryKind::Demo,
            recording.room.empty() ? recording.file_name() : recording.room,
            demo::describe(recording),
            15.0,
            std::nullopt,
            0.0,
            0.0});
    }
    if (demos.empty()) {
        empty_note_ =
            "Nothing recorded yet. Recordings are made from the pause menu "
            "during an online match, and are written to:\n"
            + directory_.string();
    }

    entries_.push_back(Entry{
        EntryKind::Import,
        "Open a file...",
        "A demo from somewhere else on this device",
        13.0,
        palette().text_dim,
        10.0,
        0.0});
    entries_.push_back(Entry{
        EntryKind::Back,
        "Back",
        {},
        13.0,
        palette().text_dim,
        0.0,
        120.0});
}

void DemoPickerView::set_closed_handler(ClosedHandler handler) {
    closed_handler_ = std::move(handler);
}

bool DemoPickerView::activate(std::size_t index) {
    if (closed_ || index >= entries_.size()) {
        return false;
    }
    const Entry& entry = entries_[index];
    if (entry.kind == EntryKind::Demo) {
        if (index >= demo_paths_.size()) {
            return false;
        }
        path_ = demo_paths_[index];
    }
    if (entry.kind == EntryKind::Import) {
        import_requested_ = true;
    }
    finish();
    return true;
}

bool DemoPickerView::key_press(WidgetKey key) {
    if (closed_ || key != WidgetKey::Escape) {
        return false;
    }
    finish();
    return true;
}

void DemoPickerView::close() {
    finish();
}

void DemoPickerView::finish() {
    if (closed_) {
        return;
    }
    closed_ = true;
    if (closed_handler_) {
        closed_handler_();
    }
}

} // namespace fruityprime::launcher::gui
