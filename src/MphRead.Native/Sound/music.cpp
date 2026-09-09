#include "Sound/music.hpp"

#include "GameState.hpp"
#include "Sound/music_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace fruityprime::sound {
namespace {

constexpr std::int32_t MusicNone = 0;
constexpr std::int32_t MusicGumbo = 3;
constexpr std::int32_t MusicGuardian = 18;
constexpr std::int32_t MusicEnergyTimer = 51;
constexpr std::int32_t MusicEscape = 55;
constexpr std::int32_t MusicEscapeAlarm = 56;
constexpr std::int32_t SequenceOregano = 52;

[[nodiscard]] float finite_clamp(float value, float fallback = 1.0F) noexcept {
    (void)fallback;
    return std::clamp(value, 0.0F, 1.0F);
}

[[nodiscard]] std::uint16_t bit_for(int bit) noexcept {
    return bit >= 0 && bit < 16
        ? static_cast<std::uint16_t>(1U << bit) : 0;
}

} // namespace

void MusicController::init() noexcept {
    commands_.clear();
    volume_start_ = music_volume_;
    volume_target_ = music_volume_;
    volume_elapsed_ = 0.0F;
    volume_duration_ = 0.0F;
    stop_after_fade_ = false;
    base_tempo_ = 256;
    tempo_ = 256;
    tempo_start_ = 256;
    tempo_target_ = 256;
    tempo_elapsed_ = 0.0F;
    tempo_duration_ = 0.0F;
    current_music_id_ = MusicNone;
    current_sequence_id_ = -1;
    next_sequence_id_ = -1;
    music_to_resume_ = MusicNone;
    encounter_suspension_ = 0;
    next_tracks_ = 0;
    pending_tracks_ = 0;
    active_tracks_ = 0;
    muted_tracks_ = 0;
    fading_tracks_ = 0;
    next_fade_in_frames_ = 0;
    next_track_not_ready_ = false;
    playing_ = false;
    playback_active_ = false;
    paused_ = false;
    queued_ = false;
    ready_ = true;
    for (auto& fader : track_faders_) {
        fader = {};
    }
}

void MusicController::set_user_volume(float volume) noexcept {
    user_volume_ = finite_clamp(volume);
    if (playing_) {
        emit(MusicCommand{MusicCommandType::SetVolume, -1, 0, 0,
                          this->volume()});
    }
}

void MusicController::set_music_volume(float volume) noexcept {
    music_volume_ = finite_clamp(volume);
    if (playing_) {
        emit(MusicCommand{MusicCommandType::SetVolume, -1, 0, 0,
                          this->volume()});
    }
}

void MusicController::emit(MusicCommand command) {
    commands_.push_back(std::move(command));
}

void MusicController::play_music(std::int32_t music_id,
                                  std::optional<std::uint16_t> tracks,
                                  bool toggle_on_tracks,
                                  bool toggle_off_tracks) {
    if (music_id < 0
        || static_cast<std::size_t>(music_id) >= catalog_.music_tracks.size()) {
        return;
    }
    const auto& info = catalog_.music_tracks[static_cast<std::size_t>(
        music_id)];
    if (!info.sequence_id.has_value()) {
        return;
    }
    if (!tracks.has_value()) {
        tracks = info.tracks;
    }
    if (toggle_on_tracks) {
        pending_tracks_ = static_cast<std::uint16_t>(
            pending_tracks_ | *tracks);
    } else if (toggle_off_tracks) {
        pending_tracks_ = static_cast<std::uint16_t>(
            pending_tracks_ & static_cast<std::uint16_t>(~*tracks));
    } else {
        pending_tracks_ = *tracks;
    }
    current_music_id_ = music_id;
    paused_ = false;
    play_sequence(*info.sequence_id, pending_tracks_, true, true,
                  info.fade_out_frames, info.fade_in_frames);
}

void MusicController::try_play_room_music(std::int32_t room_id, int track) {
    play_room_music(room_id, track);
}

void MusicController::play_room_music(std::int32_t room_id, int track) {
    track = std::clamp(track, 0, 2);
    for (const auto& room : catalog_.room_music) {
        if (room.room_id == room_id) {
            const auto music_id = room.track_ids[static_cast<std::size_t>(
                track)];
            play_music(music_id);
            return;
        }
    }
}

