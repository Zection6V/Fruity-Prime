#include "Sound/music_runtime.hpp"

#include "Sound/sseq_player.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace fruityprime::sound {
namespace {

constexpr double RenderSeconds = 30.0;
constexpr std::size_t MaxRenderTicks = 131072;
constexpr std::size_t MaxRenderEvents = 100000;

[[nodiscard]] float clamp_volume(float value) noexcept {
    if (!std::isfinite(value)) {
        return 0.0F;
    }
    return std::clamp(value, 0.0F, 1.0F);
}

[[nodiscard]] float tempo_pitch(std::uint16_t tempo) noexcept {
    return std::clamp(static_cast<float>(tempo) / 256.0F, 0.25F, 4.0F);
}

void pcm16_bytes(std::vector<std::uint8_t>& bytes,
                 const std::vector<float>& audio) {
    if (audio.size() > std::numeric_limits<std::size_t>::max() / 2U) {
        throw std::length_error("SSEQ music PCM buffer is too large");
    }
    bytes.resize(audio.size() * 2U);
    for (std::size_t index = 0; index < audio.size(); ++index) {
        const float clipped = std::clamp(audio[index], -1.0F, 1.0F);
        const auto value = static_cast<std::int16_t>(std::lround(
            clipped * 32767.0F));
        const auto raw = static_cast<std::uint16_t>(value);
        bytes[index * 2U] = static_cast<std::uint8_t>(raw);
        bytes[index * 2U + 1U] = static_cast<std::uint8_t>(raw >> 8);
    }
}

} // namespace

MusicRuntime::MusicRuntime(const Catalog& catalog, MusicController& controller,
                           mods::sound::SfxMixer& mixer) noexcept
    : catalog_(catalog), controller_(controller), mixer_(mixer) {}

MusicRuntime::~MusicRuntime() noexcept {
    reset();
}

void MusicRuntime::reset() noexcept {
    if (source_ != 0) {
        mixer_.stop(source_);
        mixer_.delete_source(source_);
        source_ = 0;
    }
    if (buffer_ != 0) {
        mixer_.delete_buffer(buffer_);
        buffer_ = 0;
    }
    sequence_id_ = -1;
    track_mask_ = 0;
    rendered_frames_ = 0;
}

bool MusicRuntime::active() const noexcept {
    return source_ != 0
        && mixer_.source_state(source_)
            == ::fruityprime::sound::AudioSourceState::Playing;
}

bool MusicRuntime::start_sequence(std::int32_t sequence_id,
                                  std::uint16_t tracks,
                                  float volume, bool play_now) noexcept {
    if (!catalog_.sdat.has_value() || sequence_id < 0
        || static_cast<std::size_t>(sequence_id)
            >= catalog_.sdat->sequences().size()) {
        return false;
    }

    mods::sound::SfxMixer::Handle new_buffer = 0;
    mods::sound::SfxMixer::Handle new_source = 0;
    try {
        SseqPlayer player(*catalog_.sdat,
                          static_cast<std::size_t>(sequence_id));
        SseqRenderOptions options;
        options.sample_rate = mixer_.output_rate();
        options.gain = 1.0F;
        options.max_ticks = MaxRenderTicks;
        options.max_events = MaxRenderEvents;
        options.track_mask = tracks;
        const auto audio = player.render(RenderSeconds, options);
        if (audio.empty() || audio.size() % 2U != 0) {
            return false;
        }
        std::vector<std::uint8_t> bytes;
        pcm16_bytes(bytes, audio);

        new_buffer = mixer_.new_buffer();
        if (!mixer_.fill_buffer(
                new_buffer, AudioBufferFormat::Stereo16, bytes,
                mixer_.output_rate())) {
            mixer_.delete_buffer(new_buffer);
            return false;
        }
        new_source = mixer_.new_source();
        mixer_.set_relative(new_source, true);
        mixer_.set_looping(new_source, true);
        mixer_.set_buffer(new_source, new_buffer);
        mixer_.set_gain(new_source, clamp_volume(volume));
        mixer_.set_pitch(new_source, tempo_);

        reset();
        buffer_ = new_buffer;
        source_ = new_source;
        sequence_id_ = sequence_id;
        track_mask_ = tracks;
        volume_ = clamp_volume(volume);
        rendered_frames_ = audio.size() / 2U;
        if (play_now) {
            mixer_.play(source_);
        }
        return true;
    } catch (...) {
        if (new_source != 0) {
            mixer_.delete_source(new_source);
        }
        if (new_buffer != 0) {
            mixer_.delete_buffer(new_buffer);
        }
        return false;
    }
}

