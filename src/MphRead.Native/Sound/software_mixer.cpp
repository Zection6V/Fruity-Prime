#include "Sound/software_mixer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fruityprime::sound {
namespace {

[[nodiscard]] float clamp_gain(float value) noexcept {
    return std::max(0.0F, value);
}

[[nodiscard]] float clamp_pitch(float value) noexcept {
    return std::clamp(value, 0.01F, 8.0F);
}

[[nodiscard]] float length(AudioVector3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y
                     + value.z * value.z);
}

[[nodiscard]] AudioVector3 subtract(AudioVector3 left,
                                    AudioVector3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] AudioVector3 divide(AudioVector3 value, float divisor) noexcept {
    return {value.x / divisor, value.y / divisor, value.z / divisor};
}

[[nodiscard]] AudioVector3 cross(AudioVector3 left,
                                 AudioVector3 right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] float dot(AudioVector3 left, AudioVector3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] AudioVector3 normalized_or(AudioVector3 value,
                                         AudioVector3 fallback) noexcept {
    const float magnitude = length(value);
    if (magnitude <= std::numeric_limits<float>::epsilon()) {
        return fallback;
    }
    return divide(value, magnitude);
}

[[nodiscard]] std::int16_t read_i16(std::span<const std::uint8_t> bytes,
                                    std::size_t offset) noexcept {
    const auto raw = static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1]) << 8;
    return static_cast<std::int16_t>(raw);
}

} // namespace

SoftwareMixer::SoftwareMixer(std::uint32_t output_rate)
    : output_rate_(output_rate == 0 ? 32728 : output_rate) {}

SoftwareMixer::Handle SoftwareMixer::create_buffer() {
    std::lock_guard lock(mutex_);
    const Handle handle = next_buffer_++;
    buffers_.emplace(handle, Buffer{});
    return handle;
}

void SoftwareMixer::delete_buffer(Handle buffer) noexcept {
    std::lock_guard lock(mutex_);
    buffers_.erase(buffer);
}

SoftwareMixer::Handle SoftwareMixer::create_source() {
    std::lock_guard lock(mutex_);
    const Handle handle = next_source_++;
    sources_.emplace(handle, Source{});
    return handle;
}

void SoftwareMixer::delete_source(Handle source) noexcept {
    std::lock_guard lock(mutex_);
    sources_.erase(source);
}

bool SoftwareMixer::fill_buffer(Handle buffer, AudioBufferFormat format,
                                std::span<const std::uint8_t> bytes,
                                std::uint32_t sample_rate) {
    std::lock_guard lock(mutex_);
    auto found = buffers_.find(buffer);
    if (found == buffers_.end()) {
        return false;
    }

    const bool stereo = format == AudioBufferFormat::Stereo8
        || format == AudioBufferFormat::Stereo16;
    const bool sixteen = format == AudioBufferFormat::Mono16
        || format == AudioBufferFormat::Stereo16;
    const std::size_t channels = stereo ? 2 : 1;
    const std::size_t bytes_per_sample = sixteen ? 2 : 1;
    const std::size_t frame_bytes = channels * bytes_per_sample;
    if (frame_bytes == 0 || bytes.size() % frame_bytes != 0) {
        return false;
    }
    const std::size_t sample_count = bytes.size() / bytes_per_sample;
    if (sample_count > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }

    Buffer converted;
    converted.channels = static_cast<std::int32_t>(channels);
    converted.frames = static_cast<std::int32_t>(bytes.size() / frame_bytes);
    converted.sample_rate = sample_rate == 0 ? 22050 : sample_rate;
    converted.data.resize(sample_count);
    if (sixteen) {
        for (std::size_t i = 0; i < sample_count; ++i) {
            converted.data[i] = read_i16(bytes, i * 2) / 32768.0F;
        }
    } else {
        for (std::size_t i = 0; i < sample_count; ++i) {
            converted.data[i] =
                (static_cast<int>(bytes[i]) - 128) / 128.0F;
        }
    }
    found->second = std::move(converted);
    return true;
}

void SoftwareMixer::set_loop_points(Handle buffer, std::int32_t start,
                                     std::int32_t end) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = buffers_.find(buffer);
    if (found != buffers_.end()) {
        found->second.loop_start = start;
        found->second.loop_end = end;
    }
}

void SoftwareMixer::set_looping(Handle source, bool enabled) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    if (found != sources_.end()) {
        found->second.looping = enabled;
    }
}

void SoftwareMixer::set_relative(Handle source, bool relative) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    if (found != sources_.end()) {
        found->second.relative = relative;
    }
}

void SoftwareMixer::reset_source(Source& source) noexcept {
    source.queue.clear();
    source.index = 0;
    source.processed = 0;
    source.cursor = 0.0;
    source.state = AudioSourceState::Initial;
}