void MusicController::play_sequence(std::int32_t sequence_id,
                                    std::uint16_t tracks, bool queue,
                                    bool not_ready,
                                    std::uint16_t fade_out_frames,
                                    std::uint16_t fade_in_frames) {
    if (sequence_id < 0) {
        return;
    }
    if (!queue) {
        queued_ = false;
        playing_ = true;
        playback_active_ = true;
        ready_ = !not_ready;
        current_sequence_id_ = sequence_id;
        active_tracks_ = tracks;
        muted_tracks_ = 0;
        fading_tracks_ = static_cast<std::uint16_t>(~active_tracks_);
        music_volume_ = 1.0F;
        volume_target_ = 1.0F;
        volume_duration_ = 0.0F;
        volume_elapsed_ = 0.0F;
        stop_after_fade_ = false;
        for (std::size_t index = 0; index < track_faders_.size(); ++index) {
            track_faders_[index] = {};
            track_faders_[index].start = bit_for(static_cast<int>(index)
                                                       & active_tracks_)
                != 0 ? 127.0F : 0.0F;
            track_faders_[index].target = track_faders_[index].start;
        }
        emit(MusicCommand{MusicCommandType::StartSequence, sequence_id,
                          tracks, 0, volume()});
        if (ready_) {
            emit(MusicCommand{MusicCommandType::SetTrackVolume, sequence_id,
                              static_cast<std::uint16_t>(~tracks), 0, 0.0F});
            emit(MusicCommand{MusicCommandType::SetVolume, sequence_id,
                              tracks, 0, volume()});
        }
        tempo_ = 256;
        base_tempo_ = 256;
        tempo_target_ = 256;
        tempo_duration_ = 0.0F;
        emit(MusicCommand{MusicCommandType::SetTempo, sequence_id, 0,
                          tempo_, 0.0F});
        next_sequence_id_ = sequence_id;
        return;
    }

    if (!queued_) {
        if (next_sequence_id_ == sequence_id) {
            if (ready_) {
                set_track_faders(tracks, 127,
                                 static_cast<float>(fade_in_frames) / 30.0F);
                set_track_faders(static_cast<std::uint16_t>(
                                     tracks ^ 0xffffU), 0,
                                 static_cast<float>(fade_out_frames) / 30.0F);
            } else {
                active_tracks_ = tracks;
            }
            return;
        }
        stop(static_cast<float>(fade_out_frames) / 30.0F);
        queued_ = true;
    }
    next_track_not_ready_ = not_ready;
    next_sequence_id_ = sequence_id;
    next_tracks_ = tracks;
    next_fade_in_frames_ = fade_in_frames;
}

void MusicController::play_paused_music() {
    if (!paused_ || current_music_id_ < 0
        || static_cast<std::size_t>(current_music_id_)
            >= catalog_.music_tracks.size()) {
        return;
    }
    const auto& info = catalog_.music_tracks[static_cast<std::size_t>(
        current_music_id_)];
    if (!info.sequence_id.has_value()) {
        return;
    }
    paused_ = false;
    play_sequence(*info.sequence_id, pending_tracks_, true, true);
}

void MusicController::pause() {
    if (!paused_) {
        stop();
        paused_ = true;
    }
}

void MusicController::stop(float fade_seconds) {
    const bool had_playback = playback_active_;
    playing_ = false;
    queued_ = false;
    next_sequence_id_ = -1;
    ready_ = true;
    if (!(fade_seconds > 0.0F) || !std::isfinite(fade_seconds)) {
        playback_active_ = false;
        emit(MusicCommand{MusicCommandType::Stop, current_sequence_id_, 0,
                          0, 0.0F});
        return;
    }
    if (!had_playback) {
        stop_after_fade_ = false;
        volume_duration_ = 0.0F;
        return;
    }
    fade_volume(0.0F, fade_seconds, true);
}

void MusicController::fade_volume(float volume, float seconds,
                                   bool stop_after_fade) {
    volume_start_ = music_volume_;
    volume_target_ = volume;
    music_volume_ = volume;
    volume_elapsed_ = 0.0F;
    volume_duration_ = std::isfinite(seconds) && seconds > 0.0F
        ? seconds : std::numeric_limits<float>::epsilon();
    stop_after_fade_ = stop_after_fade;
}