bool MusicRuntime::load_sequence(std::int32_t sequence_id,
                                 std::uint16_t tracks,
                                 float volume) noexcept {
    reset();
    return start_sequence(sequence_id, tracks, volume, false);
}

void MusicRuntime::play(float volume) noexcept {
    set_volume(volume);
    if (source_ != 0) {
        mixer_.play(source_);
    }
}

void MusicRuntime::pause() noexcept {
    if (source_ != 0) {
        mixer_.pause(source_);
    }
}

void MusicRuntime::stop() noexcept {
    if (source_ != 0) {
        mixer_.stop(source_);
    }
}

void MusicRuntime::set_volume(float volume) noexcept {
    volume_ = clamp_volume(volume);
    if (source_ != 0) {
        mixer_.set_gain(source_, volume_);
    }
}

void MusicRuntime::set_tempo(std::uint16_t tempo) noexcept {
    tempo_ = tempo_pitch(tempo);
    if (source_ != 0) {
        mixer_.set_pitch(source_, tempo_);
    }
}

std::uint16_t MusicRuntime::tempo() const noexcept {
    return static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(std::lround(tempo_ * 256.0F)), 0, 65535));
}

AudioSourceState MusicRuntime::state() const noexcept {
    return source_ == 0 ? AudioSourceState::Stopped
                        : mixer_.source_state(source_);
}

void MusicRuntime::apply_command(const MusicCommand& command) noexcept {
    switch (command.type) {
    case MusicCommandType::StartSequence:
        reset();
        if (!start_sequence(command.sequence_id, command.tracks,
                            command.volume)) {
            reset();
        }
        break;
    case MusicCommandType::Stop:
        reset();
        break;
    case MusicCommandType::SetVolume:
        volume_ = clamp_volume(command.volume);
        if (source_ != 0) {
            mixer_.set_gain(source_, volume_);
        }
        break;
    case MusicCommandType::SetTempo:
        tempo_ = tempo_pitch(command.value);
        if (source_ != 0) {
            mixer_.set_pitch(source_, tempo_);
        }
        break;
    case MusicCommandType::SetTrackVolume: {
        const auto previous_mask = track_mask_;
        if (command.value == 0) {
            track_mask_ = static_cast<std::uint16_t>(
                track_mask_ & static_cast<std::uint16_t>(~command.tracks));
        } else {
            track_mask_ = static_cast<std::uint16_t>(
                track_mask_ | command.tracks);
        }
        // Track fades are represented by endpoint mask changes here.  A
        // rebuild only occurs when the audible set changes, not every frame
        // while the controller interpolates the fader value.
        if (track_mask_ != previous_mask && sequence_id_ >= 0) {
            const auto sequence = sequence_id_;
            const auto volume = volume_;
            const auto tempo = tempo_;
            if (start_sequence(sequence, track_mask_, volume)
                && source_ != 0) {
                mixer_.set_pitch(source_, tempo);
            } else {
                // A transient allocation/decoder failure must not leave the
                // controller's mask claiming that the old buffer changed.
                track_mask_ = previous_mask;
            }
        }
        break;
    }
    }
}

void MusicRuntime::update(float seconds) noexcept {
    controller_.update(seconds);
    for (const auto& command : controller_.take_commands()) {
        apply_command(command);
    }
}

} // namespace fruityprime::sound
