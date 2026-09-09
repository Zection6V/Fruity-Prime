#include "Sound/sfx_runtime.hpp"

#include "Mods/Sound/sfx_mixer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace fruityprime::sound {
namespace {

[[nodiscard]] float clamp_nonnegative(float value) noexcept {
    if (!std::isfinite(value)) {
        return 0.0F;
    }
    return std::max(0.0F, value);
}

[[nodiscard]] float calculate_pitch_div(float pitch_factor) noexcept {
    if (!std::isfinite(pitch_factor) || pitch_factor <= 0.0F) {
        pitch_factor = 1.0F;
    }
    const auto pitch = static_cast<std::uint32_t>(pitch_factor);
    std::int32_t pitch_int = static_cast<std::int32_t>(pitch);
    if (pitch <= 0x0FFF) {
        pitch_int = -static_cast<std::int32_t>((0x600000U / pitch) >> 1);
    } else if (pitch <= 0x1FFF) {
        pitch_int = static_cast<std::int32_t>(
            (768U * (pitch - 0x2000U)) >> 12);
    } else {
        pitch_int = static_cast<std::int32_t>(
            (768U * (pitch - 0x2000U)) >> 13);
    }
    const float semitones = static_cast<float>(pitch_int) / 64.0F;
    const float octaves = std::abs(semitones / 12.0F);
    return semitones >= 0.0F ? std::pow(2.0F, octaves)
                            : std::pow(0.5F, octaves);
}

[[nodiscard]] std::optional<float> decode_pan(std::uint8_t raw) noexcept {
    if (raw == 0xFF) {
        return std::nullopt;
    }
    int value = raw;
    if (value == 127) {
        value = 128;
    }
    return std::clamp(static_cast<float>(value - 64) / 128.0F,
                      -1.0F, 1.0F);
}

void append_pcm16_bytes(std::vector<std::uint8_t>& destination,
                        std::span<const std::int16_t> samples) {
    destination.resize(samples.size() * 2);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto value = static_cast<std::uint16_t>(samples[index]);
        destination[index * 2] = static_cast<std::uint8_t>(value);
        destination[index * 2 + 1] = static_cast<std::uint8_t>(value >> 8);
    }
}

} // namespace

void SoundSource::update(AudioVector3 new_position,
                         std::int32_t range_index,
                         std::span<const Sound3dEntry> ranges) noexcept {
    if (range_index == -1) {
        position = new_position;
        reference_distance = std::numeric_limits<float>::max();
        max_distance = std::numeric_limits<float>::max();
        self = true;
        return;
    }

    position = new_position;
    self = false;
    if (range_index >= 0
        && static_cast<std::size_t>(range_index) < ranges.size()) {
        const auto& range = ranges[static_cast<std::size_t>(range_index)];
        reference_distance = range.falloff_distance / 4096.0F;
        max_distance = range.max_distance / 4096.0F;
    }
}

SfxRuntime::SfxRuntime(const Catalog& catalog, mods::sound::SfxMixer& mixer)
    : catalog_(catalog), mixer_(mixer) {}

SfxRuntime::~SfxRuntime() {
    shutdown();
}

bool SfxRuntime::load() noexcept {
    shutdown();
    try {
        loaded_ = mixer_.open();
    } catch (...) {
        loaded_ = false;
    }
    return loaded_;
}

void SfxRuntime::shutdown() noexcept {
    stop_all();
    mixer_.close();
    buffers_.clear();
    loaded_ = false;
}

void SfxRuntime::set_volume(float volume) noexcept {
    volume_ = clamp_nonnegative(volume);
}

void SfxRuntime::set_listener(AudioVector3 position, AudioVector3 facing,
                              AudioVector3 up) noexcept {
    mixer_.set_listener(position, facing, up);
}