void MusicController::update_tempo(std::uint16_t tempo, float seconds) {
    if (!(seconds > 0.0F) || !std::isfinite(seconds)) {
        tempo_ = tempo;
        base_tempo_ = tempo;
        tempo_start_ = tempo;
        tempo_target_ = tempo;
        tempo_elapsed_ = 0.0F;
        tempo_duration_ = 0.0F;
        emit(MusicCommand{MusicCommandType::SetTempo,
                          current_sequence_id_, 0, tempo_, 0.0F});
        return;
    }
    if (tempo_target_ == tempo) {
        return;
    }
    tempo_start_ = tempo_;
    tempo_target_ = tempo;
    tempo_elapsed_ = 0.0F;
    tempo_duration_ = seconds;
}

void MusicController::set_track_faders(std::uint16_t tracks,
                                        std::uint8_t target,
                                        float seconds) {
    if (target == 0) {
        active_tracks_ = static_cast<std::uint16_t>(
            active_tracks_ & static_cast<std::uint16_t>(~tracks));
    } else {
        active_tracks_ = static_cast<std::uint16_t>(active_tracks_ | tracks);
    }
    const float duration = std::isfinite(seconds) && seconds > 0.0F
        ? seconds : std::numeric_limits<float>::epsilon();
    for (std::size_t index = 0; index < track_faders_.size(); ++index) {
        const auto bit = bit_for(static_cast<int>(index));
        if ((tracks & bit) == 0) {
            continue;
        }
        auto& fader = track_faders_[index];
        fader.start = fader.target;
        fader.target = static_cast<float>(target);
        fader.elapsed = 0.0F;
        fader.duration = duration;
        fader.active = true;
    }
    fading_tracks_ = static_cast<std::uint16_t>(fading_tracks_ | tracks);
}

void MusicController::process_volume(float seconds) noexcept {
    if (!(volume_duration_ > 0.0F)) {
        return;
    }
    volume_elapsed_ += std::max(0.0F, seconds);
    const float pct = std::clamp(volume_elapsed_ / volume_duration_,
                                 0.0F, 1.0F);
    music_volume_ = volume_start_ + (volume_target_ - volume_start_) * pct;
    emit(MusicCommand{MusicCommandType::SetVolume, current_sequence_id_, 0,
                      0, volume()});
    if (pct >= 1.0F) {
        volume_duration_ = 0.0F;
        if (stop_after_fade_) {
            stop_after_fade_ = false;
            playback_active_ = false;
            emit(MusicCommand{MusicCommandType::Stop, current_sequence_id_,
                              0, 0, 0.0F});
        }
    }
}

void MusicController::process_tempo(float seconds) noexcept {
    if (!(tempo_duration_ > 0.0F)) {
        return;
    }
    tempo_elapsed_ += std::max(0.0F, seconds);
    const float pct = std::clamp(tempo_elapsed_ / tempo_duration_,
                                 0.0F, 1.0F);
    tempo_ = static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(
            tempo_start_ + (tempo_target_ - tempo_start_) * pct),
        0, 65535));
    emit(MusicCommand{MusicCommandType::SetTempo, current_sequence_id_, 0,
                      tempo_, 0.0F});
    if (pct >= 1.0F) {
        tempo_duration_ = 0.0F;
    }
}

void MusicController::process_track_faders(float seconds) {
    if (fading_tracks_ == 0) {
        return;
    }
    for (std::size_t index = 0; index < track_faders_.size(); ++index) {
        const auto bit = bit_for(static_cast<int>(index));
        if ((fading_tracks_ & bit) == 0) {
            continue;
        }
        auto& fader = track_faders_[index];
        if (!fader.active) {
            fading_tracks_ = static_cast<std::uint16_t>(
                fading_tracks_ & static_cast<std::uint16_t>(~bit));
            continue;
        }
        fader.elapsed += std::max(0.0F, seconds);
        const float pct = std::clamp(fader.elapsed / fader.duration,
                                     0.0F, 1.0F);
        const auto value = static_cast<std::uint16_t>(std::clamp(
            static_cast<int>(
                fader.start + (fader.target - fader.start) * pct),
            0, 127));
        emit(MusicCommand{MusicCommandType::SetTrackVolume,
                          current_sequence_id_, bit, value, 0.0F});
        if (pct >= 1.0F) {
            fader.active = false;
            fader.target = static_cast<float>(value);
            fading_tracks_ = static_cast<std::uint16_t>(
                fading_tracks_ & static_cast<std::uint16_t>(~bit));
            if (value == 0) {
                muted_tracks_ = static_cast<std::uint16_t>(muted_tracks_ | bit);
            } else {
                muted_tracks_ = static_cast<std::uint16_t>(
                    muted_tracks_ & static_cast<std::uint16_t>(~bit));
            }
        }
    }
}

