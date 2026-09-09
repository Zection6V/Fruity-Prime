#include "Mods/Sound/sfx_mixer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fruityprime::mods::sound {

SfxMixer::SfxMixer(std::uint32_t output_rate) : mixer_(output_rate) {}

bool SfxMixer::open() noexcept {
    if (open_) {
        return true;
    }
    open_ = true;
    try {
        output_ = std::make_unique<
            ::fruityprime::sound::Win32AudioOutput>(mixer_.output_rate());
        // The software voice mixer remains usable when Windows has no
        // default device.  In that case open() still gives callers the same
        // portable lifecycle, while output_available() reports the missing
        // device and pump() stays silent.
        static_cast<void>(output_->open());
        if (output_->running()) {
            const std::size_t samples = static_cast<std::size_t>(
                output_->block_frames()) * 2;
            mix_block_.resize(samples);
            pcm_block_.resize(samples);
        }
    } catch (...) {
        output_.reset();
        mix_block_.clear();
        pcm_block_.clear();
    }
    return true;
}

void SfxMixer::close() noexcept {
    mixer_.stop_all();
    if (output_ != nullptr) {
        output_->close();
    }
    output_.reset();
    mix_block_.clear();
    pcm_block_.clear();
    open_ = false;
}

SfxMixer::Handle SfxMixer::new_buffer() {
    return mixer_.create_buffer();
}

void SfxMixer::delete_buffer(Handle buffer) noexcept {
    mixer_.delete_buffer(buffer);
}

SfxMixer::Handle SfxMixer::new_source() {
    return mixer_.create_source();
}

void SfxMixer::delete_source(Handle source) noexcept {
    mixer_.delete_source(source);
}

bool SfxMixer::fill_buffer(Handle buffer,
                           ::fruityprime::sound::AudioBufferFormat format,
                           std::span<const std::uint8_t> bytes,
                           std::uint32_t sample_rate) {
    return mixer_.fill_buffer(buffer, format, bytes, sample_rate);
}

void SfxMixer::set_loop_points(Handle buffer, std::int32_t start,
                               std::int32_t end) noexcept {
    mixer_.set_loop_points(buffer, start, end);
}

void SfxMixer::set_looping(Handle source, bool enabled) noexcept {
    mixer_.set_looping(source, enabled);
}

void SfxMixer::set_relative(Handle source, bool relative) noexcept {
    mixer_.set_relative(source, relative);
}

void SfxMixer::set_buffer(Handle source, Handle buffer) noexcept {
    mixer_.set_buffer(source, buffer);
}

void SfxMixer::queue_buffers(Handle source, std::span<const Handle> buffers) {
    mixer_.queue_buffers(source, buffers);
}

std::size_t SfxMixer::unqueue_buffers(Handle source,
                                      std::span<Handle> buffers) noexcept {
    return mixer_.unqueue_buffers(source, buffers);
}

void SfxMixer::set_gain(Handle source, float gain) noexcept {
    mixer_.set_gain(source, gain);
}

void SfxMixer::set_pitch(Handle source, float pitch) noexcept {
    mixer_.set_pitch(source, pitch);
}

void SfxMixer::set_position(
    Handle source, ::fruityprime::sound::AudioVector3 position) noexcept {
    mixer_.set_position(source, position);
}

void SfxMixer::set_reference_distance(Handle source, float distance) noexcept {
    mixer_.set_reference_distance(source, distance);
}

void SfxMixer::set_max_distance(Handle source, float distance) noexcept {
    mixer_.set_max_distance(source, distance);
}

void SfxMixer::set_rolloff_factor(Handle source, float factor) noexcept {
    mixer_.set_rolloff_factor(source, factor);
}

void SfxMixer::play(Handle source) noexcept {
    mixer_.play(source);
}

void SfxMixer::pause(Handle source) noexcept {
    mixer_.pause(source);
}

void SfxMixer::stop(Handle source) noexcept {
    mixer_.stop(source);
}

::fruityprime::sound::AudioSourceState SfxMixer::source_state(
    Handle source) const noexcept {
    return mixer_.source_state(source);
}

std::size_t SfxMixer::buffers_queued(Handle source) const noexcept {
    return mixer_.buffers_queued(source);
}

std::size_t SfxMixer::buffers_processed(Handle source) const noexcept {
    return mixer_.buffers_processed(source);
}

SfxMixer::Handle SfxMixer::current_buffer(Handle source) const noexcept {
    return mixer_.current_buffer(source);
}

void SfxMixer::set_listener(
    ::fruityprime::sound::AudioVector3 position,
    ::fruityprime::sound::AudioVector3 facing,
    ::fruityprime::sound::AudioVector3 up) noexcept {
    mixer_.set_listener(position, facing, up);
}

void SfxMixer::mix(std::span<float> output) noexcept {
    if (!open_) {
        std::fill(output.begin(), output.end(), 0.0F);
        return;
    }
    mixer_.mix_stereo(output);
}

bool SfxMixer::pump() noexcept {
    if (!open_ || output_ == nullptr || !output_->running()) {
        return false;
    }
    if (mix_block_.empty()) {
        const std::size_t samples = static_cast<std::size_t>(
            output_->block_frames()) * 2;
        mix_block_.resize(samples);
        pcm_block_.resize(samples);
    }
    mixer_.mix_stereo(mix_block_);
    for (std::size_t index = 0; index < mix_block_.size(); ++index) {
        const float scaled = std::clamp(
            mix_block_[index], -1.0F, 1.0F) * 32767.0F;
        pcm_block_[index] = static_cast<std::int16_t>(std::clamp(
            std::lround(scaled), static_cast<long>(std::numeric_limits<
                std::int16_t>::min()), static_cast<long>(std::numeric_limits<
                    std::int16_t>::max())));
    }
    return output_->submit(pcm_block_);
}

} // namespace fruityprime::mods::sound
