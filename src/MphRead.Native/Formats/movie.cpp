#include "Formats/movie_layouts.hpp"
#include "Formats/movie.hpp"

#include "Utility/binary_reader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace fruityprime::movie {
namespace {

[[nodiscard]] std::vector<std::uint8_t> read_all(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open VX movie " + path.string());
    }
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<std::uintmax_t>(length)
        > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("VX movie is too large: " + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read VX movie " + path.string());
        }
    }
    return bytes;
}

[[nodiscard]] std::uint8_t clamp_byte(int value) noexcept {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

struct VlcCode {
    std::uint8_t length = 0;
    std::uint16_t bits = 0;
};

class VlcTable {
public:
    VlcTable() = default;

    VlcTable(std::initializer_list<int> lengths,
             std::initializer_list<int> bits) {
        if (lengths.size() != bits.size()) {
            throw std::logic_error("VX VLC table length mismatch");
        }
        auto length = lengths.begin();
        auto bit = bits.begin();
        for (; length != lengths.end(); ++length, ++bit) {
            if (*length < 0 || *length > 16 || *bit < 0
                || *bit >= (1 << std::max(*length, 1))) {
                throw std::logic_error("invalid VX VLC entry");
            }
            entries_.push_back(VlcCode{
                static_cast<std::uint8_t>(*length),
                static_cast<std::uint16_t>(*bit)
            });
            max_bits_ = std::max(max_bits_, *length);
        }
    }

    // VLCData.FindBitPattern: the index of the code that is exactly these
    // bits at exactly this length, or -1.  Length matters as well as value --
    // a two-bit 01 and a three-bit 001 are different codes.  The managed side
    // hashes the bits it has read so far and looks that hash up; comparing the
    // pair directly is the same mapping without the hash.
    [[nodiscard]] int find_bit_pattern(int length,
                                       std::uint32_t bits) const noexcept {
        for (std::size_t index = 0; index < entries_.size(); ++index) {
            const VlcCode& code = entries_[index];
            if (code.length == length && code.bits == bits) {
                return static_cast<int>(index);
            }
        }
        return -1;
    }

    // BitStreamReader.ReadVLC2: read one bit at a time until the bits read so
    // far are a whole code.  The codes are prefix-free, so the first match is
    // the only match.
    [[nodiscard]] int read_vlc2(BitReader& reader) const {
        std::uint32_t prefix = 0;
        for (int length = 1; length <= max_bits_; ++length) {
            prefix = (prefix << 1U) | reader.read_bit();
            const int index = find_bit_pattern(length, prefix);
            if (index != -1) {
                return index;
            }
        }
        throw std::runtime_error("VX decoding error 016: invalid VLC code");
    }

    // VLCData.MaxBitCount
    [[nodiscard]] int max_bit_count() const noexcept { return max_bits_; }

private:
    std::vector<VlcCode> entries_;
    int max_bits_ = 0;
};

[[nodiscard]] const std::array<VlcTable, 4>& coeff_token_vlc() {
    static const std::array<VlcTable, 4> tables{
        VlcTable{
            {1, 0, 0, 0, 6, 2, 0, 0, 8, 6, 3, 0, 9, 8, 7, 5,
             10, 9, 8, 6, 11, 10, 9, 7, 13, 11, 10, 8, 13, 13, 11, 9,
             13, 13, 13, 10, 14, 14, 13, 11, 14, 14, 14, 13, 15, 15, 14, 14,
             15, 15, 15, 14, 16, 15, 15, 15, 16, 16, 16, 15, 16, 16, 16, 16,
             16, 16, 16, 16},
            {1, 0, 0, 0, 5, 1, 0, 0, 7, 4, 1, 0, 7, 6, 5, 3,
             7, 6, 5, 3, 7, 6, 5, 4, 15, 6, 5, 4, 11, 14, 5, 4,
             8, 10, 13, 4, 15, 14, 9, 4, 11, 10, 13, 12, 15, 14, 9, 12,
             11, 10, 13, 8, 15, 1, 9, 12, 11, 14, 13, 8, 7, 10, 9, 12,
             4, 6, 5, 8}
        },
        VlcTable{
            {2, 0, 0, 0, 6, 2, 0, 0, 6, 5, 3, 0, 7, 6, 6, 4,
             8, 6, 6, 4, 8, 7, 7, 5, 9, 8, 8, 6, 11, 9, 9, 6,
             11, 11, 11, 7, 12, 11, 11, 9, 12, 12, 12, 11, 12, 12, 12, 11,
             13, 13, 13, 12, 13, 13, 13, 13, 13, 14, 13, 13, 14, 14, 14, 13,
             14, 14, 14, 14},
            {3, 0, 0, 0, 11, 2, 0, 0, 7, 7, 3, 0, 7, 10, 9, 5,
             7, 6, 5, 4, 4, 6, 5, 6, 7, 6, 5, 8, 15, 6, 5, 4,
             11, 14, 13, 4, 15, 10, 9, 4, 11, 14, 13, 12, 8, 10, 9, 8,
             15, 14, 13, 12, 11, 10, 9, 12, 7, 11, 6, 8, 9, 8, 10, 1,
             7, 6, 5, 4}
        },
        VlcTable{
            {4, 0, 0, 0, 6, 4, 0, 0, 6, 5, 4, 0, 6, 5, 5, 4,
             7, 5, 5, 4, 7, 5, 5, 4, 7, 6, 6, 4, 7, 6, 6, 4,
             8, 7, 7, 5, 8, 8, 7, 6, 9, 8, 8, 7, 9, 9, 8, 8,
             9, 9, 9, 8, 10, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10,
             10, 10, 10, 10},
            {15, 0, 0, 0, 15, 14, 0, 0, 11, 15, 13, 0, 8, 12, 14, 12,
             15, 10, 11, 11, 11, 8, 9, 10, 9, 14, 13, 9, 8, 10, 9, 8,
             15, 14, 13, 13, 11, 14, 10, 12, 15, 10, 13, 12, 11, 14, 9, 12,
             8, 10, 13, 8, 13, 7, 9, 12, 9, 12, 11, 10, 5, 8, 7, 6,
             1, 4, 3, 2}
        },
        VlcTable{
            {6, 0, 0, 0, 6, 6, 0, 0, 6, 6, 6, 0, 6, 6, 6, 6,
             6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
             6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
             6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
             6, 6, 6, 6},
            {3, 0, 0, 0, 0, 1, 0, 0, 4, 5, 6, 0, 8, 9, 10, 11,
             12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
             28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
             44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
             60, 61, 62, 63}
        }
    };
    return tables;
}

[[nodiscard]] const std::vector<VlcTable>& total_zeroes_vlc() {
    static const std::vector<VlcTable> tables{
        VlcTable{},
        VlcTable{{1, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9},
                 {1, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 1}},
        VlcTable{{3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 6, 6, 6, 6},
                 {7, 6, 5, 4, 3, 5, 4, 3, 2, 3, 2, 3, 2, 1, 0}},
        VlcTable{{4, 3, 3, 3, 4, 4, 3, 3, 4, 5, 5, 6, 5, 6},
                 {5, 7, 6, 5, 4, 3, 4, 3, 2, 3, 2, 1, 1, 0}},
        VlcTable{{5, 3, 4, 4, 3, 3, 3, 4, 3, 4, 5, 5, 5},
                 {3, 7, 5, 4, 6, 5, 4, 3, 3, 2, 2, 1, 0}},
        VlcTable{{4, 4, 4, 3, 3, 3, 3, 3, 4, 5, 4, 5},
                 {5, 4, 3, 7, 6, 5, 4, 3, 2, 1, 1, 0}},
        VlcTable{{6, 5, 3, 3, 3, 3, 3, 3, 4, 3, 6},
                 {1, 1, 7, 6, 5, 4, 3, 2, 1, 1, 0}},
        VlcTable{{6, 5, 3, 3, 3, 2, 3, 4, 3, 6},
                 {1, 1, 5, 4, 3, 3, 2, 1, 1, 0}},
        VlcTable{{6, 4, 5, 3, 2, 2, 3, 3, 6},
                 {1, 1, 1, 3, 3, 2, 2, 1, 0}},
        VlcTable{{6, 6, 4, 2, 2, 3, 2, 5},
                 {1, 0, 1, 3, 2, 1, 1, 1}},
        VlcTable{{5, 5, 3, 2, 2, 2, 4},
                 {1, 0, 1, 3, 2, 1, 1}},
        VlcTable{{4, 4, 3, 3, 1, 3}, {0, 1, 1, 2, 1, 3}},
        VlcTable{{4, 4, 2, 1, 3}, {0, 1, 1, 1, 1}},
        VlcTable{{3, 3, 1, 2}, {0, 1, 1, 1}},
        VlcTable{{2, 2, 1}, {0, 1, 1}},
        VlcTable{{1, 1}, {0, 1}}
    };
    return tables;
}

[[nodiscard]] const std::vector<VlcTable>& run_vlc() {
    static const std::vector<VlcTable> tables{
        VlcTable{},
        VlcTable{{1, 1}, {1, 0}},
        VlcTable{{1, 2, 2}, {1, 1, 0}},
        VlcTable{{2, 2, 2, 2}, {3, 2, 1, 0}},
        VlcTable{{2, 2, 2, 3, 3}, {3, 2, 1, 1, 0}},
        VlcTable{{2, 2, 3, 3, 3, 3}, {3, 2, 3, 2, 1, 0}},
        VlcTable{{2, 3, 3, 3, 3, 3, 3}, {3, 0, 1, 3, 2, 5, 4}}
    };
    return tables;
}

[[nodiscard]] const VlcTable& run7_vlc() {
    static const VlcTable table{
        {3, 3, 3, 3, 3, 3, 3, 4, 5, 6, 7, 8, 9, 10, 11},
        {7, 6, 5, 4, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1}
    };
    return table;
}

struct Vec2i {
    int x = 0;
    int y = 0;
};

// Movie.Block: a rectangle of the frame.  half_left/half_right/half_up/
// half_down are the managed HalfLeft/HalfRight/HalfUp/HalfDown, which the
// decoder uses to descend the macroblock tree.
struct Block {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] Block half_left() const noexcept {
        return {x, y, width / 2, height};
    }
    [[nodiscard]] Block half_right() const noexcept {
        return {x + width / 2, y, width / 2, height};
    }
    [[nodiscard]] Block half_up() const noexcept {
        return {x, y, width, height / 2};
    }
    [[nodiscard]] Block half_down() const noexcept {
        return {x, y + height / 2, width, height / 2};
    }
};

