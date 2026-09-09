#pragma once

#include <csignal>

namespace fruityprime::mods {

using ShutdownAction = void (*)() noexcept;

// Owns the SIGINT/SIGTERM and Ctrl+C-equivalent lifecycle for a long-running
// process. The action is fired once, then the original handlers are restored
// when the guard leaves scope so a later command in the same host is clean.
class ShutdownSignals {
public:
    ShutdownSignals() noexcept = default;
    ShutdownSignals(const ShutdownSignals&) = delete;
    ShutdownSignals& operator=(const ShutdownSignals&) = delete;
    ~ShutdownSignals() noexcept;

    void on_shutdown(ShutdownAction action) noexcept;

private:
    using Handler = void (*)(int);

    bool installed_ = false;
    Handler previous_int_ = SIG_DFL;
#ifdef SIGTERM
    Handler previous_term_ = SIG_DFL;
#endif
};

} // namespace fruityprime::mods
