#pragma once

#include "Mods/Launcher/Portable/launch_plan.hpp"
#include "Mods/settings.hpp"

#include <functional>
#include <string>
#include <vector>

namespace fruityprime::launcher::gui {

// The desktop frame around HomeView.  Rendering and card behavior remain in
// the shared home model; this value type carries only the window chrome and
// the plan/done hand-off needed by a native head.
class HomeWindow final {
public:
    static constexpr double Width = 940.0;
    static constexpr double Height = 560.0;
    static constexpr double MinWidth = 780.0;
    static constexpr double MinHeight = 480.0;

    using DoneHandler = std::function<void(const LaunchPlan&)>;

    HomeWindow(settings::MenuSettings settings, std::vector<std::string> rooms,
               std::string product_name = "Fruity Prime");

    [[nodiscard]] const std::string& title() const noexcept { return title_; }
    [[nodiscard]] const settings::MenuSettings& settings() const noexcept {
        return settings_;
    }
    [[nodiscard]] const std::vector<std::string>& rooms() const noexcept {
        return rooms_;
    }
    [[nodiscard]] const LaunchPlan& plan() const noexcept { return plan_; }
    [[nodiscard]] bool closed() const noexcept { return closed_; }
    [[nodiscard]] bool centered() const noexcept { return true; }
    [[nodiscard]] bool decorated() const noexcept { return true; }

    void set_done_handler(DoneHandler handler);
    void finish(LaunchPlan plan);
    void close() noexcept { closed_ = true; }
    void reset() noexcept;

private:
    std::string title_;
    settings::MenuSettings settings_;
    std::vector<std::string> rooms_;
    LaunchPlan plan_;
    DoneHandler done_handler_;
    bool closed_ = false;
};

} // namespace fruityprime::launcher::gui
