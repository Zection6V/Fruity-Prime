#include "Sound/sseq_player.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace fruityprime::sound {
namespace {

constexpr double Arm7Clock = 33'514'000.0;
constexpr double SequenceClockCycles = 64.0 * 2728.0;
constexpr double TimerRate = 240.0;
constexpr std::size_t TrackCount = 16;
constexpr std::size_t MaxCallDepth = 3;

enum class ValueType : std::uint8_t {
    U8,
    U16,
    Vlv,
    Variable,
    Random,
};

enum class FrameType : std::uint8_t {
    Call,
    Loop,
};

struct Frame {
    FrameType type = FrameType::Call;
    std::size_t position = 0;
    std::uint8_t count = 0;
};

struct TrackState {
    bool allocated = false;
    bool active = false;
    std::size_t position = 0;
    std::uint32_t wait = 0;
    bool note_wait = true;
    bool compare = true;
    bool tie = false;
    std::uint16_t program = 0;
    std::uint8_t volume = 127;
    std::uint8_t expression = 127;
    std::int16_t pan = 0;
    std::uint8_t pan_range = 127;
    std::int16_t pitch_bend = 0;
    std::uint8_t bend_range = 2;
    std::int16_t transpose = 0;
    std::uint8_t portamento_key = 60;
    bool portamento = false;
    std::uint8_t attack_rate = 255;
    std::uint8_t decay_rate = 255;
    std::uint8_t sustain_level = 255;
    std::uint8_t release_rate = 255;
    std::array<Frame, MaxCallDepth> frames{};
    std::size_t frame_depth = 0;
};

struct Engine {
    const std::vector<std::uint8_t>& data;
    std::array<TrackState, TrackCount> tracks{};
    std::array<std::int16_t, 32> variables{};
    std::uint16_t tempo = 120;
    std::uint8_t master_volume = 127;
    std::uint32_t random_state = 0x12345678U;
    std::uint64_t commands = 0;
    bool malformed = false;

    Engine(const std::vector<std::uint8_t>& sequence_data) : data(sequence_data) {
        variables.fill(-1);
        tracks[0].allocated = true;
        tracks[0].active = true;
        if (data.empty()) {
            tracks[0].active = false;
            malformed = true;
            return;
        }

        tracks[0].position = 0;
        if (data[0] == 0xFE) {
            tracks[0].position = 1;
            if (!read_u16(tracks[0], sequence_track_mask_)) {
                tracks[0].active = false;
                return;
            }
            const std::uint16_t logical_mask =
                static_cast<std::uint16_t>(sequence_track_mask_ >> 1);
            for (std::size_t track = 1; track < TrackCount; ++track) {
                if ((logical_mask & (1U << (track - 1))) != 0) {
                    tracks[track].allocated = true;
                }
            }
        }
    }

    std::uint16_t sequence_track_mask_ = 0;

    [[nodiscard]] bool read_u8(TrackState& track, std::uint8_t& value) {
        if (track.position >= data.size()) {
            malformed = true;
            return false;
        }
        value = data[track.position++];
        return true;
    }

    [[nodiscard]] bool read_u16(TrackState& track, std::uint16_t& value) {
        std::uint8_t low = 0;
        std::uint8_t high = 0;
        if (!read_u8(track, low) || !read_u8(track, high)) {
            return false;
        }
        value = static_cast<std::uint16_t>(low)
            | static_cast<std::uint16_t>(high) << 8;
        return true;
    }

    [[nodiscard]] bool read_u24(TrackState& track, std::uint32_t& value) {
        std::uint8_t first = 0;
        std::uint8_t second = 0;
        std::uint8_t third = 0;
        if (!read_u8(track, first) || !read_u8(track, second)
            || !read_u8(track, third)) {
            return false;
        }
        value = static_cast<std::uint32_t>(first)
            | static_cast<std::uint32_t>(second) << 8
            | static_cast<std::uint32_t>(third) << 16;
        return true;
    }