[[nodiscard]] int middle_value(int a, int b, int c) noexcept {
    if (a > b) {
        std::swap(a, b);
    }
    if (b > c) {
        std::swap(b, c);
    }
    if (a > b) {
        std::swap(a, b);
    }
    return b;
}

} // namespace

namespace {

struct AudioState {
    std::array<std::int16_t, 8> previous_samples{};
    std::array<int, 256> previous_pulses{};
    std::array<int, 8> lpc_filter{};
    std::array<int, 8> influence{};
    std::optional<int> previous_scale;
};

[[nodiscard]] AudioFrame decode_audio_frame(
    BitReader& reader, const AudioExtradata& extradata, AudioState& state) {
    const int header1 = static_cast<int>(reader.read_bits(16));
    const int header2 = static_cast<int>(reader.read_bits(16));
    const std::array<int, 3> codebook_indices{
        header1 & 0x3f,
        (header2 >> 6) & 0x3f,
        header2 & 0x3f
    };
    const int scale_modifier_index = (header1 >> 6) & 7;
    const int previous_frame_offset = (header1 >> 9) & 0x7f;
    const int pulse_packing_mode = (header2 >> 12) & 3;
    const int pulse_start_position = (header2 >> 14) & 3;
    static constexpr std::array<int, 4> pulse_data_lengths{8, 5, 4, 3};
    static constexpr std::array<int, 4> pulse_distances{3, 3, 4, 5};
    if (scale_modifier_index >= static_cast<int>(extradata.scale_modifiers.size())) {
        throw std::runtime_error("VX decoding error 021: scale modifier");
    }
    const int pulse_data_length = pulse_data_lengths[
        static_cast<std::size_t>(pulse_packing_mode)];
    std::array<std::uint16_t, 8> pulse_data{};
    for (int i = 0; i < pulse_data_length; ++i) {
        pulse_data[static_cast<std::size_t>(i)] =
            static_cast<std::uint16_t>(reader.read_bits(16));
    }
    const int pulse_value_count = pulse_packing_mode == 0
        ? 42 : pulse_data_length * 8;
    std::array<int, 42> pulse_values{};
    int pulse_value_index = 0;
    if (pulse_packing_mode == 0) {
        for (int i = 0; i < pulse_data_length; ++i) {
            for (int shift = 13; shift >= 0; shift -= 3) {
                pulse_values[static_cast<std::size_t>(pulse_value_index++)] =
                    static_cast<int>((pulse_data[static_cast<std::size_t>(i)]
                                      >> shift) & 7U) * 2 - 7;
            }
        }
        pulse_values[static_cast<std::size_t>(pulse_value_index++)] =
            static_cast<int>(((pulse_data[0] & 1U) * 4U
                              + (pulse_data[1] & 1U) * 2U
                              + (pulse_data[2] & 1U)) * 2U) - 7;
        pulse_values[static_cast<std::size_t>(pulse_value_index++)] =
            static_cast<int>(((pulse_data[3] & 1U) * 4U
                              + (pulse_data[4] & 1U) * 2U
                              + (pulse_data[5] & 1U)) * 2U) - 7;
    } else {
        for (int i = 0; i < pulse_data_length; ++i) {
            for (int shift = 14; shift >= 0; shift -= 2) {
                pulse_values[static_cast<std::size_t>(pulse_value_index++)] =
                    static_cast<int>((pulse_data[static_cast<std::size_t>(i)]
                                      >> shift) & 3U) * 2 - 3;
            }
        }
    }
    if (pulse_value_index != pulse_value_count) {
        throw std::logic_error("VX pulse table expansion has the wrong size");
    }

    int scale = extradata.scale_initial;
    if (previous_frame_offset != 127) {
        if (!state.previous_scale.has_value()) {
            throw std::runtime_error("VX audio references a missing previous frame");
        }
        scale = *state.previous_scale;
    }
    scale = static_cast<int>(static_cast<std::int64_t>(scale)
                             * extradata.scale_modifiers[
                                 static_cast<std::size_t>(scale_modifier_index)]
                             / 8192);

    std::array<int, 128> pulse_buffer{};
    if (previous_frame_offset < 126) {
        for (int i = 0; i < 128; ++i) {
            const int volume = std::min(8, std::min(i + 1, 128 - i));
            pulse_buffer[static_cast<std::size_t>(i)] =
                state.previous_pulses[static_cast<std::size_t>(
                    i + 127 - previous_frame_offset)] * volume / 16;
        }
    }
    const int pulse_distance = pulse_distances[
        static_cast<std::size_t>(pulse_packing_mode)];
    for (int i = 0; i < 128; ++i) {
        const int delta = i - pulse_start_position;
        if (delta % pulse_distance == 0) {
            const int index = delta / pulse_distance;
            if (index >= 0 && index < pulse_value_count) {
                pulse_buffer[static_cast<std::size_t>(i)] +=
                    pulse_values[static_cast<std::size_t>(index)] * scale;
            }
        }
    }

    if (previous_frame_offset == 127) {
        state.lpc_filter = extradata.lpc_base;
    }
    for (int i = 0; i < 8; ++i) {
        int coefficient_sum = 0;
        for (int j = 0; j < 3; ++j) {
            coefficient_sum += extradata.lpc_codebooks[
                static_cast<std::size_t>(j)][static_cast<std::size_t>(
                    codebook_indices[static_cast<std::size_t>(j)])][
                        static_cast<std::size_t>(i)];
        }
        state.lpc_filter[static_cast<std::size_t>(i)] += coefficient_sum;
    }

    std::array<int, 8> influence_values{};
    std::array<int, 8> influence_temp{};
    for (int i = 0; i < 8; ++i) {
        influence_temp = influence_values;
        const int coefficient = state.lpc_filter[static_cast<std::size_t>(i)];
        for (int j = 0; j < i; ++j) {
            influence_values[static_cast<std::size_t>(j)] +=
                influence_temp[static_cast<std::size_t>(i - j - 1)]
                * coefficient / 32768;
        }
        influence_values[static_cast<std::size_t>(i)] = coefficient;
    }
    for (int& value : influence_values) {
        value /= -2;
    }

    std::array<int, 32> influence_quarters{};
    if (previous_frame_offset != 127) {
        std::copy(influence_values.begin(), influence_values.end(),
                  influence_quarters.begin() + 24);
        for (int i = 0; i < 8; ++i) {
            influence_quarters[static_cast<std::size_t>(8 + i)] =
                (state.influence[static_cast<std::size_t>(i)]
                 + influence_quarters[static_cast<std::size_t>(24 + i)]) / 2;
        }
        for (int i = 0; i < 8; ++i) {
            influence_quarters[static_cast<std::size_t>(i)] =
                (state.influence[static_cast<std::size_t>(i)]
                 + influence_quarters[static_cast<std::size_t>(8 + i)]) / 2;
        }
        for (int i = 0; i < 8; ++i) {
            influence_quarters[static_cast<std::size_t>(16 + i)] =
                (influence_quarters[static_cast<std::size_t>(8 + i)]
                 + influence_quarters[static_cast<std::size_t>(24 + i)]) / 2;
        }
    } else {
        for (int quarter = 0; quarter < 4; ++quarter) {
            std::copy(influence_values.begin(), influence_values.end(),
                      influence_quarters.begin() + quarter * 8);
        }
    }

    AudioFrame frame;
    frame.scale = scale;
    for (int i = 0; i < 128; ++i) {
        const int quarter_start = i * 4 / 128 * 8;
        int sample = pulse_buffer[static_cast<std::size_t>(i)] * 16384;
        for (int j = 0; j < 8; ++j) {
            const int sample_index = i - j - 1;
            const int previous_sample = sample_index >= 0
                ? frame.samples[static_cast<std::size_t>(sample_index)]
                : state.previous_samples[static_cast<std::size_t>(sample_index + 8)];
            sample += previous_sample
                * influence_quarters[static_cast<std::size_t>(quarter_start + j)];
        }
        sample /= 16384;
        frame.samples[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(
            std::clamp(sample, static_cast<int>(std::numeric_limits<std::int16_t>::min()),
                       static_cast<int>(std::numeric_limits<std::int16_t>::max())));
    }

    std::copy(state.previous_pulses.begin() + 128,
              state.previous_pulses.end(), state.previous_pulses.begin());
    std::copy(pulse_buffer.begin(), pulse_buffer.end(),
              state.previous_pulses.begin() + 128);
    std::copy(frame.samples.begin() + 120, frame.samples.end(),
              state.previous_samples.begin());
    state.influence = influence_values;
    state.previous_scale = scale;
    return frame;
}

} // namespace

[[nodiscard]] VideoFrame decode_video_frame(
    BitReader& reader, int width, int height,
    const std::array<int, 3>& quantizer_table,
    std::array<const VideoFrame*, 3> previous);

Rgb VideoFrame::pixel_rgb(int x, int y_coordinate) const {
    if (x < 0 || y_coordinate < 0 || x >= width || y_coordinate >= height
        || y.size() != static_cast<std::size_t>(width * height)
        || u.size() != static_cast<std::size_t>(width / 2 * height / 2)
        || v.size() != static_cast<std::size_t>(width / 2 * height / 2)) {
        throw std::out_of_range("VX video pixel is outside the frame");
    }
    const int luma = y[static_cast<std::size_t>(y_coordinate * width + x)];
    const int chroma_x = x / 2;
    const int chroma_y = y_coordinate / 2;
    const std::size_t chroma_index = static_cast<std::size_t>(
        chroma_y * (width / 2) + chroma_x);
    const int u_value = static_cast<int>(u[chroma_index]) - 128;
    const int v_value = static_cast<int>(v[chroma_index]) - 128;
    return Rgb{
        clamp_byte(luma + 2 * v_value),
        clamp_byte(luma - u_value / 2 - v_value),
        clamp_byte(luma + 2 * u_value)
    };
}

std::vector<std::uint8_t> VideoFrame::rgb() const {
    std::vector<std::uint8_t> result(
        static_cast<std::size_t>(width * height * 3));
    for (int y_coordinate = 0; y_coordinate < height; ++y_coordinate) {
        for (int x = 0; x < width; ++x) {
            const Rgb pixel = pixel_rgb(x, y_coordinate);
            const std::size_t index = static_cast<std::size_t>(
                (y_coordinate * width + x) * 3);
            result[index + 0] = pixel.red;
            result[index + 1] = pixel.green;
            result[index + 2] = pixel.blue;
        }
    }
    return result;
}

VxDecoder VxDecoder::read_file(const std::filesystem::path& path) {
    return VxDecoder(read_all(path));
}

Player Player::read_file(const std::filesystem::path& path) {
    auto decoder = VxDecoder::read_file(path);
    decoder.decode();
    return Player(std::move(decoder));
}

Player Player::from_bytes(std::vector<std::uint8_t> bytes) {
    auto decoder = VxDecoder::from_bytes(std::move(bytes));
    decoder.decode();
    return Player(std::move(decoder));
}

void Player::play() noexcept {
    if (decoder_.frame_count() == 0) {
        state_ = PlaybackState::Finished;
        return;
    }
    if (state_ == PlaybackState::Finished) {
        seek(0);
    }
    state_ = PlaybackState::Playing;
}

void Player::pause() noexcept {
    if (state_ == PlaybackState::Playing) {
        state_ = PlaybackState::Paused;
    }
}

void Player::stop() noexcept {
    state_ = PlaybackState::Stopped;
    frame_index_ = 0;
    position_seconds_ = 0.0;
}

void Player::reset() noexcept {
    frame_index_ = 0;
    position_seconds_ = 0.0;
    state_ = PlaybackState::Stopped;
}

void Player::seek(std::size_t frame) noexcept {
    const std::size_t count = decoder_.frame_count();
    if (count == 0) {
        frame_index_ = 0;
        position_seconds_ = 0.0;
        state_ = PlaybackState::Finished;
        return;
    }
    frame_index_ = std::min(frame, count - 1);
    const double rate = static_cast<double>(decoder_.header().frame_rate);
    position_seconds_ = rate > 0.0
        ? static_cast<double>(frame_index_) / rate : 0.0;
    if (state_ == PlaybackState::Finished && frame_index_ + 1 < count) {
        state_ = PlaybackState::Paused;
    }
}

void Player::update(double seconds) noexcept {
    if (state_ != PlaybackState::Playing || decoder_.frame_count() == 0
        || !std::isfinite(seconds) || seconds <= 0.0) {
        return;
    }
    const double rate = static_cast<double>(decoder_.header().frame_rate);
    if (!(rate > 0.0) || !std::isfinite(rate)) {
        state_ = PlaybackState::Finished;
        return;
    }
    position_seconds_ += seconds;
    const double end = static_cast<double>(decoder_.frame_count()) / rate;
    if (position_seconds_ >= end) {
        frame_index_ = decoder_.frame_count() - 1;
        position_seconds_ = end;
        state_ = PlaybackState::Finished;
        return;
    }
    const auto next_frame = static_cast<std::size_t>(
        std::floor(position_seconds_ * rate));
    frame_index_ = std::min(next_frame, decoder_.frame_count() - 1);
}

std::vector<std::uint8_t> Player::image_rgb() const {
    return decoder_.image_rgb(frame_index_);
}

VxDecoder VxDecoder::from_bytes(std::vector<std::uint8_t> bytes) {
    return VxDecoder(std::move(bytes));
}

VxDecoder::VxDecoder(std::vector<std::uint8_t> bytes)
    : bytes_(std::move(bytes)) {
    parse_header();
}

void VxDecoder::parse_header() {
    constexpr std::size_t header_size = 4 + 11 * 4;
    if (bytes_.size() < header_size) {
        throw std::runtime_error("VX movie is smaller than its header");
    }
    core::BinaryReader reader(bytes_);
    for (char& value : header_.magic) {
        value = static_cast<char>(reader.read_u8());
    }
    if (header_.magic != std::array<char, 4>{'V', 'X', 'D', 'S'}) {
        throw std::runtime_error("VX movie has an invalid magic");
    }
    header_.frame_count = reader.read_i32_le();
    header_.frame_width = reader.read_i32_le();
    header_.frame_height = reader.read_i32_le();
    const std::int32_t frame_rate_fixed = reader.read_i32_le();
    header_.frame_rate = static_cast<float>(frame_rate_fixed) / 65536.0F;
    header_.quantizer = reader.read_i32_le();
    header_.audio_sample_rate = reader.read_i32_le();
    header_.audio_stream_count = reader.read_i32_le();
    const std::int32_t stored_max_data_size = reader.read_i32_le();
    header_.extradata_offset = reader.read_i32_le();
    header_.seek_table_offset = reader.read_i32_le();
    header_.seek_table_count = reader.read_i32_le();

    if (header_.frame_count < 0 || header_.frame_width <= 0
        || header_.frame_height <= 0 || header_.frame_width % 16 != 0
        || header_.frame_height % 16 != 0) {
        throw std::runtime_error("VX decoding error 001: invalid dimensions");
    }
    if (header_.audio_stream_count < 0 || header_.audio_stream_count > 1) {
        throw std::runtime_error("VX decoding error 022: audio stream count");
    }
    if (stored_max_data_size < 2 || (stored_max_data_size - 2) % 2 != 0) {
        throw std::runtime_error("VX movie has an invalid maximum frame size");
    }
    header_.max_data_size = stored_max_data_size - 2;
    if (header_.extradata_offset < 0 || header_.seek_table_offset < 0
        || header_.seek_table_count < 0) {
        throw std::runtime_error("VX movie has invalid table offsets");
    }
    const auto in_file = [this](std::int64_t offset, std::uint64_t size) {
        return offset >= 0 && static_cast<std::uint64_t>(offset) <= bytes_.size()
            && size <= bytes_.size() - static_cast<std::size_t>(offset);
    };
    constexpr std::size_t extradata_size = 3 * 64 * 8 * 2 + 8 * 2 + 8 * 4 + 4;
    if (!in_file(header_.extradata_offset, extradata_size)) {
        throw std::runtime_error("VX audio extradata is outside the file");
    }
    const std::uint64_t seek_bytes = static_cast<std::uint64_t>(
        header_.seek_table_count) * 8U;
    if (!in_file(header_.seek_table_offset, seek_bytes)) {
        throw std::runtime_error("VX seek table is outside the file");
    }
    if (header_.quantizer < 12 || header_.quantizer > 161) {
        throw std::runtime_error("VX decoding error 002: invalid quantizer");
    }

    core::BinaryReader extra(std::span<const std::uint8_t>(bytes_).subspan(
        static_cast<std::size_t>(header_.extradata_offset), extradata_size));
    for (auto& codebook : extradata_.lpc_codebooks) {
        for (auto& entry : codebook) {
            for (std::int16_t& coefficient : entry) {
                coefficient = extra.read_i16_le();
            }
        }
    }
    for (std::uint16_t& value : extradata_.scale_modifiers) {
        value = extra.read_u16_le();
    }
    for (std::int32_t& value : extradata_.lpc_base) {
        value = extra.read_i32_le();
    }
    extradata_.scale_initial = extra.read_i32_le();

    core::BinaryReader seek_reader(std::span<const std::uint8_t>(bytes_).subspan(
        static_cast<std::size_t>(header_.seek_table_offset),
        static_cast<std::size_t>(seek_bytes)));
    seek_table_.clear();
    seek_table_.reserve(static_cast<std::size_t>(header_.seek_table_count));
    for (int i = 0; i < header_.seek_table_count; ++i) {
        seek_table_.push_back(SeekTableEntry{
            seek_reader.read_i32_le(), seek_reader.read_i32_le()
        });
    }

    static constexpr std::array<std::array<int, 3>, 6> quantizer_values{{
        {{0x0a, 0x0d, 0x10}}, {{0x0b, 0x0e, 0x12}},
        {{0x0d, 0x10, 0x14}}, {{0x0e, 0x12, 0x17}},
        {{0x10, 0x14, 0x19}}, {{0x12, 0x17, 0x1d}}
    }};
    const int table_row = header_.quantizer / 6;
    const int table_column = header_.quantizer % 6;
    const auto& quantizer = quantizer_values[
        static_cast<std::size_t>(table_column)];
    for (std::size_t i = 0; i < quantizer_table_.size(); ++i) {
        quantizer_table_[i] = quantizer[i] << table_row;
    }
}

void VxDecoder::reset() {
    frames_.clear();
    audio_frame_total_ = 0;
    decoded_ = false;
}

void VxDecoder::decode() {
    reset();
    AudioState audio_state;
    std::size_t position = 4 + 11 * 4;
    frames_.reserve(static_cast<std::size_t>(header_.frame_count));
    for (int frame_index = 0; frame_index < header_.frame_count; ++frame_index) {
        if (position + 4 > bytes_.size()) {
            throw std::runtime_error("VX frame header is truncated");
        }
        core::BinaryReader frame_reader(
            std::span<const std::uint8_t>(bytes_).subspan(position));
        const std::uint16_t stored_size = frame_reader.read_u16_le();
        const std::uint16_t audio_frame_count = frame_reader.read_u16_le();
        position += 4;
        if (stored_size < 2 || (stored_size - 2) % 2 != 0) {
            throw std::runtime_error("VX frame has an invalid payload size");
        }
        const std::size_t data_size = stored_size - 2U;
        if (data_size > static_cast<std::size_t>(header_.max_data_size)
            || position + data_size > bytes_.size()) {
            throw std::runtime_error("VX frame payload is outside the file");
        }
        std::vector<std::uint8_t> swapped(data_size);
        for (std::size_t i = 0; i < data_size; i += 2) {
            swapped[i + 0] = bytes_[position + i + 1];
            swapped[i + 1] = bytes_[position + i + 0];
        }
        position += data_size;

        std::array<const VideoFrame*, 3> previous{};
        for (std::size_t delay = 0; delay < previous.size(); ++delay) {
            if (frames_.size() > delay) {
                previous[delay] = &frames_[frames_.size() - 1 - delay].video;
            }
        }
        BitReader bit_reader(swapped);
        Frame frame;
        frame.video = decode_video_frame(bit_reader, header_.frame_width,
                                         header_.frame_height, quantizer_table_,
                                         previous);
        frame.audio.reserve(audio_frame_count);
        for (std::uint16_t audio_index = 0;
             audio_index < audio_frame_count; ++audio_index) {
            frame.audio.push_back(decode_audio_frame(
                bit_reader, extradata_, audio_state));
        }
        audio_frame_total_ += frame.audio.size();
        frames_.push_back(std::move(frame));
    }
    decoded_ = true;
}

std::vector<std::uint8_t> VxDecoder::image_rgb(
    std::size_t frame_index) const {
    if (!decoded_) {
        throw std::logic_error("VX frames have not been decoded");
    }
    if (frame_index >= frames_.size()) {
        throw std::out_of_range("VX frame index is outside the movie");
    }
    return frames_[frame_index].video.rgb();
}

std::span<const std::int16_t> VxDecoder::audio_buffer(
    std::size_t index) const noexcept {
    if (!decoded_) {
        return {};
    }
    // Audio frames are stored per video frame, so the flat index is walked
    // rather than divided: a video frame may carry any number of them.
    std::size_t remaining = index;
    for (const Frame& frame : frames_) {
        if (remaining < frame.audio.size()) {
            return std::span<const std::int16_t>(
                frame.audio[remaining].samples);
        }
        remaining -= frame.audio.size();
    }
    return {};
}

std::vector<std::int16_t> VxDecoder::audio_samples() const {
    if (!decoded_) {
        throw std::logic_error("VX frames have not been decoded");
    }
    std::vector<std::int16_t> result;
    result.reserve(audio_frame_total_ * 128U);
    for (const Frame& frame : frames_) {
        for (const AudioFrame& audio : frame.audio) {
            result.insert(result.end(), audio.samples.begin(), audio.samples.end());
        }
    }
    return result;
}

BitReader::BitReader(std::span<const std::uint8_t> bytes) noexcept
    : bytes_(bytes) {}

std::uint32_t BitReader::read_bit() {
    if (bit_position_ >= bit_size()) {
        throw std::runtime_error("VX bitstream ended unexpectedly");
    }
    const std::size_t byte_position = bit_position_ / 8U;
    const unsigned bit_position = static_cast<unsigned>(bit_position_ % 8U);
    ++bit_position_;
    return (bytes_[byte_position] >> (7U - bit_position)) & 1U;
}

std::uint32_t BitReader::read_bits(unsigned count) {
    if (count > 32U || count > bit_size() - bit_position_) {
        throw std::runtime_error("VX bitstream read is outside the frame");
    }
    std::uint32_t value = 0;
    for (unsigned i = 0; i < count; ++i) {
        value = (value << 1U) | read_bit();
    }
    return value;
}

std::uint32_t BitReader::read_unsigned_exp_golomb() {
    unsigned zero_count = 0;
    while (read_bit() == 0U) {
        if (++zero_count > 30U) {
            throw std::runtime_error("VX Exp-Golomb value is too large");
        }
    }
    std::uint32_t value = 1U << zero_count;
    if (zero_count != 0U) {
        value |= read_bits(zero_count);
    }
    return value - 1U;
}

std::int32_t BitReader::read_signed_exp_golomb() {
    const std::uint32_t value = read_unsigned_exp_golomb() + 1U;
    const std::int32_t magnitude = static_cast<std::int32_t>(value >> 1U);
    return (value & 1U) == 0U ? magnitude : -magnitude;
}

int BitReader::consume_until_not_zero() {
    int count = 0;
    while (read_bit() == 0U) {
        ++count;
    }
    return count;
}

std::int32_t BitReader::read_int(int count) {
    if (count < 0 || count > 32) {
        throw std::runtime_error("VX bit count is outside 0..32");
    }
    std::int32_t value = 0;
    for (int i = 0; i < count; ++i) {
        value |= static_cast<std::int32_t>(read_bit()) << (count - i - 1);
    }
    return value;
}

void BitReader::align_word() {
    const std::size_t remainder = bit_position_ % 16U;
    if (remainder != 0U) {
        const std::size_t aligned = bit_position_ + (16U - remainder);
        if (aligned > bit_size()) {
            throw std::runtime_error("VX word alignment is outside the frame");
        }
        bit_position_ = aligned;
    }
}

class VideoDecoder {
public:
    VideoDecoder(BitReader& reader, int width, int height,
                 const std::array<int, 3>& quantizer_table,
                 std::array<const VideoFrame*, 3> previous)
        : reader_(reader), quantizer_table_(quantizer_table), previous_(previous),
          frame_{width, height,
                 std::vector<std::uint8_t>(static_cast<std::size_t>(width * height), 0),
                 std::vector<std::uint8_t>(static_cast<std::size_t>(width / 2 * height / 2), 0),
                 std::vector<std::uint8_t>(static_cast<std::size_t>(width / 2 * height / 2), 0)},
          coeff_y_(static_cast<std::size_t>((height / 4 + 1) * (width / 4 + 1)), 0),
          coeff_uv_(static_cast<std::size_t>((height / 8 + 1) * (width / 8 + 1)), 0),
          vector_columns_(width / 16 + 2),
          vectors_(static_cast<std::size_t>((height / 16 + 1) * vector_columns_)) {}