void MusicController::update(float seconds) noexcept {
    seconds = std::isfinite(seconds) ? std::max(0.0F, seconds) : 0.0F;
    const bool waiting_for_fade_stop = stop_after_fade_;
    if (!ready_ && playing_) {
        ready_ = true;
        emit(MusicCommand{MusicCommandType::SetVolume, current_sequence_id_,
                          active_tracks_, 0, volume()});
        emit(MusicCommand{MusicCommandType::SetTempo, current_sequence_id_,
                          0, tempo_, 0.0F});
        if (fading_tracks_ != 0) {
            emit(MusicCommand{MusicCommandType::SetTrackVolume,
                              current_sequence_id_, fading_tracks_, 0,
                              0.0F});
        }
    }
    process_volume(seconds);
    process_tempo(seconds);
    process_track_faders(seconds);
    if (!waiting_for_fade_stop && ready_ && queued_ && !playing_
        && next_sequence_id_ >= 0) {
        play_sequence(next_sequence_id_, next_tracks_, false,
                      next_track_not_ready_, 0, next_fade_in_frames_);
    }
}

void MusicController::switch_escape_music_if_needed() {
    if (current_music_id_ != MusicEscapeAlarm) {
        play_music(MusicEscapeAlarm);
    }
}

void MusicController::update_escape_music(float escape_timer_frames) {
    if (current_sequence_id_ != SequenceOregano
        || !std::isfinite(escape_timer_frames)) {
        return;
    }
    const int frames = static_cast<int>(std::max(0.0F, escape_timer_frames));
    if (frames >= 5400) {
        update_tempo(245, 1.0F / 30.0F);
    } else if (frames >= 5100) {
        const int tempo = 266 - ((frames - 5100) << 11) / 30000;
        update_tempo(static_cast<std::uint16_t>(tempo), 1.0F / 30.0F);
    } else if (frames >= 3600) {
        update_tempo(266, 1.0F / 30.0F);
    } else if (frames >= 3400) {
        const int tempo = 281 - 1536 * (frames - 3400) / 20000;
        update_tempo(static_cast<std::uint16_t>(tempo), 1.0F / 30.0F);
    } else if (frames >= 1800) {
        if (frames == 1800) {
            switch_escape_music_if_needed();
        }
        update_tempo(281, 1.0F / 30.0F);
    } else if (frames >= 1700) {
        switch_escape_music_if_needed();
        const int tempo = 294 - 1280 * (frames - 1700) / 10000;
        update_tempo(static_cast<std::uint16_t>(tempo), 1.0F / 30.0F);
    } else {
        switch_escape_music_if_needed();
        update_tempo(294, 1.0F / 30.0F);
    }
}

void MusicController::update_event_music(float elapsed_seconds) {
    if (current_music_id_ != MusicEnergyTimer
        || !std::isfinite(elapsed_seconds)
        || elapsed_seconds < 600.0F / 30.0F
        || elapsed_seconds >= 1800.0F / 30.0F) {
        return;
    }
    const int frames = static_cast<int>(elapsed_seconds * 30.0F);
    const int tempo = 384 - 12800 * (frames - 600) / 120000;
    update_tempo(static_cast<std::uint16_t>(tempo), 1.0F / 30.0F);
}

void MusicController::update_music_id_if_paused(
    std::int32_t music_id) noexcept {
    if (paused_) {
        current_music_id_ = music_id;
    }
}

