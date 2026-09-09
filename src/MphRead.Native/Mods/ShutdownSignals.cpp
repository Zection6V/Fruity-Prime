#include "Mods/shutdown_signals.hpp"

#include <csignal>

namespace fruityprime::mods {
namespace {

ShutdownAction g_action = nullptr;
volatile std::sig_atomic_t g_fired = 0;

void signal_handler(int signal) {
    // Keep the first signal on the orderly stop path. A second signal gets
    // the platform default, matching the managed handler's "one chance to
    // tidy up" contract.
    if (g_fired != 0) {
        std::signal(signal, SIG_DFL);
        std::raise(signal);
        return;
    }
    g_fired = 1;
    if (g_action != nullptr) {
        g_action();
    }
}

} // namespace

void ShutdownSignals::on_shutdown(ShutdownAction action) noexcept {
    if (installed_) {
        return;
    }
    g_action = action;
    g_fired = 0;
    previous_int_ = std::signal(SIGINT, signal_handler);
#ifdef SIGTERM
    previous_term_ = std::signal(SIGTERM, signal_handler);
#endif
    installed_ = true;
}

ShutdownSignals::~ShutdownSignals() noexcept {
    if (!installed_) {
        return;
    }
    std::signal(SIGINT, previous_int_);
#ifdef SIGTERM
    std::signal(SIGTERM, previous_term_);
#endif
    g_action = nullptr;
    g_fired = 0;
}

} // namespace fruityprime::mods