    [[nodiscard]] bool read_vlv(TrackState& track, int& value) {
        value = 0;
        for (int count = 0; count < 5; ++count) {
            std::uint8_t byte = 0;
            if (!read_u8(track, byte)) {
                return false;
            }
            value = (value << 7) | (byte & 0x7F);
            if ((byte & 0x80) == 0) {
                return true;
            }
        }
        malformed = true;
        return false;
    }

    [[nodiscard]] std::uint16_t next_random() noexcept {
        random_state = random_state * 1'664'525U + 1'013'904'223U;
        return static_cast<std::uint16_t>(random_state >> 16);
    }

    [[nodiscard]] bool parse_value(TrackState& track, ValueType type,
                                   int& value) {
        switch (type) {
        case ValueType::U8: {
            std::uint8_t result = 0;
            if (!read_u8(track, result)) {
                return false;
            }
            value = result;
            return true;
        }
        case ValueType::U16: {
            std::uint16_t result = 0;
            if (!read_u16(track, result)) {
                return false;
            }
            value = result;
            return true;
        }
        case ValueType::Vlv:
            return read_vlv(track, value);
        case ValueType::Variable: {
            std::uint8_t index = 0;
            if (!read_u8(track, index)) {
                return false;
            }
            value = variables[index < variables.size() ? index : 0];
            return true;
        }
        case ValueType::Random: {
            std::uint16_t low = 0;
            std::uint16_t high_raw = 0;
            if (!read_u16(track, low) || !read_u16(track, high_raw)) {
                return false;
            }
            const int high = static_cast<std::int16_t>(high_raw);
            int range = high - static_cast<int>(low) + 1;
            range = std::max(1, range);
            value = static_cast<int>(low)
                + ((static_cast<int>(next_random()) * range) >> 16);
            return true;
        }
        }
        malformed = true;
        return false;
    }

    void start_track(std::size_t index, std::uint32_t offset) {
        if (index >= TrackCount || !tracks[index].allocated
            || offset >= data.size()) {
            return;
        }
        auto& track = tracks[index];
        track.active = true;
        track.position = static_cast<std::size_t>(offset);
        track.wait = 0;
        track.frame_depth = 0;
    }

    void stop_track(std::size_t index) noexcept {
        if (index < TrackCount) {
            tracks[index].active = false;
        }
    }

    [[nodiscard]] static std::int16_t clamped_pan(const TrackState& track) {
        int pan = track.pan;
        if (track.pan_range != 127) {
            pan = (pan * track.pan_range + 0x40) >> 7;
        }
        return static_cast<std::int16_t>(std::clamp(pan, -128, 127));
    }

