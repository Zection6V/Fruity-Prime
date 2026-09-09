#pragma once

namespace fruityprime::mods::thumbnail {

// Process-wide state used while a room preview is rendered.  The renderer
// reads this state to suppress the normal HUD; the host supplies the current
// audio volumes so this small module does not depend on a platform audio API.
class Controller final {
public:
    static constexpr float DefaultSfxVolume = 0.35F;
    static constexpr float DefaultMusicVolume = 1.0F;

    // Matches ThumbnailMode.Enter: the first call saves and mutes, while a
    // repeated call is a no-op and cannot overwrite the saved volumes.
    [[nodiscard]] bool enter(float& sfx_volume,
                              float& music_volume) noexcept;

    // Matches ThumbnailMode.Exit: restore the values captured by enter.
    // Calling exit while inactive is a no-op.
    [[nodiscard]] bool exit(float& sfx_volume,
                             float& music_volume) noexcept;

    [[nodiscard]] bool active() const noexcept { return active_; }

private:
    bool active_ = false;
    float saved_sfx_volume_ = DefaultSfxVolume;
    float saved_music_volume_ = DefaultMusicVolume;
};

// The managed implementation is static and process-wide.  Native hosts that
// render through more than one frontend can share this instance as well.
[[nodiscard]] Controller& instance() noexcept;

} // namespace fruityprime::mods::thumbnail
