#pragma once

#include <cstdint>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>

namespace fruityprime::sound {

struct AudioVector3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

enum class AudioBufferFormat : std::uint8_t {
    Mono8,
    Stereo8,
    Mono16,
    Stereo16,
};

enum class AudioSourceState : std::uint8_t {
    Initial,
    Playing,
    Paused,
    Stopped,
};

// Dependency-free equivalent of the SfxMixer used by the managed Android
// path. It deliberately stops at interleaved float output: a WinMM/OpenAL or
// Android audio device can consume the result without entering gameplay.
class SoftwareMixer {
public:
    using Handle = std::uint32_t;

    explicit SoftwareMixer(std::uint32_t output_rate = 32728);

    [[nodiscard]] Handle create_buffer();
    void delete_buffer(Handle buffer) noexcept;
    [[nodiscard]] Handle create_source();
    void delete_source(Handle source) noexcept;

    [[nodiscard]] bool fill_buffer(Handle buffer, AudioBufferFormat format,
                                   std::span<const std::uint8_t> bytes,
                                   std::uint32_t sample_rate);
    void set_loop_points(Handle buffer, std::int32_t start,
                         std::int32_t end) noexcept;

    void set_looping(Handle source, bool enabled) noexcept;
    void set_relative(Handle source, bool relative) noexcept;
    void set_buffer(Handle source, Handle buffer) noexcept;
    void queue_buffers(Handle source, std::span<const Handle> buffers);
    std::size_t unqueue_buffers(Handle source, std::span<Handle> buffers) noexcept;
    void set_gain(Handle source, float gain) noexcept;
    void set_pitch(Handle source, float pitch) noexcept;
    void set_position(Handle source, AudioVector3 position) noexcept;
    void set_reference_distance(Handle source, float distance) noexcept;
    void set_max_distance(Handle source, float distance) noexcept;
    void set_rolloff_factor(Handle source, float factor) noexcept;
    void play(Handle source) noexcept;
    void pause(Handle source) noexcept;
    void stop(Handle source) noexcept;
    void stop_all() noexcept;

    [[nodiscard]] AudioSourceState source_state(Handle source) const noexcept;
    [[nodiscard]] std::size_t buffers_queued(Handle source) const noexcept;
    [[nodiscard]] std::size_t buffers_processed(Handle source) const noexcept;
    [[nodiscard]] Handle current_buffer(Handle source) const noexcept;

    void set_listener(AudioVector3 position, AudioVector3 facing,
                      AudioVector3 up) noexcept;

    // Mixes an interleaved stereo float block in [-1, 1]. The function keeps
    // the audio callback boundary deterministic and clips accumulated voices
    // instead of allowing a float sum to escape the documented range.
    void mix_stereo(std::span<float> output) noexcept;

    [[nodiscard]] std::uint32_t output_rate() const noexcept {
        return output_rate_;
    }

private:
    struct Buffer {
        std::vector<float> data;
        std::int32_t channels = 1;
        std::int32_t frames = 0;
        std::uint32_t sample_rate = 22050;
        std::int32_t loop_start = -1;
        std::int32_t loop_end = -1;
    };

    struct Source {
        std::vector<Handle> queue;
        std::size_t index = 0;
        std::size_t processed = 0;
        double cursor = 0.0;
        AudioSourceState state = AudioSourceState::Initial;
        float gain = 1.0F;
        float pitch = 1.0F;
        bool looping = false;
        bool relative = false;
        AudioVector3 position;
        float reference_distance = 1.0F;
        float max_distance = 3.402823466e+38F;
        float rolloff_factor = 1.0F;
    };

    void reset_source(Source& source) noexcept;
    void mix_source(Source& source, std::span<float> output) noexcept;
    void placement(const Source& source, float& left, float& right) const noexcept;

    std::uint32_t output_rate_;
    Handle next_buffer_ = 1;
    Handle next_source_ = 1;
    std::unordered_map<Handle, Buffer> buffers_;
    std::unordered_map<Handle, Source> sources_;
    mutable std::mutex mutex_;
    AudioVector3 listener_position_;
    AudioVector3 listener_facing_{0.0F, 0.0F, -1.0F};
    AudioVector3 listener_up_{0.0F, 1.0F, 0.0F};
};

} // namespace fruityprime::sound