    [[nodiscard]] bool step_track(std::size_t track_index, std::uint64_t tick,
                                  double time_seconds,
                                  SseqTimeline& result, std::size_t max_events) {
        auto& track = tracks[track_index];
        if (!track.active) {
            return true;
        }
        if (track.wait > 0) {
            --track.wait;
            if (track.wait > 0) {
                return true;
            }
        }

        // Commands which do not introduce a wait can legally chain many
        // operations at the same tick. Keep malformed/infinite loops bounded.
        constexpr std::size_t CommandsPerTick = 100'000;
        std::size_t commands_this_tick = 0;
        while (track.active && track.wait == 0) {
            if (++commands_this_tick > CommandsPerTick) {
                malformed = true;
                track.active = false;
                return false;
            }
            if (++commands > std::numeric_limits<std::uint64_t>::max() - 1) {
                malformed = true;
                track.active = false;
                return false;
            }

            std::uint8_t command = 0;
            if (!read_u8(track, command)) {
                track.active = false;
                return false;
            }
            bool run_command = true;
            ValueType value_type = ValueType::Vlv;

            if (command == 0xA2) { // If
                if (!read_u8(track, command)) {
                    track.active = false;
                    return false;
                }
                run_command = track.compare;
            }
            if (command == 0xA0) { // Random modifier
                if (!read_u8(track, command)) {
                    track.active = false;
                    return false;
                }
                value_type = ValueType::Random;
            }
            if (command == 0xA1) { // Variable modifier
                if (!read_u8(track, command)) {
                    track.active = false;
                    return false;
                }
                value_type = ValueType::Variable;
            }

            if ((command & 0x80) == 0) {
                std::uint8_t velocity = 0;
                int duration = 0;
                if (!read_u8(track, velocity)
                    || !parse_value(track, value_type, duration)) {
                    track.active = false;
                    return false;
                }
                if (run_command && result.notes.size() < max_events) {
                    const int key = std::clamp(
                        static_cast<int>(command) + track.transpose, 0, 127);
                    const std::uint32_t ticks = static_cast<std::uint32_t>(
                        std::max(0, duration));
                    const double tick_seconds = seconds_per_tick(tempo);
                    result.notes.push_back(SseqNoteEvent{
                        static_cast<std::uint8_t>(track_index), tick,
                        time_seconds,
                        static_cast<double>(ticks) * tick_seconds,
                        static_cast<std::uint8_t>(key), velocity, ticks,
                        track.program, clamped_pan(track), track.volume,
                        track.expression, master_volume, track.pitch_bend,
                        track.bend_range,
                        track.attack_rate, track.decay_rate,
                        track.sustain_level, track.release_rate
                    });
                    ++result.stats.notes;
                }
                if (run_command && track.note_wait) {
                    // A zero-length note waits for its channel to finish in
                    // the managed player. The timeline has no channel object,
                    // so one tick is the safe equivalent.
                    track.wait = static_cast<std::uint32_t>(
                        duration > 0 ? duration : 1);
                }
                continue;
            }

            switch (command & 0xF0) {
            case 0x80: {
                int parameter = 0;
                if (!parse_value(track, value_type, parameter)) {
                    track.active = false;
                    return false;
                }
                if (run_command) {
                    if (command == 0x80) { // Rest
                        track.wait = static_cast<std::uint32_t>(
                            std::max(0, parameter));
                    } else if (command == 0x81) { // Patch
                        track.program = static_cast<std::uint16_t>(
                            std::clamp(parameter, 0, 0xFFFF));
                    }
                }
                break;
            }
            case 0x90: {
                if (command == 0x93) { // OpenTrack
                    std::uint8_t target = 0;
                    std::uint32_t offset = 0;
                    if (!read_u8(track, target)
                        || !read_u24(track, offset)) {
                        track.active = false;
                        return false;
                    }
                    if (run_command) {
                        if (target < TrackCount) {
                            stop_track(target);
                            start_track(target, offset);
                        }
                    }
                } else {
                    std::uint32_t offset = 0;
                    if (!read_u24(track, offset)) {
                        track.active = false;
                        return false;
                    }
                    if (run_command && offset < data.size()) {
                        if (command == 0x94) { // Goto
                            track.position = offset;
                        } else if (command == 0x95 // Call
                                   && track.frame_depth < MaxCallDepth) {
                            track.frames[track.frame_depth++] =
                                Frame{FrameType::Call, track.position, 0};
                            track.position = offset;
                        }
                    }
                }
                break;
            }
            case 0xB0: {
                std::uint8_t variable = 0;
                int parameter = 0;
                if (!read_u8(track, variable)
                    || !parse_value(track, ValueType::U16, parameter)) {
                    track.active = false;
                    return false;
                }
                if (run_command) {
                    const std::size_t index = variable < variables.size()
                        ? variable : 0;
                    auto& value = variables[index];
                    const auto operation = command;
                    switch (operation) {
                    case 0xB0: value = static_cast<std::int16_t>(parameter); break;
                    case 0xB1: value = static_cast<std::int16_t>(value + parameter); break;
                    case 0xB2: value = static_cast<std::int16_t>(value - parameter); break;
                    case 0xB3: value = static_cast<std::int16_t>(value * parameter); break;
                    case 0xB4:
                        if (parameter != 0) {
                            value = static_cast<std::int16_t>(value / parameter);
                        }
                        break;
                    case 0xB5:
                        value = parameter >= 0
                            ? static_cast<std::int16_t>(value << std::min(parameter, 15))
                            : static_cast<std::int16_t>(value >> std::min(-parameter, 15));
                        break;
                    case 0xB6: {
                        int range = parameter;
                        const bool negative = range < 0;
                        range = std::abs(range);
                        int random = (static_cast<int>(next_random())
                                      * (range + 1)) >> 16;
                        value = static_cast<std::int16_t>(negative ? -random : random);
                        break;
                    }
                    case 0xB8: track.compare = value == parameter; break;
                    case 0xB9: track.compare = value >= parameter; break;
                    case 0xBA: track.compare = value > parameter; break;
                    case 0xBB: track.compare = value <= parameter; break;
                    case 0xBC: track.compare = value < parameter; break;
                    case 0xBD: track.compare = value != parameter; break;
                    default: break;
                    }
                }
                break;
            }
            case 0xC0:
            case 0xD0: {
                int parameter = 0;
                if (!parse_value(track, ValueType::U8, parameter)) {
                    track.active = false;
                    return false;
                }
                if (!run_command) {
                    break;
                }
                switch (command) {
                case 0xC0: track.pan = static_cast<std::int16_t>(parameter - 0x40); break;
                case 0xC1: track.volume = static_cast<std::uint8_t>(parameter); break;
                case 0xC2: master_volume = static_cast<std::uint8_t>(parameter); break;
                case 0xC3: track.transpose = static_cast<std::int8_t>(parameter); break;
                case 0xC4: track.pitch_bend = static_cast<std::int8_t>(parameter); break;
                case 0xC5: track.bend_range = static_cast<std::uint8_t>(parameter); break;
                case 0xC7: track.note_wait = parameter != 0; break;
                case 0xC8: track.tie = parameter != 0; break;
                case 0xC9:
                    track.portamento_key = static_cast<std::uint8_t>(
                        std::clamp(parameter + track.transpose, 0, 127));
                    track.portamento = true;
                    break;
                case 0xCE: track.tie = parameter != 0; break;
                case 0xD0: track.attack_rate = static_cast<std::uint8_t>(parameter); break;
                case 0xD1: track.decay_rate = static_cast<std::uint8_t>(parameter); break;
                case 0xD2: track.sustain_level = static_cast<std::uint8_t>(parameter); break;
                case 0xD3: track.release_rate = static_cast<std::uint8_t>(parameter); break;
                case 0xD4: // LoopStart is handled below with its own stack.
                    if (track.frame_depth < MaxCallDepth) {
                        track.frames[track.frame_depth++] =
                            Frame{FrameType::Loop, track.position,
                                  static_cast<std::uint8_t>(parameter)};
                    }
                    break;
                case 0xD5: track.expression = static_cast<std::uint8_t>(parameter); break;
                default: break;
                }
                break;
            }
            case 0xE0: {
                int parameter = 0;
                if (!parse_value(track, ValueType::U16, parameter)) {
                    track.active = false;
                    return false;
                }
                if (run_command) {
                    if (command == 0xE1) { // Tempo
                        tempo = static_cast<std::uint16_t>(
                            std::clamp(parameter, 1, 0xFFFF));
                    }
                }
                break;
            }
            case 0xF0:
                if (run_command) {
                    if (command == 0xFD) { // Return
                        if (track.frame_depth > 0
                            && track.frames[track.frame_depth - 1].type
                                == FrameType::Call) {
                            track.position = track.frames[--track.frame_depth].position;
                        }
                    } else if (command == 0xFC) { // LoopEnd
                        if (track.frame_depth > 0
                            && track.frames[track.frame_depth - 1].type
                                == FrameType::Loop) {
                            auto& frame = track.frames[track.frame_depth - 1];
                            if (frame.count != 0 && --frame.count == 0) {
                                --track.frame_depth;
                            } else {
                                track.position = frame.position;
                            }
                        }
                    } else if (command == 0xFF) { // End
                        track.active = false;
                    }
                }
                break;
            default:
                // Known commands with a high nibble not covered above still
                // carry a byte parameter in the Nitro format. Consume it so
                // a future command does not get mistaken for the parameter.
                if (command != 0xFE) {
                    std::uint8_t ignored = 0;
                    if (!read_u8(track, ignored)) {
                        track.active = false;
                        return false;
                    }
                }
                break;
            }
        }
        return true;
    }