SfxRuntime::Buffer* SfxRuntime::ensure_buffer(std::int32_t id) {
    const auto cached = buffers_.find(id);
    if (cached != buffers_.end()) {
        return &cached->second;
    }
    if (id < 0 || static_cast<std::size_t>(id) >= catalog_.samples.size()) {
        return nullptr;
    }
    const Sample& sample = catalog_.samples[static_cast<std::size_t>(id)];
    if (!sample.present) {
        return nullptr;
    }

    const auto pcm = sample.decode_pcm();
    if (pcm.empty()) {
        return nullptr;
    }
    std::vector<std::uint8_t> bytes;
    append_pcm16_bytes(bytes, pcm);

    Buffer result;
    result.mixer_buffer = mixer_.new_buffer();
    try {
        if (!mixer_.fill_buffer(result.mixer_buffer,
                                AudioBufferFormat::Mono16, bytes,
                                sample.sample_rate)) {
            mixer_.delete_buffer(result.mixer_buffer);
            return nullptr;
        }
        if (sample.loop_length > 0) {
            const auto max_index = static_cast<std::uint32_t>(
                std::min<std::size_t>(pcm.size(),
                                      std::numeric_limits<std::int32_t>::max()));
            const auto start = std::min(sample.loop_start, max_index);
            const auto end = static_cast<std::uint32_t>(std::min<std::uint64_t>(
                static_cast<std::uint64_t>(sample.loop_start)
                    + sample.loop_length,
                max_index));
            if (end > start) {
                mixer_.set_loop_points(
                    result.mixer_buffer, static_cast<std::int32_t>(start),
                    static_cast<std::int32_t>(end));
            }
        }
        result.sample_volume = sample_volume(id);
        const auto [inserted, was_inserted] = buffers_.emplace(id, result);
        if (!was_inserted) {
            mixer_.delete_buffer(result.mixer_buffer);
        }
        return &inserted->second;
    } catch (...) {
        mixer_.delete_buffer(result.mixer_buffer);
        throw;
    }
}

float SfxRuntime::sample_volume(std::int32_t id) const noexcept {
    if (id >= 0 && static_cast<std::size_t>(id)
            < catalog_.sound_tables.entries.size()) {
        return catalog_.sound_tables.entries[static_cast<std::size_t>(id)]
                   .initial_volume
            / 127.0F;
    }
    return 1.0F;
}

float SfxRuntime::dgn_value(std::span<const DgnData> data,
                            float amount) const noexcept {
    if (data.empty()) {
        return 0.0F;
    }
    const auto value = [](const DgnData& point) noexcept {
        return static_cast<float>(point.value & 0x3FFF);
    };
    if (amount <= data.front().amount) {
        return value(data.front());
    }
    if (amount >= data.back().amount) {
        return value(data.back());
    }
    if (data.size() == 1) {
        return 0.0F;
    }
    for (std::size_t index = 0; index + 1 < data.size(); ++index) {
        const auto& first = data[index];
        const auto& second = data[index + 1];
        if (amount >= second.amount) {
            continue;
        }
        const float denominator =
            static_cast<float>(second.amount) - first.amount;
        if (denominator <= 0.0F) {
            return value(first);
        }
        const float ratio = std::clamp(
            (amount - first.amount) / denominator, 0.0F, 1.0F);
        const float first_value = value(first);
        const float second_value = value(second);
        if ((second.value & 0xC000) == 0x4000) {
            return first_value + (second_value - first_value)
                * std::sin(1.57079632679F * ratio);
        }
        // The managed reader treats the remaining interpolation flag as a
        // linear curve. The unsupported flag is therefore still safe and
        // deterministic instead of exposing a corrupt sound value.
        return first_value + (second_value - first_value) * ratio;
    }
    return 0.0F;
}

bool SfxRuntime::create_channel(Channel& channel, std::int32_t sample_id,
                                float gain, float pitch, bool looping,
                                std::optional<float> pan,
                                SoundSource* source,
                                std::size_t dgn_entry) {
    Buffer* buffer = ensure_buffer(sample_id);
    if (buffer == nullptr) {
        return false;
    }

    channel = {};
    channel.sample_id = sample_id;
    channel.gain = clamp_nonnegative(gain);
    channel.pitch = std::isfinite(pitch) && pitch > 0.0F ? pitch : 1.0F;
    channel.looping = looping;
    channel.pan = pan;
    channel.dgn_entry = dgn_entry;
    try {
        channel.mixer_source = mixer_.new_source();
        mixer_.set_buffer(channel.mixer_source, buffer->mixer_buffer);
        mixer_.set_looping(channel.mixer_source, looping);
        Instance initial;
        initial.source = source;
        refresh_channel(channel, initial);
        mixer_.play(channel.mixer_source);
        return true;
    } catch (...) {
        if (channel.mixer_source != 0) {
            mixer_.delete_source(channel.mixer_source);
            channel.mixer_source = 0;
        }
        throw;
    }
}

