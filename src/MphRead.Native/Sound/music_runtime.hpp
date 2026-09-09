#pragma once

#include "Sound/music.hpp"
#include "Mods/Sound/sfx_mixer.hpp"

#include <cstddef>
#include <cstdint>

namespace fruityprime::sound {

// Connects MusicController's backend-independent commands to the native
// software mixer.  This is intentionally a bounded-buffer bridge: it gives
// the Win32 host real SSEQ music while keeping the controller independent of
// a platform audio API.  Full DS channel allocation and streaming remain a
// later fidelity layer.
class MusicRuntime final {
public:
    MusicRuntime(const Catalog& catalog, MusicController& controller,
                 mods::sound::SfxMixer& mixer) noexcept;
    ~MusicRuntime() noexcept;

    MusicRuntime(const MusicRuntime&) = delete;
    MusicRuntime& operator=(const MusicRuntime&) = delete;

    void update(float seconds) noexcept;
    void reset() noexcept;
    [[nodiscard]] bool load_sequence(std::int32_t sequence_id,
                                     std::uint16_t tracks = 0xffff,
                                     float volume = 1.0F) noexcept;
    void play(float volume) noexcept;
    void pause() noexcept;
    void stop() noexcept;
    void set_volume(float volume) noexcept;
    void set_tempo(std::uint16_t tempo) noexcept;
    [[nodiscard]] float volume() const noexcept { return volume_; }
    [[nodiscard]] std::uint16_t tempo() const noexcept;
    [[nodiscard]] AudioSourceState state() const noexcept;

    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] std::int32_t sequence_id() const noexcept {
        return sequence_id_;
    }
    [[nodiscard]] std::size_t rendered_frames() const noexcept {
        return rendered_frames_;
    }

private:
    [[nodiscard]] bool start_sequence(std::int32_t sequence_id,
                                      std::uint16_t tracks,
                                      float volume,
                                      bool play_now = true) noexcept;
    void apply_command(const MusicCommand& command) noexcept;

    const Catalog& catalog_;
    MusicController& controller_;
    mods::sound::SfxMixer& mixer_;
    mods::sound::SfxMixer::Handle buffer_ = 0;
    mods::sound::SfxMixer::Handle source_ = 0;
    std::int32_t sequence_id_ = -1;
    std::uint16_t track_mask_ = 0;
    float volume_ = 1.0F;
    float tempo_ = 1.0F;
    std::size_t rendered_frames_ = 0;
};

} // namespace fruityprime::sound

namespace MphReadNative::Sound {
using MusicRuntime = ::fruityprime::sound::MusicRuntime;
}