    [[nodiscard]] static double seconds_per_tick(std::uint16_t current_tempo) noexcept {
        const double tempo_value = std::max<std::uint16_t>(1, current_tempo);
        return (SequenceClockCycles / Arm7Clock) * TimerRate / tempo_value;
    }

    static void finish_stats(SseqTimeline& result, std::uint16_t current_tempo,
                             bool active, std::size_t max_ticks) {
        result.stats.final_tempo = current_tempo;
        result.stats.ended = !active;
        result.stats.truncated = active && result.stats.ticks >= max_ticks;
    }
};

[[nodiscard]] const Instrument* instrument_for(const Sbnk& bank,
                                               std::uint16_t program,
                                               std::uint8_t key) noexcept {
    if (program >= bank.entries.size()) {
        return nullptr;
    }
    const auto& entry = bank.entries[program];
    if (entry.instruments.empty()) {
        return nullptr;
    }
    if (entry.record >= 1 && entry.record <= 5) {
        return &entry.instruments.front();
    }
    if (entry.record == 16) {
        for (const auto& instrument : entry.instruments) {
            if (key >= instrument.low_note && key <= instrument.high_note) {
                return &instrument;
            }
        }
        return nullptr;
    }
    if (entry.record == 17) {
        for (const auto& instrument : entry.instruments) {
            if (key <= instrument.high_note) {
                return &instrument;
            }
        }
    }
    return nullptr;
}

[[nodiscard]] float normalized_sample(std::int16_t value) noexcept {
    return static_cast<float>(value) / 32768.0F;
}

[[nodiscard]] float envelope_gain(const SseqNoteEvent& note, double elapsed,
                                  double duration) noexcept {
    const double safe_duration = std::max(0.001, duration);
    const double attack = note.attack_rate == 255
        ? 0.001 : std::max(0.001, (255.0 - note.attack_rate) / 255.0 * 0.08);
    const double release = note.release_rate == 255
        ? 0.008 : std::max(0.004, (255.0 - note.release_rate) / 255.0 * 0.18);
    double gain = elapsed < attack ? elapsed / attack : 1.0;
    if (elapsed > safe_duration - release) {
        gain *= std::clamp((safe_duration - elapsed) / release, 0.0, 1.0);
    }
    if (note.sustain_level != 255) {
        const double sustain = std::clamp(note.sustain_level / 127.0, 0.0, 1.0);
        if (elapsed > attack) {
            gain *= sustain + (1.0 - sustain)
                * std::clamp((safe_duration - elapsed) / release, 0.0, 1.0);
        }
    }
    return static_cast<float>(std::clamp(gain, 0.0, 1.0));
}

} // namespace