void SfxRuntime::refresh_channel(const Channel& channel,
                                 const Instance& instance) noexcept {
    if (channel.mixer_source == 0) {
        return;
    }
    const auto cached = buffers_.find(channel.sample_id);
    const float sample_gain = cached == buffers_.end()
        ? 1.0F : cached->second.sample_volume;
    const float source_gain = instance.source == nullptr
        ? 1.0F : clamp_nonnegative(instance.source->volume);
    float voice_gain = channel.gain;
    float voice_pitch = channel.pitch;
    if (instance.dgn && instance.dgn_id >= 0
        && static_cast<std::size_t>(instance.dgn_id)
            < catalog_.dgn_files.size()
        && channel.dgn_entry < catalog_.dgn_files[
            static_cast<std::size_t>(instance.dgn_id)].entries.size()) {
        const auto& file = catalog_.dgn_files[
            static_cast<std::size_t>(instance.dgn_id)];
        const auto& entry = file.entries[channel.dgn_entry];
        const float volume_a = dgn_value(entry.data[0], instance.amount_a);
        const float volume_b = dgn_value(entry.data[1], instance.amount_b);
        voice_gain = volume_a / 127.0F * volume_b / 127.0F
            * file.header.initial_volume / 127.0F;
        if (voice_gain < 1.0F / 130.0F) {
            voice_gain = 0.0F;
        }
        const float pitch_a = dgn_value(entry.data[2], instance.amount_a);
        const float pitch_b = dgn_value(entry.data[3], instance.amount_b);
        const float pitch_factor = std::clamp(
            pitch_a / 8192.0F * pitch_b, 0.0F, 16383.0F);
        voice_pitch = calculate_pitch_div(pitch_factor);
    }
    mixer_.set_gain(channel.mixer_source,
                    volume_ * voice_gain * sample_gain * source_gain);
    mixer_.set_pitch(channel.mixer_source, voice_pitch);
    if (channel.pan.has_value()) {
        const float pan = std::clamp(*channel.pan, -1.0F, 1.0F);
        const float z = -std::sqrt(std::max(0.0F, 1.0F - pan * pan));
        mixer_.set_relative(channel.mixer_source, true);
        mixer_.set_position(channel.mixer_source, {pan, 0.0F, z});
        mixer_.set_reference_distance(channel.mixer_source,
                                      std::numeric_limits<float>::max());
        mixer_.set_max_distance(channel.mixer_source,
                                 std::numeric_limits<float>::max());
        mixer_.set_rolloff_factor(channel.mixer_source, 0.0F);
    } else if (instance.source != nullptr && !instance.source->self) {
        if (!instance.no_update) {
            mixer_.set_position(channel.mixer_source,
                                instance.source->position);
            mixer_.set_reference_distance(
                channel.mixer_source, instance.source->reference_distance);
            mixer_.set_max_distance(channel.mixer_source,
                                    instance.source->max_distance);
            mixer_.set_rolloff_factor(channel.mixer_source,
                                      instance.source->rolloff_factor);
        }
        mixer_.set_relative(channel.mixer_source, false);
    } else {
        mixer_.set_relative(channel.mixer_source, true);
        mixer_.set_position(channel.mixer_source, {0.0F, 0.0F, 0.0F});
        mixer_.set_reference_distance(channel.mixer_source,
                                      std::numeric_limits<float>::max());
        mixer_.set_max_distance(channel.mixer_source,
                                 std::numeric_limits<float>::max());
        mixer_.set_rolloff_factor(channel.mixer_source, 0.0F);
    }
}

void SfxRuntime::stop_channel(Channel& channel) noexcept {
    if (channel.mixer_source != 0) {
        mixer_.stop(channel.mixer_source);
        mixer_.delete_source(channel.mixer_source);
        channel.mixer_source = 0;
    }
}