    [[nodiscard]] VideoFrame decode() {
        for (int y = 0; y < frame_.height; y += 16) {
            for (int x = 0; x < frame_.width; x += 16) {
                const Vec2i& top_left = vector_at(y / 16 + 1, x / 16 + 0);
                const Vec2i& top = vector_at(y / 16 + 0, x / 16 + 1);
                const Vec2i& top_right = vector_at(y / 16 + 0, x / 16 + 2);
                const Vec2i prediction{
                    middle_value(top_left.x, top.x, top_right.x),
                    middle_value(top_left.y, top.y, top_right.y)
                };
                decode_block(Block{x, y, 16, 16}, prediction);
            }
        }
        reader_.align_word();
        return std::move(frame_);
    }

private:
    [[nodiscard]] const Vec2i& vector_at(int row, int column) const {
        if (row < 0 || column < 0
            || row >= frame_.height / 16 + 1
            || column >= vector_columns_) {
            throw std::runtime_error("VX motion-vector table access is invalid");
        }
        return vectors_[static_cast<std::size_t>(row * vector_columns_ + column)];
    }

    [[nodiscard]] Vec2i& vector_at(int row, int column) {
        if (row < 0 || column < 0
            || row >= frame_.height / 16 + 1
            || column >= vector_columns_) {
            throw std::runtime_error("VX motion-vector table access is invalid");
        }
        return vectors_[static_cast<std::size_t>(row * vector_columns_ + column)];
    }