SseqPlayer::SseqPlayer(const Sdat& sdat, std::size_t sequence_index)
    : sequence_(sdat.sequence(sequence_index)), bank_(), wave_archives_() {
    if (sequence_index >= sdat.sequences().size()) {
        throw std::out_of_range("SSEQ sequence index is outside SDAT INFO");
    }
    const auto& sequence_info = sdat.sequences()[sequence_index];
    if (sequence_info.bank >= sdat.banks().size()
        || !sdat.banks()[sequence_info.bank].present) {
        throw std::runtime_error("SSEQ has no usable SBNK reference");
    }
    bank_ = sdat.bank(sequence_info.bank);
    wave_archives_.resize(4);
    const auto& bank_info = sdat.banks()[sequence_info.bank];
    for (std::size_t slot = 0; slot < bank_info.wave_archives.size(); ++slot) {
        const auto archive_id = bank_info.wave_archives[slot];
        if (archive_id == 0xFFFF || archive_id >= sdat.wave_archives().size()
            || !sdat.wave_archives()[archive_id].present) {
            continue;
        }
        wave_archives_[slot] = sdat.wave_archive(archive_id);
    }
}

SseqPlayer::SseqPlayer(Sseq sequence, Sbnk bank,
                       std::vector<Swar> wave_archives)
    : sequence_(std::move(sequence)), bank_(std::move(bank)),
      wave_archives_(std::move(wave_archives)) {}

