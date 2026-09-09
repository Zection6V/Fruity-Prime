#pragma once

#include "Sound/software_mixer.hpp"
#include "Sound/win32_audio_output.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace fruityprime::mods::sound {

// Native counterpart of Mods/Sound/SfxMixer.cs. The managed implementation
// attaches an OpenAL-like voice mixer to the music device; the native version
// keeps the same buffer/source lifecycle while leaving the final device write
// at the dependency-free float-block boundary owned by SoftwareMixer.  On
// Windows pump() additionally hands completed blocks to the WinMM output
// queue; the voice API itself remains portable.
class SfxMixer final {
public:
    using Handle = ::fruityprime::sound::SoftwareMixer::Handle;

    explicit SfxMixer(std::uint32_t output_rate = 32728);

    // Opening the native stream is deliberately device independent. A
    // platform output adapter can consume mix() later without changing any
    // gameplay call sites.
    [[nodiscard]] bool open() noexcept;
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept { return open_; }

    [[nodiscard]] Handle new_buffer();
    void delete_buffer(Handle buffer) noexcept;
    [[nodiscard]] Handle new_source();
    void delete_source(Handle source) noexcept;

    [[nodiscard]] bool fill_buffer(
        Handle buffer, ::fruityprime::sound::AudioBufferFormat format,
        std::span<const std::uint8_t> bytes, std::uint32_t sample_rate);
    void set_loop_points(Handle buffer, std::int32_t start,
                         std::int32_t end) noexcept;
    void set_looping(Handle source, bool enabled) noexcept;
    void set_relative(Handle source, bool relative) noexcept;
    void set_buffer(Handle source, Handle buffer) noexcept;
    void queue_buffers(Handle source, std::span<const Handle> buffers);
    [[nodiscard]] std::size_t unqueue_buffers(
        Handle source, std::span<Handle> buffers) noexcept;
    void set_gain(Handle source, float gain) noexcept;
    void set_pitch(Handle source, float pitch) noexcept;
    void set_position(Handle source,
                      ::fruityprime::sound::AudioVector3 position) noexcept;
    void set_reference_distance(Handle source, float distance) noexcept;
    void set_max_distance(Handle source, float distance) noexcept;
    void set_rolloff_factor(Handle source, float factor) noexcept;
    void play(Handle source) noexcept;
    void pause(Handle source) noexcept;
    void stop(Handle source) noexcept;

    [[nodiscard]] ::fruityprime::sound::AudioSourceState source_state(
        Handle source) const noexcept;
    [[nodiscard]] std::size_t buffers_queued(Handle source) const noexcept;
    [[nodiscard]] std::size_t buffers_processed(Handle source) const noexcept;
    [[nodiscard]] Handle current_buffer(Handle source) const noexcept;

    void set_listener(::fruityprime::sound::AudioVector3 position,
                      ::fruityprime::sound::AudioVector3 facing,
                      ::fruityprime::sound::AudioVector3 up) noexcept;
    void mix(std::span<float> output) noexcept;
    // Mix one device-sized block and queue it to the native output adapter.
    // Returns false when the mixer is closed, the platform has no output, or
    // all output blocks are still in flight.
    [[nodiscard]] bool pump() noexcept;
    [[nodiscard]] bool output_available() const noexcept {
        return output_ != nullptr && output_->running();
    }

    [[nodiscard]] std::uint32_t output_rate() const noexcept {
        return mixer_.output_rate();
    }

    [[nodiscard]] ::fruityprime::sound::SoftwareMixer& voices() noexcept {
        return mixer_;
    }

private:
    ::fruityprime::sound::SoftwareMixer mixer_;
    std::unique_ptr<::fruityprime::sound::Win32AudioOutput> output_;
    std::vector<float> mix_block_;
    std::vector<std::int16_t> pcm_block_;
    bool open_ = false;
};

} // namespace fruityprime::mods::sound
