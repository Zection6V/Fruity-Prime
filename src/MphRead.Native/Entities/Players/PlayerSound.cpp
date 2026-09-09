// Native counterpart of src/MphRead/Entities/Players/PlayerSound.cs.
#include "PlayerSound.hpp"

#include <algorithm>

namespace fruityprime::players {

gameplay::SoundEvent PlayerSound::spawn(std::uint8_t slot,
                                        net::Vec3 position) noexcept {
    gameplay::SoundEvent event;
    event.cue = gameplay::SoundCue::PlayerSpawn;
    event.slot = slot;
    event.position = position;
    return event;
}

gameplay::SoundEvent PlayerSound::damage(std::uint8_t slot,
                                         net::Vec3 position) noexcept {
    gameplay::SoundEvent event;
    event.cue = gameplay::SoundCue::PlayerDamage;
    event.slot = slot;
    event.position = position;
    return event;
}

gameplay::SoundEvent PlayerSound::death(std::uint8_t slot,
                                        net::Vec3 position) noexcept {
    gameplay::SoundEvent event;
    event.cue = gameplay::SoundCue::PlayerDeath;
    event.slot = slot;
    event.position = position;
    return event;
}

gameplay::SoundEvent PlayerSound::beam_shot(
    std::uint8_t slot, std::uint8_t weapon, net::Vec3 position) noexcept {
    gameplay::SoundEvent event;
    event.cue = gameplay::SoundCue::BeamShot;
    event.slot = slot;
    event.weapon = weapon;
    event.position = position;
    return event;
}

gameplay::SoundEvent PlayerSound::timed_sfx(
    std::uint8_t slot, std::int32_t sfx_id, net::Vec3 position) noexcept {
    gameplay::SoundEvent event;
    event.cue = gameplay::SoundCue::BeamShot;
    event.slot = slot;
    // The cartridge SFX id travels in entity_id: the mixer resolves it, and
    // the simulation never needs to know which sample it is.
    event.entity_id = static_cast<std::uint32_t>(sfx_id);
    event.position = position;
    return event;
}

std::array<std::int32_t, 2> PlayerSound::continuous_beam_sfx(
    const sound::SoundMetadata& sounds, std::uint8_t beam) noexcept {
    return {sounds.beam(beam, sound::BeamSfx::Shot),
            sounds.beam(beam, sound::BeamSfx::AffinityChargeShot)};
}

void PlayerSoundState::update_scan_sfx(int index, bool enable) noexcept {
    if (index == -1) {
        for (std::size_t i = 0; i < scan_sfx_on_.size(); ++i) {
            if (scan_sfx_on_[i]) {
                scan_sfx_handles_[i] = -1;
            }
        }
        if (!enable) {
            scan_sfx_on_.fill(false);
        }
        return;
    }
    if (index < 0 || index >= static_cast<int>(scan_sfx_on_.size())) {
        return;
    }
    const auto slot = static_cast<std::size_t>(index);
    if (enable) {
        scan_sfx_on_[slot] = true;
    } else {
        scan_sfx_handles_[slot] = -1;
        scan_sfx_on_[slot] = false;
    }
}

PlayerSoundState::TimedSoundUpdate PlayerSoundState::update_timed_sounds(
    float seconds, MuteState& mute, bool camera_blocks_input,
    bool unlock_sfx_playing, bool door_sfx_playing,
    bool scroll_sfx_playing) noexcept {
    TimedSoundUpdate result;
    const float elapsed = std::max(0.0F, seconds);

    if (sfx_stop_timer > 0.0F) {
        sfx_stop_timer -= elapsed;
        if (sfx_stop_timer <= 0.0F) {
            sfx_stop_timer = 0.0F;
            stop_long_sfx(mute);
        }
    }
    if (damage_sfx_timer > 0.0F) {
        damage_sfx_timer = std::max(0.0F, damage_sfx_timer - elapsed);
    }
    if (mute.timed_sfx_mute == 0) {
        for (std::size_t i = 0; i < scan_sfx_on_.size(); ++i) {
            if (scan_sfx_on_[i] && scan_sfx_handles_[i] == -1) {
                result.play_scan[i] = true;
                // The mixer replaces this sentinel with its real handle.  As
                // in C#, non--1 means the loop has already been started.
                scan_sfx_handles_[i] = 0;
            }
        }
    }
    if (mute.long_sfx_mute == 0 && door_unlock_sfx_timer > 0.0F) {
        door_unlock_sfx_timer -= elapsed;
        if (door_unlock_sfx_timer <= 1.0F / 30.0F) {
            door_unlock_sfx_timer = 0.0F;
            result.play_door_unlock = !unlock_sfx_playing;
        }
    }
    if (door_chime_sfx_timer > 0.0F) {
        door_chime_sfx_timer -= elapsed;
        if (door_chime_sfx_timer <= 1.0F / 30.0F) {
            door_chime_sfx_timer = 0.0F;
            result.play_door_chime = mute.timed_sfx_mute == 0
                && !camera_blocks_input && !door_sfx_playing;
        }
    }
    if (force_field_sfx_timer > 0.0F) {
        force_field_sfx_timer -= elapsed;
        if (force_field_sfx_timer <= 0.0F) {
            force_field_sfx_timer = 0.0F;
            result.play_force_field = mute.timed_sfx_mute == 0;
        }
    }
    if (scroll_sfx_timer > 0.0F) {
        if (scroll_sfx_timer < 2.0F / 30.0F) {
            result.stop_scroll = true;
        } else if (!scroll_sfx_playing) {
            result.play_scroll = true;
        }
        scroll_sfx_timer -= elapsed;
    }
    return result;
}

} // namespace fruityprime::players
