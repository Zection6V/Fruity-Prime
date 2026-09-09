#pragma once

#include <cstdint>
#include <string_view>
#include <algorithm>
#include <span>
#include <string>
#include <vector>

namespace fruityprime::sound {

enum class WaveFormat : std::int8_t {
    None = -1,
    Pcm8 = 0,
    Pcm16 = 1,
    Adpcm = 2,
};

// One entry from SNDSAMPLES.DAT or WFSSNDSAMPLES.DAT.  Empty table slots are
// retained as `present == false`, matching the IDs used by the game's sound
// tables and SFX scripts.
struct Sample {
    bool present = false;
    std::uint32_t id = 0;
    std::uint32_t offset = 0;
    WaveFormat format = WaveFormat::None;
    bool loop = false;
    std::uint16_t sample_rate = 0;
    std::uint16_t timer = 0;
    std::uint32_t sample_start = 0;
    std::uint32_t sample_length = 0;
    std::uint32_t loop_start = 0;
    std::uint32_t loop_length = 0;
    std::vector<std::uint8_t> encoded;

    // Returns signed 16-bit PCM samples.  PCM8 and ADPCM use the same sample
    // counts and loop conversion as the managed SoundRead implementation.
    [[nodiscard]] std::vector<std::int16_t> decode_pcm(
        bool adpcm_rounding_error = false) const;

    // SoundSample.GetIntro / GetLoop / GetOutro: a looping sample is three
    // spans of one buffer -- what plays once before the loop, the loop
    // itself, and what plays after it is released.  ADPCM packs two samples
    // per byte, so the loop points are scaled by two for it and not for PCM.
    [[nodiscard]] std::size_t loop_factor() const noexcept {
        return format == WaveFormat::Adpcm ? 2u : 1u;
    }

    [[nodiscard]] std::span<const std::uint8_t> intro() const noexcept {
        if (loop_start == 0) {
            return {};
        }
        const std::size_t length =
            std::min<std::size_t>(loop_start * loop_factor(), encoded.size());
        return std::span<const std::uint8_t>(encoded.data(), length);
    }

    [[nodiscard]] std::span<const std::uint8_t> loop_span() const noexcept {
        const std::size_t factor = loop_factor();
        const std::size_t start =
            std::min<std::size_t>(loop_start * factor, encoded.size());
        const std::size_t length = std::min<std::size_t>(
            loop_length * factor, encoded.size() - start);
        return std::span<const std::uint8_t>(encoded.data() + start, length);
    }

    [[nodiscard]] std::span<const std::uint8_t> outro() const noexcept {
        const std::size_t start = (loop_start + loop_length) * loop_factor();
        if (start >= encoded.size()) {
            return {};
        }
        return std::span<const std::uint8_t>(encoded.data() + start,
                                             encoded.size() - start);
    }
};

// SoundRead.ReadSoundSamples / ReadWfsSoundSamples.  The two tables differ
// only in the file they come from: SNDSAMPLES is padded to 512-byte
// multiples and WFSSNDSAMPLES is not, which the parser does not care about.
inline constexpr std::string_view SoundSamplesFile = "SNDSAMPLES.DAT";
inline constexpr std::string_view WfsSoundSamplesFile = "WFSSNDSAMPLES.DAT";

// SoundRead.GetStreamBufferData: interleave a stream's per-channel buffers
// into the single buffer the mixer wants.  ADPCM carries two bytes per
// sample, PCM one.
[[nodiscard]] std::vector<std::uint8_t> interleave_stream_channels(
    std::span<const std::vector<std::uint8_t>> channels, WaveFormat format);

[[nodiscard]] std::vector<Sample> parse_sample_table(
    std::span<const std::uint8_t> bytes);

// First Hunt keeps its four sound banks in a different table format than
// MPH.  The header is 24 bytes and the file starts with a count followed by
// absolute offsets.  Empty entries are retained so their IDs continue to
// line up with the original sound references.
struct FhSample {
    bool present = false;
    std::uint32_t id = 0;
    std::uint32_t offset = 0;
    std::uint32_t data_size = 0;
    std::uint32_t data_pointer = 0;
    std::uint32_t sample_rate = 0;
    std::uint16_t volume = 0;
    std::uint8_t field_e = 0;
    WaveFormat format = WaveFormat::None;
    bool loop = false;
    std::uint32_t sample_start = 0;
    std::uint32_t sample_length = 0;
    std::uint32_t loop_start = 0;
    std::uint32_t loop_length = 0;
    std::vector<std::uint8_t> encoded;

    [[nodiscard]] std::vector<std::int16_t> decode_pcm(
        bool adpcm_rounding_error = false) const;
};

[[nodiscard]] std::vector<FhSample> parse_fh_sound_file(
    std::span<const std::uint8_t> bytes);

struct Stream {
    std::uint32_t id = 0;
    std::string name;
    WaveFormat format = WaveFormat::None;
    bool loop = false;
    std::uint8_t channel_count = 0;
    std::uint16_t sample_rate = 0;
    std::uint16_t timer = 0;
    std::uint32_t loop_start = 0;
    std::uint32_t loop_end = 0;
    float volume = 1.0F;
    std::vector<std::vector<std::uint8_t>> channels;

    [[nodiscard]] static Stream parse(std::span<const std::uint8_t> bytes,
                                      std::uint32_t id = 0,
                                      std::string name = {});

    // Interleaves the channel buffers in the byte layout consumed by a
    // conventional PCM audio device.  ADPCM channels have already been
    // converted to little-endian signed 16-bit samples by parse().
    [[nodiscard]] std::vector<std::uint8_t> interleaved_data() const;
};

} // namespace fruityprime::sound