    [[nodiscard]] static std::size_t plane_index(int width, int height, int step,
                                                 int x, int y) {
        const int plane_width = width / step;
        const int plane_height = height / step;
        // C# integer division truncates toward zero.  The original decoder
        // intentionally samples x-1/y-1 for the first chroma block, where
        // -1 / 2 is still the first sample.  Keep that edge behavior while
        // rejecting coordinates that really leave the plane.
        const int plane_x = x / step;
        const int plane_y = y / step;
        if (plane_x < 0 || plane_y < 0
            || plane_x >= plane_width || plane_y >= plane_height) {
            throw std::runtime_error("VX plane access is outside the frame");
        }
        return static_cast<std::size_t>(plane_y * plane_width + plane_x);
    }

    [[nodiscard]] std::uint8_t get_plane(const std::vector<std::uint8_t>& plane,
                                         int step, int x, int y) const {
        const int width = frame_.width / step;
        const int height = frame_.height / step;
        if (x / step < 0 || y / step < 0
            || x / step >= width || y / step >= height) {
            throw std::runtime_error("VX plane access is outside the frame");
        }
        return plane[plane_index(frame_.width, frame_.height, step, x, y)];
    }

    [[nodiscard]] std::uint8_t get_plane_at(const std::vector<std::uint8_t>& plane,
                                             int step, int x, int y) const {
        return get_plane(plane, step, x, y);
    }

    void set_plane(std::vector<std::uint8_t>& plane, int step, int x, int y,
                   int value) const {
        const int width = frame_.width / step;
        const int height = frame_.height / step;
        if (x / step < 0 || y / step < 0
            || x / step >= width || y / step >= height) {
            throw std::runtime_error("VX plane access is outside the frame");
        }
        plane[plane_index(frame_.width, frame_.height, step, x, y)] = clamp_byte(value);
    }

    [[nodiscard]] std::uint8_t get_coeff(const std::vector<std::uint8_t>& buffer,
                                         int step, int x, int y) const {
        const int width = frame_.width / (step * 4) + 1;
        const int height = frame_.height / (step * 4) + 1;
        const int cx = x / (step * 4) + 1;
        const int cy = y / (step * 4) + 1;
        if (cx < 0 || cy < 0 || cx >= width || cy >= height) {
            throw std::runtime_error("VX coefficient access is outside the frame");
        }
        return buffer[static_cast<std::size_t>(cy * width + cx)];
    }

    void set_coeff(std::vector<std::uint8_t>& buffer, int step, int x, int y,
                   std::uint8_t value) const {
        const int width = frame_.width / (step * 4) + 1;
        const int height = frame_.height / (step * 4) + 1;
        const int cx = x / (step * 4) + 1;
        const int cy = y / (step * 4) + 1;
        if (cx < 0 || cy < 0 || cx >= width || cy >= height) {
            throw std::runtime_error("VX coefficient access is outside the frame");
        }
        buffer[static_cast<std::size_t>(cy * width + cx)] = value;
    }