void MusicController::play_encounter(metadata::Hunter hunter) {
    if (encounter_suspension_ == 0) {
        music_to_resume_ = current_music_id_;
        if (hunter == metadata::Hunter::Guardian) {
            play_music(MusicGuardian);
        }
    }
    constexpr std::array<int, 8> hunter_tracks{16, 4, 1, 0, 2, 5, 3, 16};
    int bit_index = 0;
    if (hunter == metadata::Hunter::Guardian) {
        bit_index = 10;
        for (int index = 7; index <= 9; ++index) {
            if ((encounter_suspension_ & (1 << index)) == 0) {
                bit_index = index;
                break;
            }
        }
    } else {
        const int hunter_index = std::clamp(static_cast<int>(hunter), 0, 7);
        bit_index = hunter_index;
        if (current_music_id_ != MusicGumbo) {
            play_music(MusicGumbo);
            play_music(MusicGumbo, bit_for(4), false, true);
        }
        play_music(MusicGumbo,
                   bit_for(hunter_tracks[static_cast<std::size_t>(
                       hunter_index)]), true, false);
        if (hunter == metadata::Hunter::Spire) {
            play_music(MusicGumbo, bit_for(9), false, true);
        }
    }
    encounter_suspension_ |= 1 << bit_index;
}

void MusicController::update_encounter(int clear_id) {
    if (encounter_suspension_ == 0) {
        return;
    }
    if (clear_id < 0) {
        encounter_suspension_ = 0;
        if (clear_id != -2) {
            play_music(music_to_resume_);
        }
        return;
    }
    int bit_index = clear_id;
    const auto hunter = static_cast<metadata::Hunter>(clear_id);
    if (hunter == metadata::Hunter::Guardian) {
        bit_index = 7;
        for (int index = 9; index >= 7; --index) {
            if ((encounter_suspension_ & (1 << index)) != 0) {
                bit_index = index;
                break;
            }
        }
    }
    if ((encounter_suspension_ & (1 << bit_index)) == 0) {
        return;
    }
    encounter_suspension_ &= ~(1 << bit_index);
    if (encounter_suspension_ == 0) {
        if (music_to_resume_ == MusicNone) {
            stop(1.0F);
        } else {
            play_music(music_to_resume_);
        }
    } else if (hunter != metadata::Hunter::Guardian) {
        constexpr std::array<int, 8> hunter_tracks{16, 4, 1, 0, 2, 5, 3, 16};
        if ((encounter_suspension_ & 0x7f) != 0) {
            const int index = std::clamp(bit_index, 0, 7);
            play_music(MusicGumbo, bit_for(hunter_tracks[
                       static_cast<std::size_t>(index)]), false, true);
            if (hunter == metadata::Hunter::Spire) {
                play_music(MusicGumbo, bit_for(9), true, false);
            }
        } else {
            play_music(MusicGuardian);
        }
    }
}

MusicSnapshot MusicController::snapshot() const noexcept {
    return MusicSnapshot{
        current_music_id_, current_sequence_id_, music_to_resume_,
        pending_tracks_, active_tracks_, muted_tracks_, fading_tracks_, tempo_,
        user_volume_, music_volume_, encounter_suspension_, playing_, paused_,
        queued_
    };
}

std::vector<MusicCommand> MusicController::take_commands() {
    std::vector<MusicCommand> result;
    result.swap(commands_);
    return result;
}

namespace {

MusicController* bound_music = nullptr;
const game::State* bound_game_state = nullptr;
float static_user_volume = 1.0F;
float static_music_volume = 1.0F;
std::int32_t static_music_to_resume = MusicNone;

[[nodiscard]] std::int32_t enum_value(formats::MusicId value) noexcept {
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] std::int32_t enum_value(formats::SeqId value) noexcept {
    return static_cast<std::int32_t>(value);
}

} // namespace

void Music::BindRuntime(MusicController* controller,
                        const game::State* state) noexcept {
    bound_music = controller;
    bound_game_state = state;
    if (bound_music != nullptr) {
        bound_music->assign_user_volume(static_user_volume);
        bound_music->assign_music_volume(static_music_volume);
        bound_music->set_music_to_resume(static_music_to_resume);
    }
}

float Music::UserVolume() noexcept {
    return static_user_volume;
}

void Music::UserVolume(float volume) noexcept {
    // Assignment to the C# auto-property does not update a playing stream.
    static_user_volume = volume;
    if (bound_music != nullptr) {
        bound_music->assign_user_volume(volume);
    }
}

void Music::SetUserVolume(float volume) noexcept {
    static_user_volume = std::clamp(volume, 0.0F, 1.0F);
    if (bound_music != nullptr) {
        bound_music->set_user_volume(static_user_volume);
    }
}

