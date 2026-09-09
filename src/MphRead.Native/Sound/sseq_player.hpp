#pragma once

#include "Sound/sdat.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fruityprime::sound {

// The managed NcsfPlay player interprets SSEQ commands on the audio thread.
// Keep the decoded note stream as a portable boundary: the game mixer and a
// file exporter can consume the same events without depending on OpenAL or
// WinMM.
struct SseqNoteEvent {
    std::uint8_t track = 0;
    std::uint64_t tick = 0;
    double time_seconds = 0.0;
    double duration_seconds = 0.0;
    std::uint8_t key = 0;
    std::uint8_t velocity = 0;
    std::uint32_t duration_ticks = 0;
    std::uint16_t program = 0;
    std::int16_t pan = 0;
    std::uint8_t volume = 127;
    std::uint8_t expression = 127;
    std::uint8_t master_volume = 127;
    std::int16_t pitch_bend = 0;
    std::uint8_t bend_range = 2;
    std::uint8_t attack_rate = 255;
    std::uint8_t decay_rate = 255;
    std::uint8_t sustain_level = 255;
    std::uint8_t release_rate = 255;
};

struct SseqTimelineStats {
    std::uint64_t ticks = 0;
    std::uint64_t commands = 0;
    std::uint64_t notes = 0;
    std::uint16_t final_tempo = 120;
    bool ended = false;
    bool truncated = false;
};

struct SseqTimeline {
    std::vector<SseqNoteEvent> notes;
    SseqTimelineStats stats;
};

struct SseqRenderOptions {
    std::uint32_t sample_rate = 32728;
    float gain = 1.0F;
    std::size_t max_ticks = 65536;
    std::size_t max_events = 100000;
    // The DS player can start and fade individual logical tracks.  The
    // portable renderer uses this mask for the initial voice set; a live
    // host can rebuild a short buffer when a track set changes.
    std::uint16_t track_mask = 0xffff;
};

class SseqPlayer final {
public:
    // Resolves the sequence's bank and its four wave-archive slots directly
    // from SDAT INFO/FAT records. Empty bank/archive slots remain empty so a
    // malformed reference produces silence instead of indexing another asset.
    SseqPlayer(const Sdat& sdat, std::size_t sequence_index);

    SseqPlayer(Sseq sequence, Sbnk bank,
               std::vector<Swar> wave_archives = {});

    [[nodiscard]] const Sseq& sequence() const noexcept { return sequence_; }
    [[nodiscard]] const Sbnk& bank() const noexcept { return bank_; }

    // Run the native equivalent of Player.SequenceMain/Track.StepTicks and
    // return every PCM note event up to max_ticks.  This includes OpenTrack,
    // Goto, Call/Return, LoopStart/LoopEnd, variables, tempo and NoteWait.
    [[nodiscard]] SseqTimeline timeline(
        std::size_t max_ticks = 65536,
        std::size_t max_events = 100000) const;

    [[nodiscard]] std::vector<SseqNoteEvent> collect_notes(
        std::size_t max_ticks = 65536,
        std::size_t max_events = 100000) const;

    // Render PCM instruments from the decoded SDAT bank. PSG/noise records
    // are synthesized as deterministic square/noise voices. The result is
    // interleaved stereo float samples in [-1, 1]; platform output is left to
    // SoftwareMixer/Win32AudioOutput.
    [[nodiscard]] std::vector<float> render(
        double seconds, const SseqRenderOptions& options = {}) const;

private:
    Sseq sequence_;
    Sbnk bank_;
    std::vector<Swar> wave_archives_;
};

} // namespace fruityprime::sound

namespace MphReadNative::Sound {

using SseqNoteEvent = ::fruityprime::sound::SseqNoteEvent;
using SseqTimelineStats = ::fruityprime::sound::SseqTimelineStats;
using SseqTimeline = ::fruityprime::sound::SseqTimeline;
using SseqRenderOptions = ::fruityprime::sound::SseqRenderOptions;
using SseqPlayer = ::fruityprime::sound::SseqPlayer;

} // namespace MphReadNative::Sound
