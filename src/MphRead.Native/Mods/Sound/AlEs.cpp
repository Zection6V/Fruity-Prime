#include "Mods/Sound/al_es.hpp"

#include <algorithm>

namespace fruityprime::mods::sound {
namespace {

[[nodiscard]] ::fruityprime::sound::AudioBufferFormat to_buffer_format(
    AlFormat format) noexcept {
    switch (format) {
    case AlFormat::Mono8:
        return ::fruityprime::sound::AudioBufferFormat::Mono8;
    case AlFormat::Stereo8:
        return ::fruityprime::sound::AudioBufferFormat::Stereo8;
    case AlFormat::Mono16:
        return ::fruityprime::sound::AudioBufferFormat::Mono16;
    case AlFormat::Stereo16:
        return ::fruityprime::sound::AudioBufferFormat::Stereo16;
    }
    return ::fruityprime::sound::AudioBufferFormat::Mono8;
}

[[nodiscard]] int source_state_value(
    ::fruityprime::sound::AudioSourceState state) noexcept {
    // The native API intentionally returns the portable state ordinal.  The
    // OpenAL aliases on each frontend can translate this to their enum values
    // without making the mixer depend on OpenTK or an Android SDK.
    return static_cast<int>(state);
}

} // namespace

AlEs::AlEs(std::uint32_t output_rate) : mixer_(output_rate) {}

void AlEs::gen_buffers(std::span<Handle> buffers) {
    for (Handle& buffer : buffers) {
        buffer = mixer_.new_buffer();
    }
}

AlEs::Handle AlEs::gen_buffer() {
    return mixer_.new_buffer();
}

void AlEs::delete_buffers(std::span<const Handle> buffers) noexcept {
    for (const Handle buffer : buffers) {
        mixer_.delete_buffer(buffer);
    }
}

void AlEs::delete_buffer(Handle buffer) noexcept {
    mixer_.delete_buffer(buffer);
}

void AlEs::gen_sources(std::span<Handle> sources) {
    for (Handle& source : sources) {
        source = mixer_.new_source();
    }
}

AlEs::Handle AlEs::gen_source() {
    return mixer_.new_source();
}

void AlEs::delete_sources(std::span<const Handle> sources) noexcept {
    for (const Handle source : sources) {
        mixer_.delete_source(source);
    }
}

void AlEs::delete_source(Handle source) noexcept {
    mixer_.delete_source(source);
}

bool AlEs::buffer_data(Handle buffer, AlFormat format,
                       std::span<const std::uint8_t> data,
                       std::uint32_t sample_rate) {
    return mixer_.fill_buffer(buffer, to_buffer_format(format), data,
                               sample_rate);
}

void AlEs::source(Handle source, SourceBoolean parameter, bool value) noexcept {
    switch (parameter) {
    case SourceBoolean::Looping:
        mixer_.set_looping(source, value);
        break;
    case SourceBoolean::SourceRelative:
        mixer_.set_relative(source, value);
        break;
    }
}

void AlEs::source(Handle source, SourceInteger parameter, Handle value) noexcept {
    switch (parameter) {
    case SourceInteger::Buffer:
        mixer_.set_buffer(source, value);
        break;
    }
}

void AlEs::source(Handle source, SourceFloat parameter, float value) noexcept {
    switch (parameter) {
    case SourceFloat::Gain:
        mixer_.set_gain(source, std::max(0.0F, value));
        break;
    case SourceFloat::Pitch:
        mixer_.set_pitch(source, std::clamp(value, 0.01F, 8.0F));
        break;
    case SourceFloat::ReferenceDistance:
        mixer_.set_reference_distance(source, value);
        break;
    case SourceFloat::MaxDistance:
        mixer_.set_max_distance(source, value);
        break;
    case SourceFloat::RolloffFactor:
        mixer_.set_rolloff_factor(source, value);
        break;
    }
}

void AlEs::source(Handle source, SourceVector parameter,
                  ::fruityprime::sound::AudioVector3 value) noexcept {
    switch (parameter) {
    case SourceVector::Position:
        mixer_.set_position(source, value);
        break;
    }
}

void AlEs::source_play(Handle source) noexcept {
    mixer_.play(source);
}

void AlEs::source_stop(Handle source) noexcept {
    mixer_.stop(source);
}

void AlEs::source_pause(Handle source) noexcept {
    mixer_.pause(source);
}

void AlEs::source_queue_buffers(Handle source,
                                std::span<const Handle> buffers) {
    mixer_.queue_buffers(source, buffers);
}

void AlEs::source_unqueue_buffers(Handle source,
                                  std::span<Handle> buffers) noexcept {
    (void)mixer_.unqueue_buffers(source, buffers);
}

int AlEs::get_source(Handle source, SourceQuery parameter) const noexcept {
    switch (parameter) {
    case SourceQuery::SourceState:
        return source_state_value(mixer_.source_state(source));
    case SourceQuery::BuffersQueued:
        return static_cast<int>(mixer_.buffers_queued(source));
    case SourceQuery::BuffersProcessed:
        return static_cast<int>(mixer_.buffers_processed(source));
    case SourceQuery::Buffer:
        return static_cast<int>(mixer_.current_buffer(source));
    }
    return 0;
}

void AlEs::listener(ListenerVector parameter,
                    ::fruityprime::sound::AudioVector3 value) noexcept {
    switch (parameter) {
    case ListenerVector::Position:
        listener_position_ = value;
        mixer_.set_listener(listener_position_, listener_facing_, listener_up_);
        break;
    }
}

void AlEs::listener_orientation(::fruityprime::sound::AudioVector3 facing,
                                ::fruityprime::sound::AudioVector3 up) noexcept {
    listener_facing_ = facing;
    listener_up_ = up;
    mixer_.set_listener(listener_position_, listener_facing_, listener_up_);
}

void AlEs::distance_model(DistanceModel model) noexcept {
    // SoftwareMixer implements the linear-clamped model used by the managed
    // adapter. The enum is retained at the call boundary for parity.
    static_cast<void>(model);
}

void AlEs::set_loop_points(Handle buffer, BufferLoopPoint parameter,
                           std::int32_t start, std::int32_t end) noexcept {
    switch (parameter) {
    case BufferLoopPoint::LoopPointsSoft:
        mixer_.set_loop_points(buffer, start, end);
        break;
    }
}

Device AlcEs::open_device() noexcept {
    return audio_.mixer().open() ? Device{1} : Device{};
}

Context AlcEs::create_context(Device device) const noexcept {
    return device.valid() ? Context{1} : Context{};
}

bool AlcEs::make_context_current(Context context) const noexcept {
    return context.valid();
}

bool AlcEs::destroy_context(Context context) const noexcept {
    static_cast<void>(context);
    return true;
}

bool AlcEs::close_device(Device device) noexcept {
    static_cast<void>(device);
    audio_.mixer().close();
    return true;
}

ContextError AlcEs::get_error(Device device) const noexcept {
    static_cast<void>(device);
    return ContextError::NoError;
}

} // namespace fruityprime::mods::sound