    void clear_total_coeff(const Block& block) {
        for (int y = 0; y < block.height; y += 8) {
            for (int x = 0; x < block.width; x += 8) {
                set_coeff(coeff_y_, 1, block.x + x, block.y + y, 0);
                set_coeff(coeff_y_, 1, block.x + x, block.y + y + 4, 0);
                set_coeff(coeff_y_, 1, block.x + x + 4, block.y + y, 0);
                set_coeff(coeff_y_, 1, block.x + x + 4, block.y + y + 4, 0);
                set_coeff(coeff_uv_, 2, block.x + x, block.y + y, 0);
            }
        }
    }

    [[nodiscard]] const VideoFrame* previous_frame(int index) const {
        if (index < 0 || index >= static_cast<int>(previous_.size())
            || previous_[static_cast<std::size_t>(index)] == nullptr) {
            throw std::runtime_error("VX frame reference is unavailable");
        }
        return previous_[static_cast<std::size_t>(index)];
    }

    void decode_block(const Block& block, Vec2i prediction) {
        const int mode = static_cast<int>(reader_.read_unsigned_exp_golomb());
        switch (mode) {
        case 0: // vertical split, no residue
            if (block.width == 2) {
                throw std::runtime_error("VX decoding error 003");
            }
            decode_block(block.half_left(), prediction);
            decode_block(block.half_right(), prediction);
            if (block.width == 8 && block.height >= 8) {
                clear_total_coeff(block);
            }
            return;
        case 1: // no delta, no residue, reference 0
            predict_inter(block, prediction, false, previous_frame(0));
            if (block.width >= 8 && block.height >= 8) {
                clear_total_coeff(block);
            }
            return;
        case 2: // horizontal split, no residue
            if (block.height == 2) {
                throw std::runtime_error("VX decoding error 004");
            }
            decode_block(block.half_up(), prediction);
            decode_block(block.half_down(), prediction);
            if (block.width >= 8 && block.height == 8) {
                clear_total_coeff(block);
            }
            return;
        case 3: // unpredicted delta plus DC offset
            predict_inter_dc(block);
            if (block.width >= 8 && block.height >= 8) {
                clear_total_coeff(block);
            }
            return;
        case 4: // delta, no residue, reference 0
            predict_inter(block, prediction, true, previous_frame(0));
            if (block.width >= 8 && block.height >= 8) {
                clear_total_coeff(block);
            }
            return;
        case 5: // delta, no residue, reference 1
            predict_inter(block, prediction, true, previous_frame(1));
            if (block.width >= 8 && block.height >= 8) {
                clear_total_coeff(block);
            }
            return;
        case 6: // delta, no residue, reference 2
            predict_inter(block, prediction, true, previous_frame(2));
            if (block.width >= 8 && block.height >= 8) {
                clear_total_coeff(block);
            }
            return;
        case 7: // plane, no residue
            predict_mb_plane(block);
            if (block.width >= 8 && block.height >= 8) {
                clear_total_coeff(block);
            }
            return;
        case 8: // vertical split, residue
            if (block.width == 2) {
                throw std::runtime_error("VX decoding error 005");
            }
            decode_block(block.half_left(), prediction);
            decode_block(block.half_right(), prediction);
            decode_residue_blocks(block);
            return;
        case 9: // no delta, no residue, reference 1
            predict_inter(block, prediction, false, previous_frame(1));
            if (block.width >= 8 && block.height >= 8) {
                clear_total_coeff(block);
            }
            return;
        case 10: // unpredicted delta plus DC offset, residue
            predict_inter_dc(block);
            decode_residue_blocks(block);
            return;
        case 11: // no-tile prediction, no residue
            predict_no_tile(block);
            if (block.width >= 8 && block.height >= 8) {
                clear_total_coeff(block);
            }
            return;
        case 12: // no delta, residue, reference 0
            predict_inter(block, prediction, false, previous_frame(0));
            decode_residue_blocks(block);
            return;
        case 13: // horizontal split, residue
            if (block.height == 2) {
                throw std::runtime_error("VX decoding error 006");
            }
            decode_block(block.half_up(), prediction);
            decode_block(block.half_down(), prediction);
            decode_residue_blocks(block);
            return;
        case 14: // no delta, no residue, reference 2
            predict_inter(block, prediction, false, previous_frame(2));
            if (block.width >= 8 && block.height >= 8) {
                clear_total_coeff(block);
            }
            return;
        case 15: // 4x4 prediction, no residue
            predict4(block);
            if (block.width >= 8 && block.height >= 8) {
                clear_total_coeff(block);
            }
            return;
        case 16: // delta, residue, reference 0
            predict_inter(block, prediction, true, previous_frame(0));
            decode_residue_blocks(block);
            return;
        case 17: // delta, residue, reference 1
            predict_inter(block, prediction, true, previous_frame(1));
            decode_residue_blocks(block);
            return;
        case 18: // delta, residue, reference 2
            predict_inter(block, prediction, true, previous_frame(2));
            decode_residue_blocks(block);
            return;
        case 19: // 4x4 prediction, residue
            predict4(block);
            decode_residue_blocks(block);
            return;
        case 20: // no delta, residue, reference 1
            predict_inter(block, prediction, false, previous_frame(1));
            decode_residue_blocks(block);
            return;
        case 21: // no delta, residue, reference 2
            predict_inter(block, prediction, false, previous_frame(2));
            decode_residue_blocks(block);
            return;
        case 22: // no-tile prediction, residue
            predict_no_tile(block);
            decode_residue_blocks(block);
            return;
        case 23: // plane, residue
            predict_mb_plane(block);
            decode_residue_blocks(block);
            return;
        default:
            throw std::runtime_error("VX decoding error 007: invalid block mode");
        }
    }

    void predict_inter(const Block& block, Vec2i prediction, bool has_delta,
                       const VideoFrame* previous) {
        if (has_delta) {
            prediction.x += reader_.read_signed_exp_golomb();
            prediction.y += reader_.read_signed_exp_golomb();
        }
        vector_at(block.y / 16 + 1, block.x / 16 + 1) = prediction;
        for (int y = block.y; y < block.y + block.height; ++y) {
            for (int x = block.x; x < block.x + block.width; ++x) {
                set_plane(frame_.y, 1, x, y,
                          previous->y[static_cast<std::size_t>(
                              plane_index(previous->width, previous->height, 1,
                                          x + prediction.x, y + prediction.y))]);
            }
        }
        for (int y = block.y; y < block.y + block.height; y += 2) {
            for (int x = block.x; x < block.x + block.width; x += 2) {
                set_plane(frame_.u, 2, x, y,
                          previous->u[static_cast<std::size_t>(
                              plane_index(previous->width, previous->height, 2,
                                          x + prediction.x, y + prediction.y))]);
                set_plane(frame_.v, 2, x, y,
                          previous->v[static_cast<std::size_t>(
                              plane_index(previous->width, previous->height, 2,
                                          x + prediction.x, y + prediction.y))]);
            }
        }
    }

    void predict_inter_dc(const Block& block) {
        const Vec2i vector{reader_.read_signed_exp_golomb(),
                           reader_.read_signed_exp_golomb()};
        if (block.x + vector.x < 0
            || block.x + vector.x + block.width > frame_.width
            || block.y + vector.y < 0
            || block.y + vector.y + block.height > frame_.height) {
            throw std::runtime_error("VX decoding error 008");
        }
        const auto read_dc = [this](int error) {
            const int value = reader_.read_signed_exp_golomb();
            if (value < -(1 << 16) || value >= (1 << 16)) {
                throw std::runtime_error("VX decoding error " + std::to_string(error));
            }
            return value * 2;
        };
        const int dc_y = read_dc(9);
        const int dc_u = read_dc(10);
        const int dc_v = read_dc(11);
        const VideoFrame* previous = previous_frame(0);
        for (int y = block.y; y < block.y + block.height; ++y) {
            for (int x = block.x; x < block.x + block.width; ++x) {
                set_plane(frame_.y, 1, x, y,
                          previous->y[static_cast<std::size_t>(
                              plane_index(previous->width, previous->height, 1,
                                          x + vector.x, y + vector.y))] + dc_y);
            }
        }
        for (int y = block.y; y < block.y + block.height; y += 2) {
            for (int x = block.x; x < block.x + block.width; x += 2) {
                const auto source = plane_index(previous->width, previous->height, 2,
                                                x + vector.x, y + vector.y);
                set_plane(frame_.u, 2, x, y, previous->u[source] + dc_u);
                set_plane(frame_.v, 2, x, y, previous->v[source] + dc_v);
            }
        }
    }

    void predict_mb_plane(const Block& block) {
        const auto read_value = [this](int error) {
            const int value = reader_.read_signed_exp_golomb();
            if (value < -(1 << 16) || value >= (1 << 16)) {
                throw std::runtime_error("VX decoding error " + std::to_string(error));
            }
            return value * 2;
        };
        predict_plane(block, frame_.y, 1, read_value(12));
        predict_plane(block, frame_.u, 2, read_value(13));
        predict_plane(block, frame_.v, 2, read_value(14));
    }

    void predict_plane(const Block& block, std::vector<std::uint8_t>& plane,
                       int step, int value) {
        const int bottom_left = get_plane(plane, step, block.x - 1,
                                          block.y + block.height - 1);
        const int top_right = get_plane(plane, step,
                                        block.x + block.width - 1,
                                        block.y - 1);
        const int bottom_right_value = (bottom_left + top_right + 1) / 2 + value;
        set_plane(plane, step, block.x + block.width - 1,
                  block.y + block.height - 1, bottom_right_value);
        predict_plane_recursive(block, plane, step);
    }