void SfxRuntime::stop_instance(Instance& instance) noexcept {
    for (Channel& channel : instance.channels) {
        stop_channel(channel);
    }
    instance.channels.clear();
}

void SfxRuntime::remove_finished_channels(Instance& instance) noexcept {
    auto channel = instance.channels.begin();
    while (channel != instance.channels.end()) {
        if (mixer_.source_state(channel->mixer_source)
            == AudioSourceState::Stopped) {
            stop_channel(*channel);
            channel = instance.channels.erase(channel);
        } else {
            ++channel;
        }
    }
}

void SfxRuntime::update_script(Instance& instance) noexcept {
    const std::int32_t id = instance.sound_id & 0x3FFF;
    if (id < 0 || static_cast<std::size_t>(id)
            >= catalog_.sfx_script_files.size()) {
        instance.script_index = std::numeric_limits<std::size_t>::max();
        stop_instance(instance);
        return;
    }
    const auto& script = catalog_.sfx_script_files[static_cast<std::size_t>(id)];
    try {
        while (instance.script_index < script.entries.size()) {
            const auto& entry = script.entries[instance.script_index];
            const float delay = entry.delay_frames / 30.0F;
            if (delay > instance.play_time) {
                break;
            }
            ++instance.script_index;
            const std::int32_t sample_id = entry.sfx_id & 0x3FFF;
            if ((entry.sfx_id & 0x8000) != 0) {
                for (auto channel = instance.channels.begin();
                     channel != instance.channels.end();) {
                    if (channel->sample_id == sample_id) {
                        stop_channel(*channel);
                        channel = instance.channels.erase(channel);
                    } else {
                        ++channel;
                    }
                }
                continue;
            }
            if (instance.channels.size() >= 12) {
                stop_instance(instance);
                instance.script_index = script.entries.size();
                return;
            }
            Channel channel;
            const float gain = entry.volume / 127.0F
                * script.header.initial_volume / 127.0F;
            if (!create_channel(channel, sample_id, gain,
                                calculate_pitch_div(entry.pitch),
                                (entry.sfx_id & 0x4000) != 0,
                                decode_pan(entry.pan), instance.source)) {
                stop_instance(instance);
                instance.script_index = script.entries.size();
                return;
            }
            instance.channels.push_back(std::move(channel));
        }
    } catch (...) {
        stop_instance(instance);
        instance.script_index = script.entries.size();
    }
}

SfxRuntime::Handle SfxRuntime::next_handle() noexcept {
    const Handle result = next_handle_ == InvalidHandle ? 1 : next_handle_;
    ++next_handle_;
    if (next_handle_ == InvalidHandle) {
        next_handle_ = 1;
    }
    return result;
}

bool SfxRuntime::recent_instance(std::int32_t id, float recency,
                                 const SoundSource* source, bool source_only,
                                 Handle& handle) const noexcept {
    if (recency < 0.0F || !std::isfinite(recency)) {
        return false;
    }
    for (const auto& [candidate_handle, instance] : instances_) {
        if (instance.sound_id == id && instance.play_time <= recency
            && (!source_only || instance.source == source)) {
            handle = candidate_handle;
            return true;
        }
    }
    return false;
}

SfxRuntime::Handle SfxRuntime::play_sample(
    std::int32_t id, SoundSource* source, std::optional<bool> loop,
    bool no_update, float recency, bool source_only, bool cancellable) {
    if (!loaded_ || id < 0 || static_cast<std::size_t>(id)
            >= catalog_.samples.size() || !catalog_.samples[id].present) {
        return InvalidHandle;
    }
    Handle recent = InvalidHandle;
    if (recent_instance(id, recency, source, source_only, recent)) {
        return recent;
    }
    try {
        Instance instance;
        instance.handle = next_handle();
        instance.sound_id = id;
        instance.source = source;
        instance.no_update = no_update;
        instance.source_only = source_only;
        instance.cancellable = cancellable;
        Channel channel;
        const bool should_loop = loop.value_or(
            catalog_.samples[static_cast<std::size_t>(id)].loop);
        if (!create_channel(channel, id, 1.0F, 1.0F, should_loop,
                            std::nullopt, source)) {
            return InvalidHandle;
        }
        try {
            instance.channels.push_back(channel);
            channel.mixer_source = 0;
        } catch (...) {
            stop_channel(channel);
            throw;
        }
        const Handle result = instance.handle;
        try {
            instances_.emplace(result, std::move(instance));
        } catch (...) {
            stop_instance(instance);
            throw;
        }
        return result;
    } catch (...) {
        return InvalidHandle;
    }
}