float Music::MusicVolume() noexcept {
    if (bound_music != nullptr) {
        static_music_volume = bound_music->snapshot().music_volume;
    }
    return static_music_volume;
}

void Music::MusicVolume(float volume) noexcept {
    static_music_volume = volume;
    if (bound_music != nullptr) {
        bound_music->assign_music_volume(volume);
    }
}

float Music::Volume() noexcept {
    return UserVolume() * MusicVolume();
}

bool Music::IsPaused() noexcept {
    return bound_music != nullptr && bound_music->snapshot().paused;
}

int Music::MusicEncounterSuspension() noexcept {
    return bound_music != nullptr
        ? bound_music->snapshot().encounter_suspension : 0;
}

formats::MusicId Music::MusicToResume() noexcept {
    if (bound_music != nullptr) {
        static_music_to_resume = bound_music->snapshot().music_to_resume;
    }
    return static_cast<formats::MusicId>(static_music_to_resume);
}

void Music::MusicToResume(formats::MusicId music_id) noexcept {
    static_music_to_resume = enum_value(music_id);
    if (bound_music != nullptr) {
        bound_music->set_music_to_resume(static_music_to_resume);
    }
}

void Music::Init() noexcept {
    static_music_to_resume = MusicNone;
    if (bound_music != nullptr) {
        bound_music->init();
        bound_music->assign_user_volume(static_user_volume);
        bound_music->assign_music_volume(static_music_volume);
        bound_music->set_music_to_resume(MusicNone);
    }
}

void Music::PlayMusic(formats::MusicId music_id,
                      std::optional<std::uint16_t> tracks,
                      bool toggle_on_tracks, bool toggle_off_tracks) {
    if (bound_music != nullptr) {
        bound_music->play_music(enum_value(music_id), tracks,
                                toggle_on_tracks, toggle_off_tracks);
    }
}

void Music::TryPlayRoomMusic(int room_id, int track) {
    if (bound_music == nullptr) {
        return;
    }
    if (bound_game_state != nullptr
        && !((bound_game_state->escape_timer == -1.0F
              || bound_game_state->escape_state
                    != game::EscapeState::Escape)
             && bound_music->snapshot().encounter_suspension == 0)) {
        return;
    }
    bound_music->play_room_music(room_id, track);
}

void Music::PlayRoomMusic(int room_id, int track) {
    if (bound_music != nullptr) {
        bound_music->play_room_music(room_id, track);
    }
}

void Music::PlaySeq(formats::SeqId sequence_id, bool not_ready) {
    PlaySeq(sequence_id, 0xffffU, false, not_ready);
}

void Music::PlaySeq(formats::SeqId sequence_id, std::uint16_t tracks,
                    bool queue, bool not_ready,
                    std::uint16_t fade_out_frames,
                    std::uint16_t fade_in_frames) {
    if (bound_music != nullptr) {
        bound_music->play_sequence(enum_value(sequence_id), tracks, queue,
                                   not_ready, fade_out_frames,
                                   fade_in_frames);
    }
}

void Music::UpdateMusic(float seconds) noexcept {
    if (bound_music != nullptr) {
        bound_music->update(seconds);
        static_music_volume = bound_music->snapshot().music_volume;
    }
}

void Music::UpdateMusicIdIfPaused(formats::MusicId music_id) noexcept {
    if (bound_music != nullptr) {
        bound_music->update_music_id_if_paused(enum_value(music_id));
    }
}

void Music::UpdateEscapeMusic() noexcept {
    if (bound_music != nullptr && bound_game_state != nullptr) {
        bound_music->update_escape_music(
            bound_game_state->escape_timer * 30.0F);
    }
}

void Music::UpdateEventMusic(float time) {
    if (bound_music != nullptr) {
        bound_music->update_event_music(time);
    }
}

void Music::PlayEncounterMusic(metadata::Hunter hunter) {
    if (bound_music != nullptr) {
        bound_music->play_encounter(hunter);
    }
}

void Music::UpdateEncounterMusic(int clear_id) {
    if (bound_music != nullptr) {
        bound_music->update_encounter(clear_id);
    }
}

void Music::PlayPausedMusic() {
    if (bound_music != nullptr) {
        bound_music->play_paused_music();
    }
}