    void predict_plane_recursive(const Block& block,
                                 std::vector<std::uint8_t>& plane, int step) {
        if (block.width == step && block.height == step) {
            return;
        }
        if (block.width == step && block.height > step) {
            const int top = get_plane(plane, step, block.x, block.y - 1);
            const int bottom = get_plane(plane, step, block.x,
                                         block.y + block.height - 1);
            set_plane(plane, step, block.x,
                      block.y + block.height / 2 - 1, (top + bottom) / 2);
            predict_plane_recursive(block.half_up(), plane, step);
            predict_plane_recursive(block.half_down(), plane, step);
            return;
        }
        if (block.width > step && block.height == step) {
            const int left = get_plane(plane, step, block.x - 1, block.y);
            const int right = get_plane(plane, step,
                                        block.x + block.width - 1, block.y);
            set_plane(plane, step, block.x + block.width / 2 - 1,
                      block.y, (left + right) / 2);
            predict_plane_recursive(block.half_left(), plane, step);
            predict_plane_recursive(block.half_right(), plane, step);
            return;
        }

        const int bottom_left = get_plane(plane, step, block.x - 1,
                                          block.y + block.height - 1);
        const int top_right = get_plane(plane, step,
                                        block.x + block.width - 1,
                                        block.y - 1);
        const int bottom_right = get_plane(plane, step,
                                           block.x + block.width - 1,
                                           block.y + block.height - 1);
        const int bottom_center = (bottom_left + bottom_right) / 2;
        const int center_right = (top_right + bottom_right) / 2;
        set_plane(plane, step, block.x + block.width / 2 - 1,
                  block.y + block.height - 1, bottom_center);
        set_plane(plane, step, block.x + block.width - 1,
                  block.y + block.height / 2 - 1, center_right);

        int center = 0;
        if ((block.width == 4 * step || block.width == 16 * step)
            != (block.height == 4 * step || block.height == 16 * step)) {
            const int center_left = get_plane(
                plane, step, block.x - step,
                block.y + block.height / 2 - 1);
            center = (center_left + center_right) / 2;
        } else {
            const int top_center = get_plane(
                plane, step, block.x + block.width / 2 - 1,
                block.y - 1);
            center = (top_center + bottom_center) / 2;
        }
        set_plane(plane, step, block.x + block.width / 2 - 1,
                  block.y + block.height / 2 - 1, center);
        predict_plane_recursive(block.half_left().half_up(), plane, step);
        predict_plane_recursive(block.half_right().half_up(), plane, step);
        predict_plane_recursive(block.half_left().half_down(), plane, step);
        predict_plane_recursive(block.half_right().half_down(), plane, step);
    }

    void predict_no_tile(const Block& block) {
        const int mode = static_cast<int>(reader_.read_unsigned_exp_golomb());
        switch (mode) {
        case 0:
            predict_vertical(block, frame_.y, 1);
            break;
        case 1:
            predict_horizontal(block, frame_.y, 1);
            break;
        case 2:
            predict_dc(block, frame_.y, 1);
            break;
        case 3:
            predict_plane(block, frame_.y, 1, 0);
            break;
        default:
            throw std::runtime_error("VX decoding error 017: invalid no-tile mode");
        }
        predict_no_tile_uv(block);
    }

    void predict_vertical(const Block& block, std::vector<std::uint8_t>& plane,
                          int step) {
        for (int y = block.y; y < block.y + block.height; y += step) {
            for (int x = block.x; x < block.x + block.width; x += step) {
                set_plane(plane, step, x, y,
                          get_plane(plane, step, x, block.y - 1));
            }
        }
    }

    void predict_horizontal(const Block& block,
                            std::vector<std::uint8_t>& plane, int step) {
        for (int y = block.y; y < block.y + block.height; y += step) {
            for (int x = block.x; x < block.x + block.width; x += step) {
                set_plane(plane, step, x, y,
                          get_plane(plane, step, block.x - 1, y));
            }
        }
    }

    void predict_dc(const Block& block, std::vector<std::uint8_t>& plane,
                    int step) {
        int dc = 128;
        if (block.x != 0 && block.y != 0) {
            int sum_x = block.width / 2;
            for (int x = 0; x < block.width; ++x) {
                sum_x += get_plane(plane, step, block.x + x,
                                   block.y - 1);
            }
            int sum_y = block.height / 2;
            for (int y = 0; y < block.height; ++y) {
                sum_y += get_plane(plane, step, block.x - 1,
                                   block.y + y);
            }
            dc = ((sum_x / block.width) + (sum_y / block.height) + 1) / 2;
        } else if (block.x == 0 && block.y != 0) {
            int sum_x = block.width / 2;
            for (int x = 0; x < block.width; ++x) {
                sum_x += get_plane(plane, step, block.x + x,
                                   block.y - 1);
            }
            dc = sum_x / block.width;
        } else if (block.x != 0 && block.y == 0) {
            int sum_y = block.height / 2;
            for (int y = 0; y < block.height; ++y) {
                sum_y += get_plane(plane, step, block.x - 1,
                                   block.y + y);
            }
            dc = sum_y / block.height;
        }
        for (int y = block.y; y < block.y + block.height; y += step) {
            for (int x = block.x; x < block.x + block.width; x += step) {
                set_plane(plane, step, x, y, dc);
            }
        }
    }

    void predict_no_tile_uv(const Block& block) {
        const int mode = static_cast<int>(reader_.read_unsigned_exp_golomb());
        switch (mode) {
        case 0:
            predict_dc(block, frame_.u, 2);
            predict_dc(block, frame_.v, 2);
            return;
        case 1:
            predict_horizontal(block, frame_.u, 2);
            predict_horizontal(block, frame_.v, 2);
            return;
        case 2:
            predict_vertical(block, frame_.u, 2);
            predict_vertical(block, frame_.v, 2);
            return;
        case 3:
            predict_plane(block, frame_.u, 2, 0);
            predict_plane(block, frame_.v, 2, 0);
            return;
        default:
            throw std::runtime_error("VX decoding error 018: invalid UV mode");
        }
    }

    void predict4(const Block& block) {
        std::array<int, 25> cache{};
        cache.fill(9);
        for (int y2 = 0; y2 < block.height / 4; ++y2) {
            for (int x2 = 0; x2 < block.width / 4; ++x2) {
                int mode = std::min(cache[static_cast<std::size_t>(
                                      (1 + y2 - 1) * 5 + 1 + x2)],
                                  cache[static_cast<std::size_t>(
                                      (1 + y2) * 5 + 1 + x2 - 1)]);
                if (mode == 9) {
                    mode = 2;
                }
                if (reader_.read_bit() == 0U) {
                    const int value = static_cast<int>(reader_.read_bits(3));
                    mode = value + (value >= mode ? 1 : 0);
                }
                cache[static_cast<std::size_t>((1 + y2) * 5 + 1 + x2)] = mode;
                const Vec2i vector{block.x + x2 * 4, block.y + y2 * 4};
                switch (mode) {
                case 0:
                    predict4x4_vertical(frame_.y, vector);
                    break;
                case 1:
                    predict4x4_horizontal(frame_.y, vector);
                    break;
                case 2:
                    if (vector.x != 0 && vector.y != 0) {
                        predict4x4_dc(frame_.y, vector);
                    } else if (vector.x != 0) {
                        predict4x4_left_dc(frame_.y, vector);
                    } else if (vector.y != 0) {
                        predict4x4_top_dc(frame_.y, vector);
                    } else {
                        predict4x4_dc128(frame_.y, vector);
                    }
                    break;
                case 3:
                    predict4x4_down_left(frame_.y, vector);
                    break;
                case 4:
                    predict4x4_down_right(frame_.y, vector);
                    break;
                case 5:
                    predict4x4_vertical_right(frame_.y, vector);
                    break;
                case 6:
                    predict4x4_horizontal_down(frame_.y, vector);
                    break;
                case 7:
                    predict4x4_vertical_left(frame_.y, vector);
                    break;
                case 8:
                    predict4x4_horizontal_up(frame_.y, vector);
                    break;
                default:
                    throw std::runtime_error("VX decoding error 019: invalid 4x4 mode");
                }
            }
        }
        predict_no_tile_uv(block);
    }

