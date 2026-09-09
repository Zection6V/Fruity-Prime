#pragma once

#include "Formats/sound_catalog.hpp"
#include "Sound/software_mixer.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace fruityprime::mods::sound {
class SfxMixer;
}

namespace fruityprime::sound {

// The managed SoundSource stores the spatial parameters used by every SFX
// instance. Keeping it as a small value type makes it usable by gameplay,
// entities, and a future renderer without depending on an audio API.
struct SoundSource {
    AudioVector3 position;
    float reference_distance = 1.0F;
    float max_distance = std::numeric_limits<float>::max();
    float rolloff_factor = 1.0F;
    float volume = 1.0F;
    bool self = false;

    void update(AudioVector3 new_position, std::int32_t range_index,
                std::span<const Sound3dEntry> ranges) noexcept;
};

// Runtime equivalent of the useful, device-independent part of Sound/Sfx.cs.
// Samples are decoded into the shared voice mixer on first use, while scripts
// retain their cartridge delay/pan/pitch fields until the frame that emits
// them. SDAT music remains a separate sequencer API.
class SfxRuntime final {
public:
    using Handle = std::uint32_t;
    static constexpr Handle InvalidHandle = 0;

    SfxRuntime(const Catalog& catalog, mods::sound::SfxMixer& mixer);
    ~SfxRuntime();

    SfxRuntime(const SfxRuntime&) = delete;
    SfxRuntime& operator=(const SfxRuntime&) = delete;

    // Opening remains successful when the host has no default audio device;
    // the mixer still provides deterministic state and float output for
    // headless tests and later platform adapters.
    [[nodiscard]] bool load() noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool loaded() const noexcept { return loaded_; }

    void set_volume(float volume) noexcept;
    [[nodiscard]] float volume() const noexcept { return volume_; }

    void set_listener(AudioVector3 position, AudioVector3 facing,
                      AudioVector3 up) noexcept;

    [[nodiscard]] Handle play_sample(
        std::int32_t id, SoundSource* source = nullptr,
        std::optional<bool> loop = std::nullopt, bool no_update = false,
        float recency = -1.0F, bool source_only = false,
        bool cancellable = false);

    [[nodiscard]] Handle play_free_sfx(std::int32_t id);

    [[nodiscard]] Handle play_script(
        std::int32_t id, SoundSource* source = nullptr,
        bool no_update = false, float recency = -1.0F,
        bool source_only = false, bool cancellable = false);

    [[nodiscard]] Handle play_dgn(
        std::int32_t id, SoundSource* source = nullptr, bool loop = false,
        bool no_update = false, float recency = -1.0F,
        bool source_only = true, bool cancellable = false,
        float amount_a = 0.0F, float amount_b = 0.0F);

    // Dispatches the same encoded ID convention as SoundSource.PlaySfx:
    // 0x4000 selects a script and 0x8000 selects a DGN curve.
    [[nodiscard]] Handle play_encoded(
        std::int32_t id, SoundSource* source = nullptr,
        std::optional<bool> loop = std::nullopt, bool no_update = false,
        float recency = -1.0F, bool source_only = false,
        bool cancellable = false, float amount_a = 0.0F,
        float amount_b = 0.0F);

    void update(float seconds) noexcept;
    [[nodiscard]] bool pump() noexcept;

    void stop_handle(Handle handle) noexcept;
    void stop_sound_by_id(std::int32_t id) noexcept;
    void stop_source(SoundSource* source, bool force = false) noexcept;
    void stop_sound_from_source(SoundSource* source,
                                std::int32_t id) noexcept;
    void stop_scripts() noexcept;
    void set_scripts_paused(bool paused) noexcept;
    void stop_all() noexcept;

    [[nodiscard]] bool is_handle_playing(Handle handle) const noexcept;
    [[nodiscard]] std::size_t count_playing(std::int32_t id) const noexcept;
    [[nodiscard]] std::size_t count_source_playing(
        std::int32_t id, const SoundSource* source) const noexcept;
    [[nodiscard]] std::size_t cached_sample_count() const noexcept {
        return buffers_.size();
    }

private:
    struct Buffer {
        Handle mixer_buffer = 0;
        float sample_volume = 1.0F;
    };

    struct Channel {
        Handle mixer_source = 0;
        std::int32_t sample_id = -1;
        std::size_t dgn_entry = std::numeric_limits<std::size_t>::max();
        float gain = 1.0F;
        float pitch = 1.0F;
        bool looping = false;
        std::optional<float> pan;
    };

    struct Instance {
        Handle handle = InvalidHandle;
        std::int32_t sound_id = -1;
        SoundSource* source = nullptr;
        bool no_update = false;
        bool source_only = false;
        bool cancellable = false;
        bool script = false;
        bool dgn = false;
        bool paused = false;
        float play_time = 0.0F;
        std::size_t script_index = 0;
        std::int32_t dgn_id = -1;
        float amount_a = 0.0F;
        float amount_b = 0.0F;
        std::vector<Channel> channels;
    };

    [[nodiscard]] Buffer* ensure_buffer(std::int32_t id);
    [[nodiscard]] bool create_channel(Channel& channel,
                                       std::int32_t sample_id, float gain,
                                       float pitch, bool looping,
                                       std::optional<float> pan,
                                       SoundSource* source,
                                       std::size_t dgn_entry =
                                           std::numeric_limits<std::size_t>::max());
    void refresh_channel(const Channel& channel, const Instance& instance) noexcept;
    void stop_channel(Channel& channel) noexcept;
    void stop_instance(Instance& instance) noexcept;
    void remove_finished_channels(Instance& instance) noexcept;
    void update_script(Instance& instance) noexcept;
    [[nodiscard]] Handle next_handle() noexcept;
    [[nodiscard]] bool recent_instance(std::int32_t id, float recency,
                                       const SoundSource* source,
                                       bool source_only,
                                       Handle& handle) const noexcept;
    [[nodiscard]] float sample_volume(std::int32_t id) const noexcept;
    [[nodiscard]] float dgn_value(std::span<const DgnData> data,
                                  float amount) const noexcept;

    const Catalog& catalog_;
    mods::sound::SfxMixer& mixer_;
    std::unordered_map<std::int32_t, Buffer> buffers_;
    std::unordered_map<Handle, Instance> instances_;
    Handle next_handle_ = 1;
    float volume_ = 0.35F;
    bool loaded_ = false;
};

} // namespace fruityprime::sound
