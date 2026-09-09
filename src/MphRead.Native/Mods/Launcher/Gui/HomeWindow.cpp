#include "Mods/Launcher/Gui/home_window.hpp"

#include <utility>

namespace fruityprime::launcher::gui {

HomeWindow::HomeWindow(settings::MenuSettings settings,
                       std::vector<std::string> rooms,
                       std::string product_name)
    : title_(std::move(product_name)), settings_(std::move(settings)),
      rooms_(std::move(rooms)) {}

void HomeWindow::set_done_handler(DoneHandler handler) {
    done_handler_ = std::move(handler);
}

void HomeWindow::finish(LaunchPlan plan) {
    if (closed_) {
        return;
    }
    plan_ = std::move(plan);
    closed_ = true;
    if (done_handler_) {
        done_handler_(plan_);
    }
}

void HomeWindow::reset() noexcept {
    plan_ = {};
    closed_ = false;
}

} // namespace fruityprime::launcher::gui