void Music::Pause() {
    if (bound_music != nullptr) {
        bound_music->pause();
    }
}

void Music::Stop(float fade_time) {
    if (bound_music != nullptr) {
        bound_music->stop(fade_time);
    }
}

void Music::FadeVolume(float volume, float time, bool stop_after_fade) {
    static_music_volume = volume;
    if (bound_music != nullptr) {
        bound_music->fade_volume(volume, time, stop_after_fade);
    }
}

void Music::UpdateTempo(std::uint16_t tempo, float time) {
    if (bound_music != nullptr) {
        bound_music->update_tempo(tempo, time);
    }
}

MusicSnapshot Music::Snapshot() noexcept {
    return bound_music != nullptr ? bound_music->snapshot() : MusicSnapshot{
        MusicNone, -1, static_music_to_resume, 0, 0, 0, 0, 256,
        static_user_volume, static_music_volume, 0, false, false, false};
}

namespace {

MusicRuntime* bound_music_player = nullptr;
bool stop_loading = false;

} // namespace

void MusicPlayer::BindRuntime(MusicRuntime* runtime) noexcept {
    bound_music_player = runtime;
    stop_loading = false;
}

bool MusicPlayer::Available() noexcept {
    return bound_music_player != nullptr;
}

void* MusicPlayer::Engine() noexcept {
    return bound_music_player;
}

void* MusicPlayer::PlaybackDevice() noexcept {
    return bound_music_player;
}

MusicAudioFormat MusicPlayer::Format() noexcept {
    return {};
}

bool MusicPlayer::Loading() noexcept {
    return false;
}

bool MusicPlayer::StopLoading() noexcept {
    return stop_loading;
}

void MusicPlayer::StopLoading(bool value) noexcept {
    stop_loading = value;
}

void MusicPlayer::Load(formats::SeqId sequence_id, std::uint16_t tracks,
                       float volume) noexcept {
    if (bound_music_player == nullptr) {
        return;
    }
    stop_loading = false;
    bound_music_player->stop();
    if (sequence_id == formats::SeqId::None) {
        return;
    }
    if (stop_loading) {
        return;
    }
    static_cast<void>(bound_music_player->load_sequence(
        enum_value(sequence_id), tracks, volume));
}

void MusicPlayer::WaitForLoad(int sleep_ms) noexcept {
    (void)sleep_ms;
}

void MusicPlayer::Play(float volume) noexcept {
    if (bound_music_player != nullptr) {
        bound_music_player->play(volume);
    }
}

void MusicPlayer::Pause() noexcept {
    if (bound_music_player != nullptr) {
        bound_music_player->pause();
    }
}

PlaybackState MusicPlayer::State() noexcept {
    if (bound_music_player == nullptr) {
        return PlaybackState::Stopped;
    }
    switch (bound_music_player->state()) {
    case AudioSourceState::Playing:
        return PlaybackState::Playing;
    case AudioSourceState::Paused:
        return PlaybackState::Paused;
    case AudioSourceState::Initial:
    case AudioSourceState::Stopped:
        return PlaybackState::Stopped;
    }
    return PlaybackState::Stopped;
}

float MusicPlayer::Volume() noexcept {
    return bound_music_player != nullptr ? bound_music_player->volume() : 0.0F;
}

void MusicPlayer::Volume(float value) noexcept {
    if (bound_music_player != nullptr) {
        bound_music_player->set_volume(value);
    }
}

std::uint16_t MusicPlayer::Tempo() noexcept {
    return bound_music_player != nullptr ? bound_music_player->tempo() : 0;
}

void MusicPlayer::Tempo(std::uint16_t value) noexcept {
    if (bound_music_player != nullptr) {
        bound_music_player->set_tempo(value);
    }
}

std::nullptr_t MusicPlayer::GetTrack(int index) noexcept {
    (void)index;
    // Native SSEQ rendering applies the 16-bit track mask when building the
    // PCM buffer; it has no mutable NCSF Track object to expose.
    return nullptr;
}

void MusicPlayer::Stop() noexcept {
    if (bound_music_player != nullptr) {
        bound_music_player->stop();
    }
}

void MusicPlayer::Remove(bool shutdown) noexcept {
    (void)shutdown;
    if (bound_music_player != nullptr) {
        bound_music_player->reset();
    }
}

} // namespace fruityprime::sound
