#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace fruityprime::movie {

struct SeekTableEntry {
    std::int32_t frame_id = 0;
    std::int32_t frame_offset = 0;
};

struct AudioExtradata {
    // The VX stream stores three 64-entry, eight-coefficient LPC codebooks.
    std::array<std::array<std::array<std::int16_t, 8>, 64>, 3>
        lpc_codebooks{};
    std::array<std::uint16_t, 8> scale_modifiers{};
    std::array<std::int32_t, 8> lpc_base{};
    std::int32_t scale_initial = 0;
};

struct Header {
    std::array<char, 4> magic{};
    std::int32_t frame_count = 0;
    std::int32_t frame_width = 0;
    std::int32_t frame_height = 0;
    float frame_rate = 0.0F;
    std::int32_t quantizer = 0;
    std::int32_t audio_sample_rate = 0;
    std::int32_t audio_stream_count = 0;
    // This is the maximum swapped video/audio payload, excluding the two
    // bytes used by the per-frame audio-frame count, matching Movie.cs.
    std::int32_t max_data_size = 0;
    std::int32_t extradata_offset = 0;
    std::int32_t seek_table_offset = 0;
    std::int32_t seek_table_count = 0;
};

class BitReader {
public:
    explicit BitReader(std::span<const std::uint8_t> bytes) noexcept;

    [[nodiscard]] std::size_t bit_position() const noexcept {
        return bit_position_;
    }
    [[nodiscard]] std::size_t bit_size() const noexcept {
        return bytes_.size() * 8U;
    }

    [[nodiscard]] std::uint32_t read_bits(unsigned count);
    [[nodiscard]] std::uint32_t read_bit();
    [[nodiscard]] std::uint32_t read_unsigned_exp_golomb();
    [[nodiscard]] std::int32_t read_signed_exp_golomb();

    // BitStreamReader.ConsumeUntilNotZero: how many zero bits preceded the
    // next one bit, which the exp-Golomb codes are built on.  It reads past
    // the end of a truncated stream in the managed code too, so callers only
    // use it where a one bit is guaranteed.
    [[nodiscard]] int consume_until_not_zero();

    // BitStreamReader.ReadInt: `count` bits, most significant first.
    [[nodiscard]] std::int32_t read_int(int count);

    // BitStreamReader.EnsureWordAlignment
    void align_word();

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t bit_position_ = 0;
};

struct Rgb {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

struct VideoFrame {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> y;
    std::vector<std::uint8_t> u;
    std::vector<std::uint8_t> v;

    [[nodiscard]] std::vector<std::uint8_t> rgb() const;
    [[nodiscard]] Rgb pixel_rgb(int x, int y_coordinate) const;
};

struct AudioFrame {
    std::array<std::int16_t, 128> samples{};
    std::int32_t scale = 0;
};

struct Frame {
    VideoFrame video;
    std::vector<AudioFrame> audio;
};

// Decoder for the VXDS movie format used by Metroid Prime Hunters.  It is a
// value type on purpose: callers can decode a cartridge entry without any
// process-global decoder state, while the host can still keep two instances
// for the DS top and bottom screens.
class VxDecoder {
public:
    [[nodiscard]] static VxDecoder read_file(const std::filesystem::path& path);
    [[nodiscard]] static VxDecoder from_bytes(std::vector<std::uint8_t> bytes);

    VxDecoder() = default;

    void decode();
    void reset();

    [[nodiscard]] const Header& header() const noexcept { return header_; }
    [[nodiscard]] const AudioExtradata& extradata() const noexcept {
        return extradata_;
    }
    [[nodiscard]] const std::vector<SeekTableEntry>& seek_table() const noexcept {
        return seek_table_;
    }
    [[nodiscard]] const std::array<int, 3>& quantizer_table() const noexcept {
        return quantizer_table_;
    }
    [[nodiscard]] const std::vector<Frame>& frames() const noexcept {
        return frames_;
    }
    [[nodiscard]] std::size_t frame_count() const noexcept {
        return frames_.size();
    }
    [[nodiscard]] std::size_t audio_frame_total() const noexcept {
        return audio_frame_total_;
    }
    [[nodiscard]] std::vector<std::uint8_t> image_rgb(std::size_t frame_index) const;
    [[nodiscard]] std::vector<std::int16_t> audio_samples() const;

    // VxDecoder.SampleBufferCount: the managed decoder recycles this many
    // 128-sample buffers, because up to five queued video frames can each
    // carry twelve audio frames that have not been played yet.  Nothing here
    // recycles -- every frame is decoded up front and kept -- but the number
    // is what a caller streaming audio should stay within if it wants to
    // behave like the retail decoder.
    static constexpr std::size_t SampleBufferCount = 5 * 12;

    // VxDecoder.GetAudioBuffer: the 128 samples of one audio frame, counted
    // across the whole movie rather than within a video frame.  An index past
    // the end returns an empty span rather than throwing, because a caller
    // draining the stream reaches the end by asking.
    [[nodiscard]] std::span<const std::int16_t> audio_buffer(
        std::size_t index) const noexcept;

private:
    explicit VxDecoder(std::vector<std::uint8_t> bytes);

    void parse_header();

    std::vector<std::uint8_t> bytes_;
    Header header_{};
    AudioExtradata extradata_{};
    std::vector<SeekTableEntry> seek_table_;
    std::array<int, 3> quantizer_table_{};
    std::vector<Frame> frames_;
    std::size_t audio_frame_total_ = 0;
    bool decoded_ = false;
};

enum class PlaybackState : std::uint8_t {
    Stopped,
    Playing,
    Paused,
    Finished,
};

// Scene-level counterpart of the managed movie player.  It deliberately
// keeps presentation separate from the VX decoder: a window, an exporter, or
// a future Android surface can all advance the same decoded timeline.
class Player final {
public:
    [[nodiscard]] static Player read_file(const std::filesystem::path& path);
    [[nodiscard]] static Player from_bytes(std::vector<std::uint8_t> bytes);

    explicit Player(VxDecoder decoder) : decoder_(std::move(decoder)) {}

    void play() noexcept;
    void pause() noexcept;
    void stop() noexcept;
    void reset() noexcept;
    void seek(std::size_t frame) noexcept;
    void update(double seconds) noexcept;

    [[nodiscard]] PlaybackState state() const noexcept { return state_; }
    [[nodiscard]] std::size_t frame_index() const noexcept {
        return frame_index_;
    }
    [[nodiscard]] std::size_t frame_count() const noexcept {
        return decoder_.frame_count();
    }
    [[nodiscard]] float frame_rate() const noexcept {
        return decoder_.header().frame_rate;
    }
    [[nodiscard]] double position_seconds() const noexcept {
        return position_seconds_;
    }
    [[nodiscard]] const VxDecoder& decoder() const noexcept {
        return decoder_;
    }
    [[nodiscard]] std::vector<std::uint8_t> image_rgb() const;

private:
    VxDecoder decoder_;
    PlaybackState state_ = PlaybackState::Stopped;
    std::size_t frame_index_ = 0;
    double position_seconds_ = 0.0;
};

} // namespace fruityprime::movie

namespace MphReadNative {
namespace Movie = ::fruityprime::movie;
}