SseqTimeline SseqPlayer::timeline(std::size_t max_ticks,
                                  std::size_t max_events) const {
    SseqTimeline result;
    if (max_ticks == 0 || max_events == 0) {
        result.stats.truncated = max_ticks == 0;
        return result;
    }

    Engine engine(sequence_.data);
    double time_seconds = 0.0;
    bool active = true;
    for (std::size_t tick = 0; tick < max_ticks; ++tick) {
        active = false;
        for (const auto& track : engine.tracks) {
            active = active || track.active;
        }
        if (!active) {
            break;
        }
        for (std::size_t track = 0; track < TrackCount; ++track) {
            if (!engine.step_track(track, tick, time_seconds, result,
                                   max_events)) {
                // A malformed track is stopped, while other valid tracks may
                // still produce useful notes from the same sequence.
                continue;
            }
        }
        ++result.stats.ticks;
        active = false;
        for (const auto& track : engine.tracks) {
            active = active || track.active;
        }
        if (!active) {
            break;
        }
        time_seconds += Engine::seconds_per_tick(engine.tempo);
    }
    Engine::finish_stats(result, engine.tempo, active, max_ticks);
    if (engine.malformed) {
        result.stats.truncated = true;
    }
    result.stats.commands = engine.commands;
    return result;
}

std::vector<SseqNoteEvent> SseqPlayer::collect_notes(std::size_t max_ticks,
                                                     std::size_t max_events) const {
    return timeline(max_ticks, max_events).notes;
}

