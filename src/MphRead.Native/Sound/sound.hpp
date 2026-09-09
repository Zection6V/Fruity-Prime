#pragma once

#include "Sound/sdat.hpp"
#include "Sound/music.hpp"
#include "Sound/music_runtime.hpp"
#include "Formats/sound_catalog.hpp"
#include "Sound/sound_resources.hpp"
#include "Sound/sseq_player.hpp"
#include "Sound/software_mixer.hpp"
#include "Mods/Sound/sfx_mixer.hpp"
#include "Sound/win32_audio_output.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace MphReadNative::Sound {

using Sdat = ::fruityprime::sound::Sdat;
using Sbnk = ::fruityprime::sound::Sbnk;
using Sseq = ::fruityprime::sound::Sseq;
using Swar = ::fruityprime::sound::Swar;
using Swav = ::fruityprime::sound::Swav;
using Sample = ::fruityprime::sound::Sample;
using FhSample = ::fruityprime::sound::FhSample;
using Stream = ::fruityprime::sound::Stream;
using Catalog = ::fruityprime::sound::Catalog;
using MusicCommand = ::fruityprime::sound::MusicCommand;
using MusicCommandType = ::fruityprime::sound::MusicCommandType;
using MusicController = ::fruityprime::sound::MusicController;
using MusicSnapshot = ::fruityprime::sound::MusicSnapshot;
using MusicRuntime = ::fruityprime::sound::MusicRuntime;
using SseqNoteEvent = ::fruityprime::sound::SseqNoteEvent;
using SseqTimelineStats = ::fruityprime::sound::SseqTimelineStats;
using SseqTimeline = ::fruityprime::sound::SseqTimeline;
using SseqRenderOptions = ::fruityprime::sound::SseqRenderOptions;
using SseqPlayer = ::fruityprime::sound::SseqPlayer;
using StreamInfo = ::fruityprime::sound::StreamInfo;
using SoftwareMixer = ::fruityprime::sound::SoftwareMixer;
using SfxMixer = ::fruityprime::mods::sound::SfxMixer;
using Win32AudioOutput = ::fruityprime::sound::Win32AudioOutput;
using AudioBufferFormat = ::fruityprime::sound::AudioBufferFormat;
using AudioSourceState = ::fruityprime::sound::AudioSourceState;
using AudioVector3 = ::fruityprime::sound::AudioVector3;
using SelectEntry = ::fruityprime::sound::SelectEntry;
using Sound3dEntry = ::fruityprime::sound::Sound3dEntry;
using SoundTable = ::fruityprime::sound::SoundTable;
using RoomMusic = ::fruityprime::sound::RoomMusic;
using MusicTrack = ::fruityprime::sound::MusicTrack;
using SfxScriptFile = ::fruityprime::sound::SfxScriptFile;
using DgnFile = ::fruityprime::sound::DgnFile;

// Portable audio boundary.  The null backend is the current native default;
// platform mixers can implement this contract without leaking backend types
// into gameplay or the launcher.
enum class Backend : std::uint8_t {
    Null,
    WinMm,
    OpenAl,
    Android,
};

struct Configuration {
    Backend backend = Backend::Null;
    float master_gain = 1.0F;
    bool enabled = true;
};

class Mixer {
public:
    explicit Mixer(Configuration configuration = {}) noexcept
        : configuration_(configuration) {}

    [[nodiscard]] bool start() noexcept;
    void stop() noexcept;
    [[nodiscard]] bool submit(std::span<const std::int16_t> samples) noexcept;

    [[nodiscard]] const Configuration& configuration() const noexcept {
        return configuration_;
    }
    [[nodiscard]] bool running() const noexcept { return running_; }
    [[nodiscard]] std::size_t submitted_samples() const noexcept {
        return submitted_samples_;
    }

private:
    Configuration configuration_;
    std::unique_ptr<Win32AudioOutput> output_;
    bool running_ = false;
    std::size_t submitted_samples_ = 0;
};

} // namespace MphReadNative::Sound
