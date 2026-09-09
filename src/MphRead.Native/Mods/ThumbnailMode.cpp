#include "Mods/thumbnail_mode.hpp"

namespace fruityprime::mods::thumbnail {

bool Controller::enter(float& sfx_volume, float& music_volume) noexcept {
    if (active_) {
        return false;
    }
    saved_sfx_volume_ = sfx_volume;
    saved_music_volume_ = music_volume;
    sfx_volume = 0.0F;
    music_volume = 0.0F;
    active_ = true;
    return true;
}

bool Controller::exit(float& sfx_volume, float& music_volume) noexcept {
    if (!active_) {
        return false;
    }
    active_ = false;
    sfx_volume = saved_sfx_volume_;
    music_volume = saved_music_volume_;
    return true;
}

Controller& instance() noexcept {
    static Controller controller;
    return controller;
}

} // namespace fruityprime::mods::thumbnail
