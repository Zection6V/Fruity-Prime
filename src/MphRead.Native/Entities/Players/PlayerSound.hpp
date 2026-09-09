#pragma once

#include "Entities/gameplay.hpp"
#include "Formats/enum_tables.hpp"
#include "Metadata/sound_metadata.hpp"

#include <array>
#include <cstdint>

namespace fruityprime::players {

using HunterSfx = sound::HunterSfx;

// Native counterpart of PlayerSound.cs.  The managed sound source resolves
// these events to cartridge SFX IDs; the native simulation retains the event
// and lets the active audio backend choose the actual sample.
class PlayerSound final {
public:
    [[nodiscard]] static gameplay::SoundEvent spawn(
        std::uint8_t slot, net::Vec3 position) noexcept;
    [[nodiscard]] static gameplay::SoundEvent damage(
        std::uint8_t slot, net::Vec3 position) noexcept;
    [[nodiscard]] static gameplay::SoundEvent death(
        std::uint8_t slot, net::Vec3 position) noexcept;
    [[nodiscard]] static gameplay::SoundEvent beam_shot(
        std::uint8_t slot, std::uint8_t weapon,
        net::Vec3 position) noexcept;

    // PlayerSound.PlayTimedSfx: a sound played on the timed source,
    // which is the one a pause mutes.  Recency is zero and the sound is
    // source-only, so it always retriggers and never leaks to the free
    // channel.
    [[nodiscard]] static gameplay::SoundEvent timed_sfx(
        std::uint8_t slot, std::int32_t sfx_id,
        net::Vec3 position) noexcept;

    // PlayerSound.StopContinuousBeamSfx: the two looping shot sounds a
    // beam can be holding.  Both are stopped, because a weapon switched
    // away from mid-charge is playing the affinity one rather than the
    // plain one, and which of the two it is is not tracked.
    [[nodiscard]] static std::array<std::int32_t, 2>
        continuous_beam_sfx(
            const sound::SoundMetadata& sounds,
            std::uint8_t beam) noexcept;
};

// PlayerSound.cs's per-player continuous-sound state and its two nested mute
// stacks.
//
// The managed code does not stop the double damage, cloak and flag-carry
// loops when time stops; it mutes them, so they resume mid-loop rather than
// restarting.  StopLongSfx nests inside StopTimedSfx, and only the outermost
// restart clears the corresponding mute -- reproducing the counters is what
// keeps a pause inside a pause from unmuting early.
class PlayerSoundState final {
public:
    static constexpr std::array<formats::SfxId, 3> DoubleDamageIds{
        formats::SfxId::DBL_DAMAGE_A, formats::SfxId::DBL_DAMAGE_B,
        formats::SfxId::DBL_DAMAGE_C};
    static constexpr std::array<formats::SfxId, 3> CloakIds{
        formats::SfxId::CLOAK_A, formats::SfxId::CLOAK_B,
        formats::SfxId::CLOAK_C};
    static constexpr std::array<formats::SfxId, 3> ScanSfxIds{
        formats::SfxId::SCAN_VISOR_ON, formats::SfxId::SCAN_STATUS_BAR,
        formats::SfxId::SCAN_VISOR_LOOP};

    // Sfx.TimedSfxMute / Sfx.LongSfxMute / Sfx.SfxMute are process-wide in
    // the managed implementation; they are kept here so a native session owns
    // its own audio state rather than a global.
    struct MuteState {
        int timed_sfx_mute = 0;
        int long_sfx_mute = 0;
        bool sfx_mute = false;
    };

    // PlayerSound.StartFlagCarrySfx / StopFlagCarrySfx
    void start_flag_carry_sfx() noexcept { flag_carry_on_ = true; }
    void stop_flag_carry_sfx() noexcept {
        flag_carry_handle_ = -1;
        flag_carry_on_ = false;
    }

    void update_scan_sfx(int index, bool enable) noexcept;

