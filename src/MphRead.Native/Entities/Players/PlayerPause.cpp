// Native counterpart of src/MphRead/Entities/Players/PlayerPause.cs.
#include "PlayerPause.hpp"

#include <algorithm>

namespace fruityprime::players {

void PlayerPause::open(bool room_loading) noexcept {
    if (state_ == PauseState::Open || state_ == PauseState::Opening) {
        return;
    }
    room_loading_ = room_loading;
    state_ = PauseState::Opening;
    transition_seconds_ = 0.15F;
}

void PlayerPause::close() noexcept {
    if (state_ == PauseState::Closed || state_ == PauseState::Closing) {
        return;
    }
    state_ = PauseState::Closing;
    transition_seconds_ = 0.15F;
}

void PlayerPause::toggle(bool room_loading) noexcept {
    if (state_ == PauseState::Closed || state_ == PauseState::Closing) {
        open(room_loading);
    } else {
        close();
    }
}

void PlayerPause::move(int direction) noexcept {
    if (state_ != PauseState::Open) {
        return;
    }
    selected_item_ = std::clamp(selected_item_ + direction, 0, 3);
}

void PlayerPause::update(float frame_seconds) noexcept {
    const float seconds = std::max(0.0F, frame_seconds);
    if (transition_seconds_ <= 0.0F) {
        return;
    }
    transition_seconds_ = std::max(0.0F, transition_seconds_ - seconds);
    if (transition_seconds_ != 0.0F) {
        return;
    }
    if (state_ == PauseState::Opening) {
        state_ = PauseState::Open;
    } else if (state_ == PauseState::Closing) {
        state_ = PauseState::Closed;
        room_loading_ = false;
    }
}

} // namespace fruityprime::players
