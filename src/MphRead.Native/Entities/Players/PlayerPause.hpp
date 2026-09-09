#pragma once

#include <cstdint>

namespace fruityprime::players {

enum class PauseState : std::uint8_t {
    Closed,
    Opening,
    Open,
    Closing
};

// Native counterpart of PlayerPause.cs.  The map renderer can consume the
// state and selected item without making the simulation depend on a windowing
// toolkit.
class PlayerPause final {
public:
    void open(bool room_loading = false) noexcept;
    void close() noexcept;
    void toggle(bool room_loading = false) noexcept;
    void move(int direction) noexcept;
    void update(float frame_seconds) noexcept;

    [[nodiscard]] PauseState state() const noexcept { return state_; }
    [[nodiscard]] bool open_or_transitioning() const noexcept {
        return state_ != PauseState::Closed;
    }
    [[nodiscard]] bool room_loading() const noexcept { return room_loading_; }
    [[nodiscard]] int selected_item() const noexcept { return selected_item_; }

private:
    PauseState state_ = PauseState::Closed;
    float transition_seconds_ = 0.0F;
    bool room_loading_ = false;
    int selected_item_ = 0;
};

} // namespace fruityprime::players