    // PlayerSound.StopTimedSfx() -- the no-argument form, which mutes rather
    // than stops.
    void stop_timed_sfx(MuteState& mute) noexcept {
        if (mute.timed_sfx_mute == 0) {
            double_damage_muted_ = true;
            cloak_muted_ = true;
            if (flag_carry_on_) {
                flag_carry_on_ = false;
                flag_carry_muted_ = true;
            }
            update_scan_sfx(-1, true);
        }
        ++mute.timed_sfx_mute;
    }

    // PlayerSound.RestartTimedSfx
    void restart_timed_sfx(MuteState& mute, bool force = false) noexcept {
        if (force || --mute.timed_sfx_mute <= 0) {
            mute.timed_sfx_mute = 0;
            double_damage_muted_ = false;
            cloak_muted_ = false;
            if (flag_carry_muted_) {
                flag_carry_muted_ = false;
                flag_carry_on_ = true;
            }
        }
    }

    // PlayerSound.StopLongSfx
    void stop_long_sfx(MuteState& mute) noexcept {
        stop_timed_sfx(mute);
        if (mute.long_sfx_mute == 0) {
            mute.sfx_mute = true;
        }
        ++mute.long_sfx_mute;
    }

    // PlayerSound.RestartLongSfx
    void restart_long_sfx(MuteState& mute, bool force = false) noexcept {
        restart_timed_sfx(mute, force);
        if (force || --mute.long_sfx_mute <= 0) {
            mute.long_sfx_mute = 0;
            // The managed comment notes that this is cleared alongside the
            // timed SFX even though only the long suppression sets it.
            mute.sfx_mute = false;
        }
    }

    // PlayerSound.StopAllSfx: the parts that are this player's own state.
    // Stopping the environment and free scripts is the mixer's job.
    void stop_all_sfx() noexcept {
        stop_flag_carry_sfx();
        health_sfx_handle_ = -1;
        double_damage_handle_ = -1;
        double_damage_id_ = formats::SfxId::None;
        cloak_handle_ = -1;
        cloak_id_ = formats::SfxId::None;
        update_scan_sfx(-1, false);
        double_damage_muted_ = false;
        cloak_muted_ = false;
    }

    [[nodiscard]] bool flag_carry_on() const noexcept { return flag_carry_on_; }
    [[nodiscard]] bool flag_carry_muted() const noexcept {
        return flag_carry_muted_;
    }
    [[nodiscard]] bool double_damage_muted() const noexcept {
        return double_damage_muted_;
    }
    [[nodiscard]] bool cloak_muted() const noexcept { return cloak_muted_; }
    [[nodiscard]] const std::array<bool, 3>& scan_sfx_on() const noexcept {
        return scan_sfx_on_;
    }

    // PlayerSound.ForceFieldSfxTimer / DoorUnlockSfxTimer / DoorChimeSfxTimer
    float force_field_sfx_timer = 0.0F;
    float door_unlock_sfx_timer = 0.0F;
    float door_chime_sfx_timer = 0.0F;

    struct TimedSoundUpdate {
        bool play_door_unlock = false;
        bool play_door_chime = false;
        bool play_force_field = false;
        bool play_scroll = false;
        bool stop_scroll = false;
        std::array<bool, 3> play_scan{};
    };

    [[nodiscard]] TimedSoundUpdate update_timed_sounds(
        float seconds, MuteState& mute,
        bool camera_blocks_input = false, bool unlock_sfx_playing = false,
        bool door_sfx_playing = false, bool scroll_sfx_playing = false) noexcept;

    float sfx_stop_timer = 0.0F;
    float damage_sfx_timer = 0.0F;
    float scroll_sfx_timer = 0.0F;

private:
    int flag_carry_handle_ = -1;
    bool flag_carry_on_ = false;
    bool flag_carry_muted_ = false;
    bool double_damage_muted_ = false;
    bool cloak_muted_ = false;
    int health_sfx_handle_ = -1;
    int double_damage_handle_ = -1;
    int cloak_handle_ = -1;
    formats::SfxId double_damage_id_ = formats::SfxId::None;
    formats::SfxId cloak_id_ = formats::SfxId::None;
    std::array<bool, 3> scan_sfx_on_{};
    std::array<int, 3> scan_sfx_handles_{-1, -1, -1};
};

} // namespace fruityprime::players
