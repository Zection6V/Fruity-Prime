#pragma once

#include "Mods/Network/demo_library.hpp"
#include "Formats/launcher_theme.hpp"
#include "Mods/Launcher/Gui/launcher_widgets.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace fruityprime::launcher::gui {

class DemoPickerView final {
public:
    enum class EntryKind {
        Demo,
        Import,
        Back,
    };

    struct Entry final {
        EntryKind kind = EntryKind::Back;
        std::string title;
        std::string subtitle;
        double title_size = 13.0;
        std::optional<Color> accent;
        double top_margin = 0.0;
        double width = 0.0;
    };

    using ClosedHandler = std::function<void()>;

    DemoPickerView(std::vector<demo::Recording> demos,
                   std::filesystem::path directory);

    [[nodiscard]] const std::vector<Entry>& entries() const noexcept {
        return entries_;
    }
    [[nodiscard]] const std::filesystem::path& directory() const noexcept {
        return directory_;
    }
    [[nodiscard]] const std::optional<std::filesystem::path>& path()
        const noexcept {
        return path_;
    }
    [[nodiscard]] bool import_requested() const noexcept {
        return import_requested_;
    }
    [[nodiscard]] bool closed() const noexcept { return closed_; }
    [[nodiscard]] const std::string& empty_note() const noexcept {
        return empty_note_;
    }

    // The first focusable control is the first demo, or the import entry when
    // the recording folder is empty.  The explanatory note is not focusable.
    [[nodiscard]] std::size_t first_focus_index() const noexcept {
        return first_focus_index_;
    }

    void set_closed_handler(ClosedHandler handler);
    [[nodiscard]] bool activate(std::size_t index);
    [[nodiscard]] bool key_press(WidgetKey key);
    void close();

private:
    void finish();

    std::filesystem::path directory_;
    std::vector<Entry> entries_;
    std::vector<std::filesystem::path> demo_paths_;
    std::optional<std::filesystem::path> path_;
    std::string empty_note_;
    std::size_t first_focus_index_ = 0;
    bool import_requested_ = false;
    bool closed_ = false;
    ClosedHandler closed_handler_;
};

} // namespace fruityprime::launcher::gui
