#pragma once

#include "Metadata/metadata.hpp"
#include "Formats/sound_catalog.hpp"
#include "Formats/enum_tables.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace fruityprime::game {
struct State;
}

namespace fruityprime::sound {

class MusicRuntime;

enum class PlaybackState : std::uint8_t {
    Stopped,
    Playing,
    Paused,
};

struct MusicAudioFormat {
    std::uint32_t sample_rate = 32728;
    std::uint8_t channels = 2;
    bool floating_point = true;
};

// Backend-independent part of Sound/Music.cs.  A command is deliberately a
// value object: the sequencer or a platform audio service can consume it
// without making gameplay depend on OpenAL, WinMM, or Android types.
enum class MusicCommandType : std::uint8_t {
    StartSequence,
    Stop,
    SetVolume,
    SetTempo,
    SetTrackVolume,
};

struct MusicCommand {
    MusicCommandType type = MusicCommandType::Stop;
    std::int32_t sequence_id = -1;
    std::uint16_t tracks = 0;
    std::uint16_t value = 0;
    float volume = 0.0F;
};

struct MusicSnapshot {
    std::int32_t current_music_id = 0;
    std::int32_t current_sequence_id = -1;
    std::int32_t music_to_resume = 0;
    std::uint16_t pending_tracks = 0;
    std::uint16_t active_tracks = 0;
    std::uint16_t muted_tracks = 0;
    std::uint16_t fading_tracks = 0;
    std::uint16_t tempo = 256;
    float user_volume = 1.0F;
    float music_volume = 1.0F;
    int encounter_suspension = 0;
    bool playing = false;
    bool paused = false;
    bool queued = false;
};

class MusicController final {
public:
    explicit MusicController(const Catalog& catalog) noexcept
        : catalog_(catalog) {}

    void init() noexcept;

    void set_user_volume(float volume) noexcept;
    void set_music_volume(float volume) noexcept;
    void assign_user_volume(float volume) noexcept { user_volume_ = volume; }
    void assign_music_volume(float volume) noexcept { music_volume_ = volume; }
    [[nodiscard]] float volume() const noexcept {
        return user_volume_ * music_volume_;
    }

    void play_music(std::int32_t music_id,
                    std::optional<std::uint16_t> tracks = std::nullopt,
                    bool toggle_on_tracks = false,
                    bool toggle_off_tracks = false);
    void try_play_room_music(std::int32_t room_id, int track);
    void play_room_music(std::int32_t room_id, int track);

    void play_sequence(std::int32_t sequence_id,
                       std::uint16_t tracks = 0xffff,
                       bool queue = false,
                       bool not_ready = false,
                       std::uint16_t fade_out_frames = 0,
                       std::uint16_t fade_in_frames = 0);
    void play_paused_music();
    void pause();
    void stop(float fade_seconds = 0.0F);
    void fade_volume(float volume, float seconds,
                     bool stop_after_fade = false);
    void update_tempo(std::uint16_t tempo, float seconds);

    void play_encounter(metadata::Hunter hunter);
    void update_encounter(int clear_id);

    // Escape/event tempo rules are kept here rather than in the renderer so
    // story, replay, and a future Android host share the same state changes.
    void update_escape_music(float escape_timer_frames);
    void update_event_music(float elapsed_frames);
    void update_music_id_if_paused(std::int32_t music_id) noexcept;
    void set_music_to_resume(std::int32_t music_id) noexcept {
        music_to_resume_ = music_id;
    }

    void update(float seconds) noexcept;
    [[nodiscard]] MusicSnapshot snapshot() const noexcept;
    [[nodiscard]] std::vector<MusicCommand> take_commands();

private:
    struct Fader {
        float start = 127.0F;
        float target = 127.0F;
        float elapsed = 0.0F;
        float duration = 0.0F;
        bool active = false;
    };

    void emit(MusicCommand command);
    void set_track_faders(std::uint16_t tracks, std::uint8_t target,
                          float seconds);
    void process_volume(float seconds) noexcept;
    void process_tempo(float seconds) noexcept;
    void process_track_faders(float seconds);
    void switch_escape_music_if_needed();

    const Catalog& catalog_;
    std::vector<MusicCommand> commands_;
    std::array<Fader, 16> track_faders_{};

    float user_volume_ = 1.0F;
    float music_volume_ = 1.0F;
    float volume_start_ = 1.0F;
    float volume_target_ = 1.0F;
    float volume_elapsed_ = 0.0F;
    float volume_duration_ = 0.0F;
    bool stop_after_fade_ = false;

    std::uint16_t base_tempo_ = 256;
    std::uint16_t tempo_ = 256;
    std::uint16_t tempo_start_ = 256;
    std::uint16_t tempo_target_ = 256;
    float tempo_elapsed_ = 0.0F;
    float tempo_duration_ = 0.0F;