void SoftwareMixer::set_buffer(Handle source, Handle buffer) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    if (found == sources_.end()) {
        return;
    }
    reset_source(found->second);
    if (buffer != 0) {
        found->second.queue.push_back(buffer);
    }
}

void SoftwareMixer::queue_buffers(Handle source,
                                  std::span<const Handle> buffers) {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    if (found == sources_.end()) {
        return;
    }
    found->second.queue.insert(found->second.queue.end(), buffers.begin(),
                               buffers.end());
}

std::size_t SoftwareMixer::unqueue_buffers(Handle source,
                                           std::span<Handle> buffers) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    if (found == sources_.end()) {
        return 0;
    }
    Source& voice = found->second;
    const std::size_t count = std::min({buffers.size(), voice.processed,
                                        voice.queue.size()});
    std::copy_n(voice.queue.begin(), count, buffers.begin());
    voice.queue.erase(voice.queue.begin(), voice.queue.begin() + count);
    voice.processed -= count;
    voice.index = voice.index < count ? 0 : voice.index - count;
    return count;
}

void SoftwareMixer::set_gain(Handle source, float gain) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    if (found != sources_.end()) {
        found->second.gain = clamp_gain(gain);
    }
}

void SoftwareMixer::set_pitch(Handle source, float pitch) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    if (found != sources_.end()) {
        found->second.pitch = clamp_pitch(pitch);
    }
}

void SoftwareMixer::set_position(Handle source, AudioVector3 position) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    if (found != sources_.end()) {
        found->second.position = position;
    }
}

void SoftwareMixer::set_reference_distance(Handle source,
                                            float distance) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    if (found != sources_.end()) {
        found->second.reference_distance = distance;
    }
}

void SoftwareMixer::set_max_distance(Handle source, float distance) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    if (found != sources_.end()) {
        found->second.max_distance = distance;
    }
}

void SoftwareMixer::set_rolloff_factor(Handle source, float factor) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    if (found != sources_.end()) {
        found->second.rolloff_factor = factor;
    }
}

void SoftwareMixer::play(Handle source) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    if (found == sources_.end()) {
        return;
    }
    if (found->second.state != AudioSourceState::Paused) {
        found->second.index = 0;
        found->second.cursor = 0.0;
    }
    found->second.state = AudioSourceState::Playing;
}

void SoftwareMixer::pause(Handle source) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    if (found != sources_.end()
        && found->second.state == AudioSourceState::Playing) {
        found->second.state = AudioSourceState::Paused;
    }
}

void SoftwareMixer::stop(Handle source) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    if (found != sources_.end()) {
        found->second.state = AudioSourceState::Stopped;
        found->second.cursor = 0.0;
        found->second.index = 0;
    }
}

void SoftwareMixer::stop_all() noexcept {
    std::lock_guard lock(mutex_);
    for (auto& [unused, source] : sources_) {
        (void)unused;
        source.state = AudioSourceState::Stopped;
        source.cursor = 0.0;
        source.index = 0;
    }
}

AudioSourceState SoftwareMixer::source_state(Handle source) const noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    return found == sources_.end() ? AudioSourceState::Stopped
                                   : found->second.state;
}

std::size_t SoftwareMixer::buffers_queued(Handle source) const noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    return found == sources_.end() ? 0 : found->second.queue.size();
}

std::size_t SoftwareMixer::buffers_processed(Handle source) const noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    return found == sources_.end() ? 0 : found->second.processed;
}

SoftwareMixer::Handle SoftwareMixer::current_buffer(Handle source) const noexcept {
    std::lock_guard lock(mutex_);
    const auto found = sources_.find(source);
    if (found == sources_.end() || found->second.index >= found->second.queue.size()) {
        return 0;
    }
    return found->second.queue[found->second.index];
}

void SoftwareMixer::set_listener(AudioVector3 position, AudioVector3 facing,
                                 AudioVector3 up) noexcept {
    std::lock_guard lock(mutex_);
    listener_position_ = position;
    listener_facing_ = normalized_or(facing, {0.0F, 0.0F, -1.0F});
    listener_up_ = normalized_or(up, {0.0F, 1.0F, 0.0F});
}

