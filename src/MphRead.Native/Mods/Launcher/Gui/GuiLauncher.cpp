#include "Mods/Launcher/Gui/launcher_gui.hpp"

namespace fruityprime::launcher::gui {

bool GuiLauncher::ensure_setup(Environment environment) noexcept {
    if (set_up_) {
        return true;
    }
    if (failed_ || environment.android || !environment.has_display()) {
        return false;
    }
    // Toolkit-specific SetupWithoutStarting/Android activity setup belongs to
    // the platform head. Reaching this point is the managed Probe + setup
    // success edge and is intentionally remembered for later visits.
    set_up_ = true;
    return true;
}

bool GuiLauncher::try_run(Environment environment, RunHandler run) {
    if (!ensure_setup(environment)) {
        return false;
    }
    try {
        if (run) {
            run();
        }
        return true;
    } catch (...) {
        // The managed launcher reports the exception and falls back to the
        // text launcher, but a failed run does not poison a toolkit that was
        // already initialized for a later visit.
        return false;
    }
}

void GuiLauncher::mark_setup_failed() noexcept {
    set_up_ = false;
    failed_ = true;
}

void GuiLauncher::pump() noexcept {
    if (set_up_) {
        ++pump_count_;
    }
}

void GuiLauncher::reset() noexcept {
    set_up_ = false;
    failed_ = false;
    pump_count_ = 0;
}

} // namespace fruityprime::launcher::gui