    void predict4x4_vertical(std::vector<std::uint8_t>& plane, Vec2i vector) {
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                set_plane(plane, 1, vector.x + x, vector.y + y,
                          get_plane(plane, 1, vector.x + x, vector.y - 1));
            }
        }
    }

    void predict4x4_horizontal(std::vector<std::uint8_t>& plane, Vec2i vector) {
        for (int y = 0; y < 4; ++y) {
            const int value = get_plane(plane, 1, vector.x - 1, vector.y + y);
            for (int x = 0; x < 4; ++x) {
                set_plane(plane, 1, vector.x + x, vector.y + y, value);
            }
        }
    }

    void predict4x4_dc(std::vector<std::uint8_t>& plane, Vec2i vector) {
        int value = get_plane(plane, 1, vector.x + 0, vector.y - 1)
            + get_plane(plane, 1, vector.x + 1, vector.y - 1)
            + get_plane(plane, 1, vector.x + 2, vector.y - 1)
            + get_plane(plane, 1, vector.x + 3, vector.y - 1)
            + get_plane(plane, 1, vector.x - 1, vector.y + 0)
            + get_plane(plane, 1, vector.x - 1, vector.y + 1)
            + get_plane(plane, 1, vector.x - 1, vector.y + 2)
            + get_plane(plane, 1, vector.x - 1, vector.y + 3);
        value = (value + 4) / 8;
        fill4x4(plane, vector, value);
    }

    void predict4x4_left_dc(std::vector<std::uint8_t>& plane, Vec2i vector) {
        int value = get_plane(plane, 1, vector.x - 1, vector.y + 0)
            + get_plane(plane, 1, vector.x - 1, vector.y + 1)
            + get_plane(plane, 1, vector.x - 1, vector.y + 2)
            + get_plane(plane, 1, vector.x - 1, vector.y + 3);
        fill4x4(plane, vector, (value + 2) / 4);
    }

    void predict4x4_top_dc(std::vector<std::uint8_t>& plane, Vec2i vector) {
        int value = get_plane(plane, 1, vector.x + 0, vector.y - 1)
            + get_plane(plane, 1, vector.x + 1, vector.y - 1)
            + get_plane(plane, 1, vector.x + 2, vector.y - 1)
            + get_plane(plane, 1, vector.x + 3, vector.y - 1);
        fill4x4(plane, vector, (value + 2) / 4);
    }

    void predict4x4_dc128(std::vector<std::uint8_t>& plane, Vec2i vector) {
        fill4x4(plane, vector, 128);
    }

    void fill4x4(std::vector<std::uint8_t>& plane, Vec2i vector, int value) {
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                set_plane(plane, 1, vector.x + x, vector.y + y, value);
            }
        }
    }

    void predict4x4_down_left(std::vector<std::uint8_t>& plane, Vec2i vector) {
        const int t0 = get_plane(plane, 1, vector.x + 0, vector.y - 1);
        const int t1 = get_plane(plane, 1, vector.x + 1, vector.y - 1);
        const int t2 = get_plane(plane, 1, vector.x + 2, vector.y - 1);
        const int t3 = get_plane(plane, 1, vector.x + 3, vector.y - 1);
        const int t4 = get_plane(plane, 1, vector.x + 4, vector.y - 1);
        const int t5 = get_plane(plane, 1, vector.x + 5, vector.y - 1);
        const int t6 = get_plane(plane, 1, vector.x + 6, vector.y - 1);
        const int t7 = get_plane(plane, 1, vector.x + 7, vector.y - 1);
        const std::array<int, 7> pixels{
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + 2 * t2 + t3 + 2) / 4,
            (t2 + 2 * t3 + t4 + 2) / 4,
            (t3 + 2 * t4 + t5 + 2) / 4,
            (t4 + 2 * t5 + t6 + 2) / 4,
            (t5 + 2 * t6 + t7 + 2) / 4,
            (t6 + 3 * t7 + 2) / 4
        };
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                set_plane(plane, 1, vector.x + x, vector.y + y,
                          pixels[static_cast<std::size_t>(x + y)]);
            }
        }
    }

    void predict4x4_down_right(std::vector<std::uint8_t>& plane, Vec2i vector) {
        const int left_top = get_plane(plane, 1, vector.x - 1, vector.y - 1);
        const int t0 = get_plane(plane, 1, vector.x + 0, vector.y - 1);
        const int t1 = get_plane(plane, 1, vector.x + 1, vector.y - 1);
        const int t2 = get_plane(plane, 1, vector.x + 2, vector.y - 1);
        const int t3 = get_plane(plane, 1, vector.x + 3, vector.y - 1);
        const int l0 = get_plane(plane, 1, vector.x - 1, vector.y + 0);
        const int l1 = get_plane(plane, 1, vector.x - 1, vector.y + 1);
        const int l2 = get_plane(plane, 1, vector.x - 1, vector.y + 2);
        const int l3 = get_plane(plane, 1, vector.x - 1, vector.y + 3);
        const std::array<int, 7> pixels{
            (l3 + 2 * l2 + l1 + 2) / 4,
            (l2 + 2 * l1 + l0 + 2) / 4,
            (l1 + 2 * l0 + left_top + 2) / 4,
            (l0 + 2 * left_top + t0 + 2) / 4,
            (left_top + 2 * t0 + t1 + 2) / 4,
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + 2 * t2 + t3 + 2) / 4
        };
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                set_plane(plane, 1, vector.x + x, vector.y + y,
                          pixels[static_cast<std::size_t>(3 + x - y)]);
            }
        }
    }

    void predict4x4_vertical_right(std::vector<std::uint8_t>& plane, Vec2i vector) {
        const int left_top = get_plane(plane, 1, vector.x - 1, vector.y - 1);
        const int t0 = get_plane(plane, 1, vector.x + 0, vector.y - 1);
        const int t1 = get_plane(plane, 1, vector.x + 1, vector.y - 1);
        const int t2 = get_plane(plane, 1, vector.x + 2, vector.y - 1);
        const int t3 = get_plane(plane, 1, vector.x + 3, vector.y - 1);
        const int l0 = get_plane(plane, 1, vector.x - 1, vector.y + 0);
        const int l1 = get_plane(plane, 1, vector.x - 1, vector.y + 1);
        const int l2 = get_plane(plane, 1, vector.x - 1, vector.y + 2);
        const std::array<int, 10> pixels{
            (l0 + 2 * l1 + l2 + 2) / 4,
            (left_top + 2 * l0 + l1 + 2) / 4,
            (l0 + 2 * left_top + t0 + 2) / 4,
            (left_top + t0 + 1) / 2,
            (left_top + 2 * t0 + t1 + 2) / 4,
            (t0 + t1 + 1) / 2,
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + t2 + 1) / 2,
            (t1 + 2 * t2 + t3 + 2) / 4,
            (t2 + t3 + 1) / 2
        };
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                set_plane(plane, 1, vector.x + x, vector.y + y,
                          pixels[static_cast<std::size_t>(3 + 2 * x - y)]);
            }
        }
    }

    void predict4x4_horizontal_down(std::vector<std::uint8_t>& plane,
                                    Vec2i vector) {
        const int left_top = get_plane(plane, 1, vector.x - 1, vector.y - 1);
        const int t0 = get_plane(plane, 1, vector.x + 0, vector.y - 1);
        const int t1 = get_plane(plane, 1, vector.x + 1, vector.y - 1);
        const int t2 = get_plane(plane, 1, vector.x + 2, vector.y - 1);
        const int l0 = get_plane(plane, 1, vector.x - 1, vector.y + 0);
        const int l1 = get_plane(plane, 1, vector.x - 1, vector.y + 1);
        const int l2 = get_plane(plane, 1, vector.x - 1, vector.y + 2);
        const int l3 = get_plane(plane, 1, vector.x - 1, vector.y + 3);
        const std::array<int, 10> pixels{
            (t0 + 2 * t1 + t2 + 2) / 4,
            (left_top + 2 * t0 + t1 + 2) / 4,
            (l0 + 2 * left_top + t0 + 2) / 4,
            (left_top + l0 + 1) / 2,
            (left_top + 2 * l0 + l1 + 2) / 4,
            (l0 + l1 + 1) / 2,
            (l0 + 2 * l1 + l2 + 2) / 4,
            (l1 + l2 + 1) / 2,
            (l1 + 2 * l2 + l3 + 2) / 4,
            (l2 + l3 + 1) / 2
        };
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                set_plane(plane, 1, vector.x + x, vector.y + y,
                          pixels[static_cast<std::size_t>(3 - x + 2 * y)]);
            }
        }
    }

    void predict4x4_vertical_left(std::vector<std::uint8_t>& plane,
                                  Vec2i vector) {
        const int t0 = get_plane(plane, 1, vector.x + 0, vector.y - 1);
        const int t1 = get_plane(plane, 1, vector.x + 1, vector.y - 1);
        const int t2 = get_plane(plane, 1, vector.x + 2, vector.y - 1);
        const int t3 = get_plane(plane, 1, vector.x + 3, vector.y - 1);
        const int t4 = get_plane(plane, 1, vector.x + 4, vector.y - 1);
        const int t5 = get_plane(plane, 1, vector.x + 5, vector.y - 1);
        const int t6 = get_plane(plane, 1, vector.x + 6, vector.y - 1);
        const std::array<int, 10> pixels{
            (t0 + t1 + 1) / 2,
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + t2 + 1) / 2,
            (t1 + 2 * t2 + t3 + 2) / 4,
            (t2 + t3 + 1) / 2,
            (t2 + 2 * t3 + t4 + 2) / 4,
            (t3 + t4 + 1) / 2,
            (t3 + 2 * t4 + t5 + 2) / 4,
            (t4 + t5 + 1) / 2,
            (t4 + 2 * t5 + t6 + 2) / 4
        };
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                set_plane(plane, 1, vector.x + x, vector.y + y,
                          pixels[static_cast<std::size_t>(2 * x + y)]);
            }
        }
    }

    void predict4x4_horizontal_up(std::vector<std::uint8_t>& plane,
                                  Vec2i vector) {
        const int l0 = get_plane(plane, 1, vector.x - 1, vector.y + 0);
        const int l1 = get_plane(plane, 1, vector.x - 1, vector.y + 1);
        const int l2 = get_plane(plane, 1, vector.x - 1, vector.y + 2);
        const int l3 = get_plane(plane, 1, vector.x - 1, vector.y + 3);
        const std::array<int, 7> pixels{
            (l0 + l1 + 1) / 2,
            (l0 + 2 * l1 + l2 + 2) / 4,
            (l1 + l2 + 1) / 2,
            (l1 + 2 * l2 + l3 + 2) / 4,
            (l2 + l3 + 1) / 2,
            (l2 + 2 * l3 + l3 + 2) / 4,
            l3
        };
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                set_plane(plane, 1, vector.x + x, vector.y + y,
                          pixels[static_cast<std::size_t>(
                              std::min(x + 2 * y, 6))]);
            }
        }
    }

    static constexpr std::array<int, 32> residue_mask_table{
        0x00, 0x08, 0x04, 0x02, 0x01, 0x1F, 0x0F, 0x0A,
        0x05, 0x0C, 0x03, 0x10, 0x0E, 0x0D, 0x0B, 0x07,
        0x09, 0x06, 0x1E, 0x1B, 0x1A, 0x1D, 0x17, 0x15,
        0x18, 0x12, 0x11, 0x1C, 0x14, 0x13, 0x16, 0x19
    };

    static constexpr std::array<int, 17> token_index_table{
        0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3
    };

    static constexpr std::array<int, 7> suffix_limits{
        0, 3, 6, 12, 24, 48, 0x8000
    };

    static constexpr std::array<int, 16> zigzag_scan_table{
        0 * 4 + 0, 1 * 4 + 0, 0 * 4 + 1, 0 * 4 + 2,
        1 * 4 + 1, 2 * 4 + 0, 3 * 4 + 0, 2 * 4 + 1,
        1 * 4 + 2, 0 * 4 + 3, 1 * 4 + 3, 2 * 4 + 2,
        3 * 4 + 1, 3 * 4 + 2, 2 * 4 + 3, 3 * 4 + 3
    };

    void decode_residue_blocks(const Block& block) {
        for (int y = 0; y < block.height; y += 8) {
            for (int x = 0; x < block.width; x += 8) {
                const int index = static_cast<int>(
                    reader_.read_unsigned_exp_golomb());
                if (index < 0 || index >= static_cast<int>(residue_mask_table.size())) {
                    throw std::runtime_error("VX decoding error 015: residue mask");
                }
                const int mask = residue_mask_table[static_cast<std::size_t>(index)];
                decode_residue_plane(mask & 1, coeff_y_, block.x + x,
                                     block.y + y, frame_.y, 1);
                decode_residue_plane(mask & 2, coeff_y_, block.x + x + 4,
                                     block.y + y, frame_.y, 1);
                decode_residue_plane(mask & 4, coeff_y_, block.x + x,
                                     block.y + y + 4, frame_.y, 1);
                decode_residue_plane(mask & 8, coeff_y_, block.x + x + 4,
                                     block.y + y + 4, frame_.y, 1);
                if ((mask & 16) != 0) {
                    const int coeff_left = get_coeff(
                        coeff_uv_, 2, block.x + x - 1, block.y + y);
                    const int coeff_top = get_coeff(
                        coeff_uv_, 2, block.x + x, block.y + y - 1);
                    const int nc = (coeff_left + coeff_top + 1) / 2;
                    const int total_u = decode_residue_cavlc(
                        block.x + x, block.y + y, nc, frame_.u, 2);
                    const int total_v = decode_residue_cavlc(
                        block.x + x, block.y + y, nc, frame_.v, 2);
                    set_coeff(coeff_uv_, 2, block.x + x, block.y + y,
                              static_cast<std::uint8_t>((total_u + total_v + 1) / 2));
                } else {
                    set_coeff(coeff_uv_, 2, block.x + x, block.y + y, 0);
                }
            }
        }
    }

    void decode_residue_plane(bool present, std::vector<std::uint8_t>& coeff,
                              int x, int y, std::vector<std::uint8_t>& plane,
                              int step) {
        if (!present) {
            set_coeff(coeff, step, x, y, 0);
            return;
        }
        const int coeff_left = get_coeff(coeff, step, x - 1, y);
        const int coeff_top = get_coeff(coeff, step, x, y - 1);
        const int nc = (coeff_left + coeff_top + 1) / 2;
        const int total = decode_residue_cavlc(x, y, nc, plane, step);
        set_coeff(coeff, step, x, y, static_cast<std::uint8_t>(total));
    }

    [[nodiscard]] int decode_residue_cavlc(
        int x, int y, int nc, std::vector<std::uint8_t>& plane, int step) {
        if (nc < 0 || nc >= static_cast<int>(token_index_table.size())) {
            throw std::runtime_error("VX decoding error 016: invalid CAVLC context");
        }
        const int token_index = token_index_table[static_cast<std::size_t>(nc)];
        const int coeff_token = coeff_token_vlc()[static_cast<std::size_t>(token_index)]
            .read_vlc2(reader_);
        int trailing_ones = coeff_token & 3;
        int total_coeff = coeff_token >> 2;
        const int output_total_coeff = total_coeff;
        if (total_coeff == 0) {
            return output_total_coeff;
        }
        if (total_coeff < 0 || total_coeff > 16) {
            throw std::runtime_error("VX decoding error 016: invalid coefficient count");
        }

        std::array<int, 16> level{};
        int level_position = 0;
        int zeroes_remaining = 0;
        if (total_coeff != 16) {
            zeroes_remaining = total_zeroes_vlc()
                .at(static_cast<std::size_t>(total_coeff)).read_vlc2(reader_);
            const int leading_zeroes = 16 - (total_coeff + zeroes_remaining);
            if (leading_zeroes < 0 || leading_zeroes > 16) {
                throw std::runtime_error("VX decoding error 016: invalid zero count");
            }
            for (int i = 0; i < leading_zeroes; ++i) {
                level[static_cast<std::size_t>(level_position++)] = 0;
            }
        }

        int suffix_length = 0;
        while (true) {
            if (trailing_ones > 0) {
                --trailing_ones;
                level[static_cast<std::size_t>(level_position++)] =
                    reader_.read_bit() == 0U ? 1 : -1;
            } else {
                int level_prefix = 0;
                while (reader_.read_bit() == 0U) {
                    if (++level_prefix > 31) {
                        throw std::runtime_error("VX level prefix is too large");
                    }
                }
                const int level_suffix = level_prefix == 15
                    ? static_cast<int>(reader_.read_bits(11))
                    : static_cast<int>(reader_.read_bits(
                          static_cast<unsigned>(suffix_length)));
                int level_code = (level_prefix << suffix_length)
                    + level_suffix + 1;
                if (suffix_length + 1 >= static_cast<int>(suffix_limits.size())) {
                    throw std::runtime_error("VX level suffix is too large");
                }
                if (level_code > suffix_limits[static_cast<std::size_t>(suffix_length + 1)]) {
                    ++suffix_length;
                }
                if (reader_.read_bit() != 0U) {
                    level_code = -level_code;
                }
                if (level_position >= 16) {
                    throw std::runtime_error("VX coefficient list is too long");
                }
                level[static_cast<std::size_t>(level_position++)] = level_code;
            }

            --total_coeff;
            if (total_coeff == 0) {
                break;
            }
            if (zeroes_remaining == 0) {
                continue;
            }
            const int run_before = zeroes_remaining < 7
                ? run_vlc().at(static_cast<std::size_t>(zeroes_remaining))
                      .read_vlc2(reader_)
                : run7_vlc().read_vlc2(reader_);
            if (run_before < 0 || run_before > zeroes_remaining) {
                throw std::runtime_error("VX run-before exceeds zero count");
            }
            zeroes_remaining -= run_before;
            if (level_position + run_before > 16) {
                throw std::runtime_error("VX coefficient list is too long");
            }
            for (int i = 0; i < run_before; ++i) {
                level[static_cast<std::size_t>(level_position++)] = 0;
            }
        }
        if (level_position + zeroes_remaining != 16) {
            throw std::runtime_error("VX coefficient list is incomplete");
        }
        for (int i = 0; i < zeroes_remaining; ++i) {
            level[static_cast<std::size_t>(level_position++)] = 0;
        }
        decode_dct(x, y, plane, step, level);
        return output_total_coeff;
    }

    void decode_dct(int x, int y, std::vector<std::uint8_t>& plane, int step,
                    const std::array<int, 16>& level) {
        std::array<int, 16> dct{};
        for (std::size_t i = 0; i < zigzag_scan_table.size(); ++i) {
            const int z = zigzag_scan_table[i];
            dct[static_cast<std::size_t>(z)] = level[15 - i]
                * quantizer_table_[static_cast<std::size_t>(
                    (z & 1) + ((z >> 2) & 1))];
        }
        dct[0] += 1 << 5;
        for (int i = 0; i < 4; ++i) {
            const int z0 = dct[static_cast<std::size_t>(i + 4 * 0)]
                + dct[static_cast<std::size_t>(i + 4 * 2)];
            const int z1 = dct[static_cast<std::size_t>(i + 4 * 0)]
                - dct[static_cast<std::size_t>(i + 4 * 2)];
            const int z2 = dct[static_cast<std::size_t>(i + 4 * 1)] / 2
                - dct[static_cast<std::size_t>(i + 4 * 3)];
            const int z3 = dct[static_cast<std::size_t>(i + 4 * 1)]
                + dct[static_cast<std::size_t>(i + 4 * 3)] / 2;
            dct[static_cast<std::size_t>(i + 4 * 0)] = z0 + z3;
            dct[static_cast<std::size_t>(i + 4 * 1)] = z1 + z2;
            dct[static_cast<std::size_t>(i + 4 * 2)] = z1 - z2;
            dct[static_cast<std::size_t>(i + 4 * 3)] = z0 - z3;
        }
        for (int i = 0; i < 4; ++i) {
            const int z0 = dct[static_cast<std::size_t>(0 + 4 * i)]
                + dct[static_cast<std::size_t>(2 + 4 * i)];
            const int z1 = dct[static_cast<std::size_t>(0 + 4 * i)]
                - dct[static_cast<std::size_t>(2 + 4 * i)];
            const int z2 = dct[static_cast<std::size_t>(1 + 4 * i)] / 2
                - dct[static_cast<std::size_t>(3 + 4 * i)];
            const int z3 = dct[static_cast<std::size_t>(1 + 4 * i)]
                + dct[static_cast<std::size_t>(3 + 4 * i)] / 2;
            const int bx = x + step * i;
            set_plane(plane, step, bx, y,
                      get_plane(plane, step, bx, y) + ((z0 + z3) >> 6));
            set_plane(plane, step, bx, y + step,
                      get_plane(plane, step, bx, y + step) + ((z1 + z2) >> 6));
            set_plane(plane, step, bx, y + 2 * step,
                      get_plane(plane, step, bx, y + 2 * step)
                          + ((z1 - z2) >> 6));
            set_plane(plane, step, bx, y + 3 * step,
                      get_plane(plane, step, bx, y + 3 * step)
                          + ((z0 - z3) >> 6));
        }
    }

    BitReader& reader_;
    const std::array<int, 3>& quantizer_table_;
    std::array<const VideoFrame*, 3> previous_{};
    VideoFrame frame_;
    std::vector<std::uint8_t> coeff_y_;
    std::vector<std::uint8_t> coeff_uv_;
    int vector_columns_ = 0;
    std::vector<Vec2i> vectors_;
};

[[nodiscard]] VideoFrame decode_video_frame(
    BitReader& reader, int width, int height,
    const std::array<int, 3>& quantizer_table,
    std::array<const VideoFrame*, 3> previous) {
    VideoDecoder decoder(reader, width, height, quantizer_table, previous);
    return decoder.decode();
}

} // namespace fruityprime::movie