SfxRuntime::Handle SfxRuntime::play_free_sfx(std::int32_t id) {
    return play_sample(id, nullptr, std::nullopt, false, -1.0F, false,
                       false);
}

SfxRuntime::Handle SfxRuntime::play_script(
    std::int32_t id, SoundSource* source, bool no_update, float recency,
    bool source_only, bool cancellable) {
    if (!loaded_) {
        return InvalidHandle;
    }
    const std::int32_t script_id = id & 0x3FFF;
    if (script_id < 0 || static_cast<std::size_t>(script_id)
            >= catalog_.sfx_script_files.size()
        || catalog_.sfx_script_files[static_cast<std::size_t>(script_id)]
               .entries.empty()) {
        return InvalidHandle;
    }
    const std::int32_t sound_id = 0x4000 | script_id;
    Handle recent = InvalidHandle;
    if (recent_instance(sound_id, recency, source, source_only, recent)) {
        return recent;
    }
    try {
        Instance instance;
        instance.handle = next_handle();
        instance.sound_id = sound_id;
        instance.source = source;
        instance.no_update = no_update;
        instance.source_only = source_only;
        instance.cancellable = cancellable;
        instance.script = true;
        const Handle result = instance.handle;
        instances_.emplace(result, std::move(instance));
        return result;
    } catch (...) {
        return InvalidHandle;
    }
}

SfxRuntime::Handle SfxRuntime::play_dgn(
    std::int32_t id, SoundSource* source, bool loop, bool no_update,
    float recency, bool source_only, bool cancellable, float amount_a,
    float amount_b) {
    if (!loaded_) {
        return InvalidHandle;
    }
    const std::int32_t dgn_id = id & 0x3FFF;
    if (dgn_id < 0 || static_cast<std::size_t>(dgn_id)
            >= catalog_.dgn_files.size()) {
        return InvalidHandle;
    }
    const auto& file = catalog_.dgn_files[static_cast<std::size_t>(dgn_id)];
    if (file.entries.empty()) {
        return InvalidHandle;
    }
    const std::int32_t sound_id = 0x8000 | dgn_id;
    Handle recent = InvalidHandle;
    if (recent_instance(sound_id, recency, source, source_only, recent)) {
        return recent;
    }
    try {
        Instance instance;
        instance.handle = next_handle();
        instance.sound_id = sound_id;
        instance.source = source;
        instance.no_update = no_update;
        instance.source_only = source_only;
        instance.cancellable = cancellable;
        instance.dgn = true;
        instance.dgn_id = dgn_id;
        instance.amount_a = amount_a;
        instance.amount_b = amount_b;
        const std::size_t count = std::min<std::size_t>(file.entries.size(), 3);
        for (std::size_t index = 0; index < count; ++index) {
            const auto& entry = file.entries[index];
            Channel channel;
            if (!create_channel(channel, entry.sfx_id & 0x3FFF, 1.0F,
                                1.0F, loop, std::nullopt, source, index)) {
                stop_instance(instance);
                return InvalidHandle;
            }
            try {
                instance.channels.push_back(channel);
                channel.mixer_source = 0;
            } catch (...) {
                stop_channel(channel);
                throw;
            }
        }
        for (const Channel& channel : instance.channels) {
            refresh_channel(channel, instance);
        }
        const Handle result = instance.handle;
        try {
            instances_.emplace(result, std::move(instance));
        } catch (...) {
            stop_instance(instance);
            throw;
        }
        return result;
    } catch (...) {
        return InvalidHandle;
    }
}

SfxRuntime::Handle SfxRuntime::play_encoded(
    std::int32_t id, SoundSource* source, std::optional<bool> loop,
    bool no_update, float recency, bool source_only, bool cancellable,
    float amount_a, float amount_b) {
    if (id < 0) {
        return InvalidHandle;
    }
    if ((id & 0x8000) != 0) {
        return play_dgn(id, source, loop.value_or(false), no_update, recency,
                        source_only, cancellable, amount_a, amount_b);
    }
    if ((id & 0x4000) != 0) {
        return play_script(id, source, no_update, recency, source_only,
                           cancellable);
    }
    return play_sample(id, source, loop, no_update, recency, source_only,
                       cancellable);
}