    std::int32_t current_music_id_ = 0;
    std::int32_t current_sequence_id_ = -1;
    std::int32_t next_sequence_id_ = -1;
    std::int32_t music_to_resume_ = 0;
    std::int32_t encounter_suspension_ = 0;
    std::uint16_t next_tracks_ = 0;
    std::uint16_t pending_tracks_ = 0;
    std::uint16_t active_tracks_ = 0;
    std::uint16_t muted_tracks_ = 0;
    std::uint16_t fading_tracks_ = 0;
    std::uint16_t next_fade_in_frames_ = 0;
    bool next_track_not_ready_ = false;
    bool playing_ = false;
    bool playback_active_ = false;
    bool paused_ = false;
    bool queued_ = false;
    bool ready_ = true;
};

// Sound/Music.cs is process-wide. MusicController remains the portable
// implementation object used by the audio backend, while this static boundary
// preserves the managed ownership and public call shape.
class Music final {
public:
    static void BindRuntime(MusicController* controller,
                            const game::State* state = nullptr) noexcept;

    [[nodiscard]] static float UserVolume() noexcept;
    static void UserVolume(float volume) noexcept;
    static void SetUserVolume(float volume) noexcept;
    [[nodiscard]] static float MusicVolume() noexcept;
    static void MusicVolume(float volume) noexcept;
    [[nodiscard]] static float Volume() noexcept;
    [[nodiscard]] static bool IsPaused() noexcept;
    [[nodiscard]] static int MusicEncounterSuspension() noexcept;
    [[nodiscard]] static formats::MusicId MusicToResume() noexcept;
    static void MusicToResume(formats::MusicId music_id) noexcept;

    static void Init() noexcept;
    static void PlayMusic(formats::MusicId music_id,
                          std::optional<std::uint16_t> tracks = std::nullopt,
                          bool toggle_on_tracks = false,
                          bool toggle_off_tracks = false);
    static void TryPlayRoomMusic(int room_id, int track);
    static void PlayRoomMusic(int room_id, int track);
    static void PlaySeq(formats::SeqId sequence_id, bool not_ready = true);
    static void PlaySeq(formats::SeqId sequence_id, std::uint16_t tracks,
                        bool queue = false, bool not_ready = false,
                        std::uint16_t fade_out_frames = 0,
                        std::uint16_t fade_in_frames = 0);
    static void UpdateMusic(float seconds = 0.0F) noexcept;
    static void UpdateMusicIdIfPaused(formats::MusicId music_id) noexcept;
    static void UpdateEscapeMusic() noexcept;
    static void UpdateEventMusic(float time);
    static void PlayEncounterMusic(metadata::Hunter hunter);
    static void UpdateEncounterMusic(int clear_id);
    static void PlayPausedMusic();
    static void Pause();
    static void Stop(float fade_time = 0.0F);
    static void FadeVolume(float volume, float time,
                           bool stop_after_fade = false);
    static void UpdateTempo(std::uint16_t tempo, float time);
    [[nodiscard]] static MusicSnapshot Snapshot() noexcept;

private:
    Music() = delete;
};

// Public MusicPlayer class from Music.cs. The managed implementation loads
// NCSF on a task; native SSEQ rendering is synchronous, so Loading is false as
// soon as Load returns and WaitForLoad has no work to perform.
class MusicPlayer final {
public:
    static void BindRuntime(MusicRuntime* runtime) noexcept;
    [[nodiscard]] static bool Available() noexcept;
    [[nodiscard]] static void* Engine() noexcept;
    [[nodiscard]] static void* PlaybackDevice() noexcept;
    [[nodiscard]] static MusicAudioFormat Format() noexcept;
    [[nodiscard]] static bool Loading() noexcept;
    [[nodiscard]] static bool StopLoading() noexcept;
    static void StopLoading(bool value) noexcept;
    static void Load(formats::SeqId sequence_id,
                     std::uint16_t tracks = 0xffff,
                     float volume = 1.0F) noexcept;
    static void WaitForLoad(int sleep_ms = 100) noexcept;
    static void Play(float volume) noexcept;
    static void Pause() noexcept;
    [[nodiscard]] static PlaybackState State() noexcept;
    [[nodiscard]] static float Volume() noexcept;
    static void Volume(float value) noexcept;
    [[nodiscard]] static std::uint16_t Tempo() noexcept;
    static void Tempo(std::uint16_t value) noexcept;
    [[nodiscard]] static std::nullptr_t GetTrack(int index) noexcept;
    static void Stop() noexcept;
    static void Remove(bool shutdown = false) noexcept;

private:
    MusicPlayer() = delete;
};

} // namespace fruityprime::sound

namespace MphReadNative::Sound {
using Music = ::fruityprime::sound::Music;
using MusicAudioFormat = ::fruityprime::sound::MusicAudioFormat;
using MusicCommand = ::fruityprime::sound::MusicCommand;
using MusicCommandType = ::fruityprime::sound::MusicCommandType;
using MusicController = ::fruityprime::sound::MusicController;
using MusicSnapshot = ::fruityprime::sound::MusicSnapshot;
using MusicPlayer = ::fruityprime::sound::MusicPlayer;
using PlaybackState = ::fruityprime::sound::PlaybackState;
}
