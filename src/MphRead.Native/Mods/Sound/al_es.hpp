#pragma once

#include "Mods/Sound/sfx_mixer.hpp"

#include <cstdint>
#include <span>

namespace fruityprime::mods::sound {

// The Android OpenAL replacement only needs the subset of the OpenAL enum
// surface used by Sfx.cs and Formats/Movie.cs.  Keeping these enums explicit
// makes invalid calls visible at the native boundary instead of passing magic
// integers through the software mixer.
enum class AlFormat {
    Mono8,
    Stereo8,
    Mono16,
    Stereo16
};

enum class SourceBoolean {
    Looping,
    SourceRelative
};

enum class SourceInteger {
    Buffer
};

enum class SourceFloat {
    Gain,
    Pitch,
    ReferenceDistance,
    MaxDistance,
    RolloffFactor
};

enum class SourceVector {
    Position
};

enum class SourceQuery {
    SourceState,
    BuffersQueued,
    BuffersProcessed,
    Buffer
};

enum class ListenerVector {
    Position
};

enum class DistanceModel {
    LinearDistanceClamped
};

enum class BufferLoopPoint {
    LoopPointsSoft
};

enum class Error {
    NoError
};

enum class ContextError {
    NoError
};

enum class SourceState {
    Initial,
    Playing,
    Paused,
    Stopped
};

struct Device final {
    std::uintptr_t handle = 0;

    [[nodiscard]] bool valid() const noexcept { return handle != 0; }
};

struct Context final {
    std::uintptr_t handle = 0;

    [[nodiscard]] bool valid() const noexcept { return handle != 0; }
};

// Native counterpart of MphRead/Mods/Sound/AlEs.cs.  It is an object rather
// than a process-global singleton so a test or a future Android host can own
// its mixer explicitly while preserving the same OpenAL-shaped calls.
class AlEs final {
public:
    using Handle = SfxMixer::Handle;

    explicit AlEs(std::uint32_t output_rate = 32728);

    void gen_buffers(std::span<Handle> buffers);
    [[nodiscard]] Handle gen_buffer();
    void delete_buffers(std::span<const Handle> buffers) noexcept;
    void delete_buffer(Handle buffer) noexcept;

    void gen_sources(std::span<Handle> sources);
    [[nodiscard]] Handle gen_source();
    void delete_sources(std::span<const Handle> sources) noexcept;
    void delete_source(Handle source) noexcept;

    [[nodiscard]] bool buffer_data(Handle buffer, AlFormat format,
                                   std::span<const std::uint8_t> data,
                                   std::uint32_t sample_rate);
    void source(Handle source, SourceBoolean parameter, bool value) noexcept;
    void source(Handle source, SourceInteger parameter, Handle value) noexcept;
    void source(Handle source, SourceFloat parameter, float value) noexcept;
    void source(Handle source, SourceVector parameter,
                ::fruityprime::sound::AudioVector3 value) noexcept;

    void source_play(Handle source) noexcept;
    void source_stop(Handle source) noexcept;
    void source_pause(Handle source) noexcept;
    void source_queue_buffers(Handle source,
                              std::span<const Handle> buffers);
    void source_unqueue_buffers(Handle source, std::span<Handle> buffers) noexcept;

    [[nodiscard]] int get_source(Handle source, SourceQuery parameter) const noexcept;
    void listener(ListenerVector parameter,
                  ::fruityprime::sound::AudioVector3 value) noexcept;
    void listener_orientation(::fruityprime::sound::AudioVector3 facing,
                              ::fruityprime::sound::AudioVector3 up) noexcept;
    void distance_model(DistanceModel model) noexcept;

    [[nodiscard]] Error get_error() const noexcept { return Error::NoError; }
    [[nodiscard]] bool loop_points_extension_present() const noexcept {
        return true;
    }
    void set_loop_points(Handle buffer, BufferLoopPoint parameter,
                         std::int32_t start, std::int32_t end) noexcept;

    [[nodiscard]] SfxMixer& mixer() noexcept { return mixer_; }
    [[nodiscard]] const SfxMixer& mixer() const noexcept { return mixer_; }

private:
    SfxMixer mixer_;
    ::fruityprime::sound::AudioVector3 listener_position_{};
    ::fruityprime::sound::AudioVector3 listener_facing_{0.0F, 0.0F, -1.0F};
    ::fruityprime::sound::AudioVector3 listener_up_{0.0F, 1.0F, 0.0F};
};

// Native counterpart of the device/context half of AlEs.cs.  There is one
// output on Android, so these handles are bookkeeping around the mixer-owned
// stream rather than a second audio device.
class AlcEs final {
public:
    explicit AlcEs(AlEs& audio) noexcept : audio_(audio) {}

    [[nodiscard]] Device open_device() noexcept;
    [[nodiscard]] Context create_context(Device device) const noexcept;
    [[nodiscard]] bool make_context_current(Context context) const noexcept;
    [[nodiscard]] bool destroy_context(Context context) const noexcept;
    [[nodiscard]] bool close_device(Device device) noexcept;
    [[nodiscard]] ContextError get_error(Device device) const noexcept;

private:
    AlEs& audio_;
};

} // namespace fruityprime::mods::sound