void SfxRuntime::update(float seconds) noexcept {
    if (!loaded_) {
        return;
    }
    const float delta = std::isfinite(seconds) ? std::max(0.0F, seconds)
                                               : 0.0F;
    for (auto instance = instances_.begin(); instance != instances_.end();) {
        Instance& value = instance->second;
        if (!value.paused) {
            value.play_time += delta;
            if (value.script) {
                update_script(value);
            }
            for (const Channel& channel : value.channels) {
                refresh_channel(channel, value);
            }
            remove_finished_channels(value);
        }
        const bool script_done = value.script
            && value.script_index >= catalog_.sfx_script_files[
                static_cast<std::size_t>(value.sound_id & 0x3FFF)].entries.size()
            && value.channels.empty();
        if (value.channels.empty() || script_done) {
            stop_instance(value);
            instance = instances_.erase(instance);
        } else {
            ++instance;
        }
    }
}

bool SfxRuntime::pump() noexcept {
    return loaded_ && mixer_.pump();
}

void SfxRuntime::stop_handle(Handle handle) noexcept {
    const auto found = instances_.find(handle);
    if (found != instances_.end()) {
        stop_instance(found->second);
        instances_.erase(found);
    }
}

void SfxRuntime::stop_sound_by_id(std::int32_t id) noexcept {
    for (auto instance = instances_.begin(); instance != instances_.end();) {
        if (instance->second.sound_id == id) {
            stop_instance(instance->second);
            instance = instances_.erase(instance);
        } else {
            ++instance;
        }
    }
}

void SfxRuntime::stop_source(SoundSource* source, bool force) noexcept {
    for (auto instance = instances_.begin(); instance != instances_.end();) {
        const auto& value = instance->second;
        bool looping = false;
        for (const Channel& channel : value.channels) {
            looping = looping || channel.looping;
        }
        if (value.source == source
            && (force || looping || value.cancellable)
            && (!force || !value.no_update)) {
            stop_instance(instance->second);
            instance = instances_.erase(instance);
        } else {
            ++instance;
        }
    }
}

void SfxRuntime::stop_sound_from_source(SoundSource* source,
                                        std::int32_t id) noexcept {
    for (auto instance = instances_.begin(); instance != instances_.end();) {
        if (instance->second.source == source
            && instance->second.sound_id == id) {
            stop_instance(instance->second);
            instance = instances_.erase(instance);
        } else {
            ++instance;
        }
    }
}

void SfxRuntime::stop_scripts() noexcept {
    for (auto instance = instances_.begin(); instance != instances_.end();) {
        if (instance->second.script) {
            stop_instance(instance->second);
            instance = instances_.erase(instance);
        } else {
            ++instance;
        }
    }
}

void SfxRuntime::set_scripts_paused(bool paused) noexcept {
    for (auto& [unused, instance] : instances_) {
        (void)unused;
        if (instance.script) {
            instance.paused = paused;
        }
    }
}

void SfxRuntime::stop_all() noexcept {
    for (auto& [unused, instance] : instances_) {
        (void)unused;
        stop_instance(instance);
    }
    instances_.clear();
}

bool SfxRuntime::is_handle_playing(Handle handle) const noexcept {
    return handle != InvalidHandle && instances_.contains(handle);
}

std::size_t SfxRuntime::count_playing(std::int32_t id) const noexcept {
    std::size_t count = 0;
    for (const auto& [unused, instance] : instances_) {
        (void)unused;
        if (instance.sound_id == id) {
            ++count;
        }
    }
    return count;
}

std::size_t SfxRuntime::count_source_playing(
    std::int32_t id, const SoundSource* source) const noexcept {
    std::size_t count = 0;
    for (const auto& [unused, instance] : instances_) {
        (void)unused;
        if (instance.sound_id == id && instance.source == source) {
            ++count;
        }
    }
    return count;
}

} // namespace fruityprime::sound