std::vector<float> SseqPlayer::render(double seconds,
                                      const SseqRenderOptions& options) const {
    if (!(seconds > 0.0) || !std::isfinite(seconds)
        || options.sample_rate == 0 || !std::isfinite(options.gain)) {
        return {};
    }
    const auto frame_count = static_cast<std::size_t>(std::ceil(
        seconds * options.sample_rate));
    if (frame_count > std::numeric_limits<std::size_t>::max() / 2) {
        throw std::length_error("SSEQ render buffer is too large");
    }
    std::vector<float> output(frame_count * 2, 0.0F);
    const auto decoded = timeline(options.max_ticks, options.max_events);

    struct CachedWave {
        std::size_t archive = 0;
        std::size_t wave = 0;
        std::vector<std::int16_t> samples;
    };
    std::vector<CachedWave> wave_cache;
    const auto get_wave = [&](std::size_t archive, std::size_t wave)
        -> const std::vector<std::int16_t>* {
        for (const auto& cached : wave_cache) {
            if (cached.archive == archive && cached.wave == wave) {
                return &cached.samples;
            }
        }
        if (archive >= wave_archives_.size()
            || wave >= wave_archives_[archive].waves.size()
            || !wave_archives_[archive].waves[wave].has_value()) {
            return static_cast<const std::vector<std::int16_t>*>(nullptr);
        }
        wave_cache.push_back(CachedWave{
            archive, wave, wave_archives_[archive].waves[wave]->decode_pcm()
        });
        return &wave_cache.back().samples;
    };

    for (const auto& note : decoded.notes) {
        if (note.track >= 16
            || (options.track_mask
                & static_cast<std::uint16_t>(1U << note.track)) == 0) {
            continue;
        }
        if (note.time_seconds >= seconds) {
            continue;
        }
        const auto* instrument = instrument_for(bank_, note.program, note.key);
        if (instrument == nullptr) {
            continue;
        }
        const double note_end = std::min(
            seconds, note.time_seconds + std::max(0.001, note.duration_seconds));
        const auto first_frame = static_cast<std::size_t>(std::max(
            0.0, std::floor(note.time_seconds * options.sample_rate)));
        const auto last_frame = static_cast<std::size_t>(std::min<double>(
            frame_count, std::ceil(note_end * options.sample_rate)));
        if (first_frame >= last_frame || first_frame >= frame_count) {
            continue;
        }
        const double base_gain = std::clamp(
            note.velocity / 127.0 * note.volume / 127.0
                * note.expression / 127.0 * note.master_volume / 127.0
                * options.gain,
            0.0, 4.0);
        const int pan = std::clamp(static_cast<int>(note.pan)
                                       + static_cast<int>(instrument->pan) - 0x40,
                                   -128, 127);
        const float left_gain = static_cast<float>((127 - pan) / 254.0);
        const float right_gain = static_cast<float>((127 + pan) / 254.0);
        const double semitone = (static_cast<int>(note.key)
                                 - static_cast<int>(instrument->note_number))
            + static_cast<double>(note.pitch_bend) * note.bend_range / 8192.0;
        const double pitch = std::pow(2.0, semitone / 12.0);
        const bool pcm = instrument->record == 1;
        const std::vector<std::int16_t>* samples = nullptr;
        const Swav* wave = nullptr;
        if (pcm) {
            const std::size_t archive = instrument->swar;
            const std::size_t wave_index = instrument->swav;
            if (archive < wave_archives_.size()
                && wave_index < wave_archives_[archive].waves.size()
                && wave_archives_[archive].waves[wave_index].has_value()) {
                wave = &*wave_archives_[archive].waves[wave_index];
                samples = get_wave(archive, wave_index);
            }
        }
        std::uint32_t noise = 0xACE1U
            ^ (static_cast<std::uint32_t>(note.key) << 8)
            ^ note.track;
        const double source_rate = wave != nullptr && wave->sample_rate != 0
            ? wave->sample_rate : 32728.0;
        const double source_step = source_rate / options.sample_rate * pitch;
        for (std::size_t frame = first_frame; frame < last_frame; ++frame) {
            const double elapsed = static_cast<double>(frame)
                / options.sample_rate - note.time_seconds;
            float sample = 0.0F;
            if (samples != nullptr && !samples->empty()) {
                double position = elapsed * source_step;
                const double loop_start = wave->loop_offset;
                const double loop_length = wave->loop_length;
                if (wave->loop != 0 && loop_length > 0.0
                    && position >= loop_start) {
                    position = loop_start + std::fmod(position - loop_start,
                                                      loop_length);
                }
                if (position < samples->size()) {
                    const auto index = static_cast<std::size_t>(position);
                    const auto next = std::min(index + 1, samples->size() - 1);
                    const float fraction = static_cast<float>(position - index);
                    sample = normalized_sample((*samples)[index])
                        + (normalized_sample((*samples)[next])
                           - normalized_sample((*samples)[index])) * fraction;
                }
            } else if (instrument->record == 2) {
                const double frequency = 440.0 * std::pow(
                    2.0, (static_cast<int>(note.key) - 69) / 12.0);
                const double phase = std::fmod(elapsed * frequency, 1.0);
                const double duty = std::clamp(
                    (instrument->swav & 7U) / 8.0, 0.125, 0.875);
                sample = phase < duty ? 0.65F : -0.65F;
            } else if (instrument->record == 3) {
                noise ^= noise << 13;
                noise ^= noise >> 17;
                noise ^= noise << 5;
                sample = static_cast<float>(
                    static_cast<std::int32_t>(noise & 0xFFFFU) - 32768)
                    / 32768.0F * 0.55F;
            }
            const float gain = static_cast<float>(base_gain)
                * envelope_gain(note, elapsed,
                                 std::max(0.001, note_end - note.time_seconds));
            output[frame * 2] += sample * gain * left_gain;
            output[frame * 2 + 1] += sample * gain * right_gain;
        }
    }
    for (auto& sample : output) {
        sample = std::clamp(sample, -1.0F, 1.0F);
    }
    return output;
}

} // namespace fruityprime::sound
