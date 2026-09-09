#pragma once

#include "Entities/gameplay.hpp"
#include "Mods/Input/input.hpp"

#include <cstdint>

namespace fruityprime::mods::spectator {

// The managed SpectatorMode is a stateful bridge between input, the current
// player camera, and the network session.  Keep that state in its own native
// object instead of making the Win32 game loop own another gameplay system.
class Controller final {
public:
    [[nodiscard]] bool is_spectating() const noexcept {
        return is_spectating_;
    }

    [[nodiscard]] bool free_camera() const noexcept {
        return free_camera_;
    }

    [[nodiscard]] std::uint8_t view_slot() const noexcept {
        return view_slot_;
    }

    [[nodiscard]] bool show_scoreboard() const noexcept {
        return show_scoreboard_;
    }

    // F6 has the native window's historical meaning: enter the spectator
    // overview, then move to the next live player on subsequent presses.
    // `multiplayer` is the native equivalent of GameState.Multiplayer.
    bool toggle(gameplay::Session& session, std::uint8_t local_slot,
                bool multiplayer, bool replay_active,
                input::State& input);

    // The managed pause-menu/input path uses these two operations separately.
    bool start(gameplay::Session& session, std::uint8_t local_slot,
               bool multiplayer, bool watch_someone,
               input::State& input);
    bool cycle_next(gameplay::Session& session,
                    std::uint8_t local_slot);
    bool toggle_view(gameplay::Session& session,
                     std::uint8_t local_slot);
    bool rejoin(gameplay::Session& session, std::uint8_t local_slot);

    // Scene/input reporting hooks matching SpectatorMode.NoteFreeCamera and
    // NoteScoreboard.  The current native renderer has no separate Scene
    // camera object, but keeping these hooks makes the ownership boundary
    // explicit and prevents the Win32 loop from reaching into the state.
    void note_free_camera(bool on) noexcept { free_camera_ = on; }
    void note_scoreboard(bool down) noexcept {
        show_scoreboard_ = down && is_spectating_;
    }

    void reset() noexcept {
        is_spectating_ = false;
        free_camera_ = false;
        show_scoreboard_ = false;
        view_slot_ = 0;
    }

private:
    [[nodiscard]] static bool can_watch_player(
        const net::PlayerState& player, std::uint8_t local_slot) noexcept;
    [[nodiscard]] static int find_next_active(
        const gameplay::Session& session, int from_slot,
        std::uint8_t local_slot) noexcept;
    void switch_view(gameplay::Session& session, std::uint8_t slot) noexcept;

    bool is_spectating_ = false;
    bool free_camera_ = false;
    bool show_scoreboard_ = false;
    std::uint8_t view_slot_ = 0;
};

} // namespace fruityprime::mods::spectator