void SoftwareMixer::placement(const Source& voice, float& left,
                              float& right) const noexcept {
    const AudioVector3 relative = voice.relative
        ? voice.position : subtract(voice.position, listener_position_);
    const float distance = length(relative);
    float attenuation = 1.0F;
    const float span = voice.max_distance - voice.reference_distance;
    if (voice.rolloff_factor > 0.0F && span > 0.0F
        && std::isfinite(span) && std::isfinite(voice.max_distance)) {
        const float clamped = std::clamp(distance, voice.reference_distance,
                                         voice.max_distance);
        attenuation = std::clamp(
            1.0F - voice.rolloff_factor * (clamped - voice.reference_distance)
                / span,
            0.0F, 1.0F);
    }

    float pan = 0.0F;
    if (distance > 0.0001F) {
        const AudioVector3 facing = voice.relative
            ? AudioVector3{0.0F, 0.0F, -1.0F} : listener_facing_;
        const AudioVector3 up = voice.relative
            ? AudioVector3{0.0F, 1.0F, 0.0F} : listener_up_;
        const AudioVector3 side = cross(facing, up);
        const float side_length = length(side);
        if (side_length > 0.0001F) {
            pan = std::clamp(dot(divide(relative, distance),
                                 divide(side, side_length)),
                             -1.0F, 1.0F);
        }
    }
    constexpr float EqualPower = 1.41421356237F;
    left = std::min(1.0F, std::sqrt(0.5F * (1.0F - pan)) * EqualPower)
        * attenuation;
    right = std::min(1.0F, std::sqrt(0.5F * (1.0F + pan)) * EqualPower)
        * attenuation;
}

void SoftwareMixer::mix_source(Source& voice, std::span<float> output) noexcept {
    if (voice.index >= voice.queue.size()) {
        voice.state = AudioSourceState::Stopped;
        return;
    }
    auto buffer_it = buffers_.find(voice.queue[voice.index]);
    if (buffer_it == buffers_.end() || buffer_it->second.frames <= 0) {
        voice.state = AudioSourceState::Stopped;
        return;
    }

    const auto* buffer = &buffer_it->second;
    float left = 0.0F;
    float right = 0.0F;
    placement(voice, left, right);
    const auto sample = [](const Buffer& value, std::int32_t frame,
                           std::int32_t channel) noexcept {
        return value.data[static_cast<std::size_t>(frame) * value.channels
                          + channel];
    };
    double step = buffer->sample_rate / static_cast<double>(output_rate_)
        * voice.pitch;
    for (std::size_t i = 0; i + 1 < output.size(); i += 2) {
        std::int32_t loop_start = 0;
        std::int32_t loop_end = buffer->frames;
        if (buffer->loop_end > buffer->loop_start && buffer->loop_start >= 0) {
            loop_start = std::min(buffer->loop_start, buffer->frames - 1);
            loop_end = std::min(buffer->loop_end, buffer->frames);
        }

        if (voice.cursor >= (voice.looping ? loop_end : buffer->frames)) {
            if (voice.looping && loop_end > loop_start) {
                voice.cursor = loop_start
                    + std::fmod(voice.cursor - loop_end,
                                static_cast<double>(loop_end - loop_start));
            } else if (voice.index + 1 < voice.queue.size()) {
                ++voice.index;
                ++voice.processed;
                voice.cursor = 0.0;
                buffer_it = buffers_.find(voice.queue[voice.index]);
                if (buffer_it == buffers_.end() || buffer_it->second.frames <= 0) {
                    voice.state = AudioSourceState::Stopped;
                    return;
                }
                buffer = &buffer_it->second;
                step = buffer->sample_rate / static_cast<double>(output_rate_)
                    * voice.pitch;
                loop_start = 0;
                loop_end = buffer->frames;
            } else {
                ++voice.processed;
                ++voice.index;
                voice.state = AudioSourceState::Stopped;
                return;
            }
        }

        const auto frame = static_cast<std::int32_t>(voice.cursor);
        const auto next = std::min(frame + 1, buffer->frames - 1);
        const float blend = static_cast<float>(voice.cursor - frame);
        float sample_left = sample(*buffer, frame, 0);
        float sample_right = sample_left;
        if (buffer->channels == 2) {
            sample_right = sample(*buffer, frame, 1);
            sample_left = sample_left +
                (sample(*buffer, next, 0) - sample_left) * blend;
            sample_right = sample_right +
                (sample(*buffer, next, 1) - sample_right) * blend;
        } else {
            sample_left = sample_left +
                (sample(*buffer, next, 0) - sample_left) * blend;
            sample_right = sample_left;
        }
        output[i] += sample_left * left * voice.gain;
        output[i + 1] += sample_right * right * voice.gain;
        voice.cursor += step;
    }
}

void SoftwareMixer::mix_stereo(std::span<float> output) noexcept {
    std::lock_guard lock(mutex_);
    std::fill(output.begin(), output.end(), 0.0F);
    for (auto& [unused, voice] : sources_) {
        (void)unused;
        if (voice.state == AudioSourceState::Playing) {
            mix_source(voice, output);
        }
    }
    for (float& value : output) {
        value = std::clamp(value, -1.0F, 1.0F);
    }
}

} // namespace fruityprime::sound
