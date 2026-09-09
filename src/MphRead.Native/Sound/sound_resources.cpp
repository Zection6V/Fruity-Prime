#include "Sound/sound_resources.hpp"
#include "Metadata/MetadataValues.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace fruityprime::sound {
namespace {

using Bytes = std::span<const std::uint8_t>;

[[noreturn]] void invalid(const std::string& message) {
    throw std::runtime_error("invalid MPH sound data: " + message);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        invalid(message);
    }
}

void require_range(Bytes bytes, std::size_t offset, std::size_t length,
                   std::string_view what) {
    if (offset > bytes.size() || length > bytes.size() - offset) {
        invalid(std::string(what) + " is outside its file");
    }
}

[[nodiscard]] std::uint16_t u16(Bytes bytes, std::size_t offset) {
    require_range(bytes, offset, 2, "16-bit value");
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1]) << 8;
}

[[nodiscard]] std::uint32_t u32(Bytes bytes, std::size_t offset) {
    require_range(bytes, offset, 4, "32-bit value");
    return static_cast<std::uint32_t>(bytes[offset])
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 8
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 16
        | static_cast<std::uint32_t>(bytes[offset + 3]) << 24;
}

[[nodiscard]] bool tag_is(Bytes bytes, std::size_t offset,
                          std::string_view tag) {
    if (tag.size() != 4 || offset > bytes.size()
        || bytes.size() - offset < tag.size()) {
        return false;
    }
    for (std::size_t i = 0; i < tag.size(); ++i) {
        if (bytes[offset + i] != static_cast<std::uint8_t>(tag[i])) {
            return false;
        }
    }
    return true;
}

void require_tag(Bytes bytes, std::size_t offset, std::string_view tag,
                 std::string_view what) {
    require(tag_is(bytes, offset, tag),
            std::string(what) + " does not have the " + std::string(tag)
                + " tag");
}

constexpr std::array<int, 89> ImaStepTable{
    0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E,
    0x0010, 0x0011, 0x0013, 0x0015, 0x0017, 0x0019, 0x001C, 0x001F,
    0x0022, 0x0025, 0x0029, 0x002D, 0x0032, 0x0037, 0x003C, 0x0042,
    0x0049, 0x0050, 0x0058, 0x0061, 0x006B, 0x0076, 0x0082, 0x008F,
    0x009D, 0x00AD, 0x00BE, 0x00D1, 0x00E6, 0x00FD, 0x0117, 0x0133,
    0x0151, 0x0173, 0x0198, 0x01C1, 0x01EE, 0x0220, 0x0256, 0x0292,
    0x02D4, 0x031C, 0x036C, 0x03C3, 0x0424, 0x048E, 0x0502, 0x0583,
    0x0610, 0x06AB, 0x0756, 0x0812, 0x08E0, 0x09C3, 0x0ABD, 0x0BD0,
    0x0CFF, 0x0E4C, 0x0FBA, 0x114C, 0x1307, 0x14EE, 0x1706, 0x1954,
    0x1BDC, 0x1EA5, 0x21B6, 0x2515, 0x28CA, 0x2CDF, 0x315B, 0x364B,
    0x3BB9, 0x41B2, 0x4844, 0x4F7E, 0x5771, 0x602F, 0x69CE, 0x7462,
    0x7FFF};

void append_i16(std::vector<std::uint8_t>& bytes, std::int16_t value) {
    const auto raw = static_cast<std::uint16_t>(value);
    bytes.push_back(static_cast<std::uint8_t>(raw));
    bytes.push_back(static_cast<std::uint8_t>(raw >> 8));
}

[[nodiscard]] std::vector<std::int16_t> decode_adpcm(
    Bytes bytes, std::size_t sample_count, bool rounding_error) {
    require_range(bytes, 0, 4, "IMA-ADPCM decoder header");
    require(sample_count <= (bytes.size() - 4) * 2,
            "IMA-ADPCM data is shorter than its sample count");

    std::vector<std::int16_t> result;
    result.reserve(sample_count);
    int sample_value = static_cast<std::int16_t>(u16(bytes, 0));
    int step_index = std::clamp(static_cast<int>(u16(bytes, 2)), 0, 88);
    for (std::size_t i = 0; i < sample_count; ++i) {
        const std::uint8_t packed = bytes[4 + i / 2];
        const int value = (i & 1U) == 0 ? packed & 0x0F : packed >> 4;
        const int step = ImaStepTable[step_index];
        int difference = step >> 3;
        if ((value & 1) != 0) {
            difference += step >> 2;
        }
        if ((value & 2) != 0) {
            difference += step >> 1;
        }
        if ((value & 4) != 0) {
            difference += step;
        }
        if ((value & 8) != 0) {
            sample_value -= difference;
            sample_value = std::max(sample_value,
                                    rounding_error ? -32767 : -32768);
        } else {
            sample_value += difference;
            sample_value = std::min(sample_value, 32767);
        }
        step_index = std::clamp(
            step_index + metadata::ImaIndexTable[value], 0, 88);
        result.push_back(static_cast<std::int16_t>(sample_value));
    }
    return result;
}

void append_pcm16(std::vector<std::uint8_t>& output,
                  std::span<const std::int16_t> samples) {
    output.reserve(output.size() + samples.size() * 2);
    for (const auto sample : samples) {
        append_i16(output, sample);
    }
}

[[nodiscard]] std::vector<std::uint8_t> decode_stream_block(
    Bytes bytes, WaveFormat format, std::size_t sample_count) {
    switch (format) {
    case WaveFormat::Pcm8:
        require_range(bytes, 0, sample_count, "STRM PCM8 block");
        {
            std::vector<std::uint8_t> result(sample_count);
            for (std::size_t i = 0; i < sample_count; ++i) {
                result[i] = static_cast<std::uint8_t>(bytes[i] ^ 0x80);
            }
            return result;
        }
    case WaveFormat::Pcm16:
        require_range(bytes, 0, sample_count * 2, "STRM PCM16 block");
        return std::vector<std::uint8_t>(bytes.begin(),
                                         bytes.begin() + sample_count * 2);
    case WaveFormat::Adpcm: {
        const auto samples = decode_adpcm(bytes, sample_count, false);
        std::vector<std::uint8_t> result;
        append_pcm16(result, samples);
        return result;
    }
    case WaveFormat::None:
        break;
    }
    invalid("unsupported stream format");
}

} // namespace

std::vector<std::int16_t> Sample::decode_pcm(
    bool adpcm_rounding_error) const {
    if (!present) {
        return {};
    }
    switch (format) {
    case WaveFormat::Pcm8: {
        std::vector<std::int16_t> result;
        result.reserve(encoded.size());
        for (const auto value : encoded) {
            const auto signed_value = static_cast<std::int8_t>(value);
            const int scaled = static_cast<int>(signed_value) * 258;
            result.push_back(static_cast<std::int16_t>(
                std::clamp(scaled, -32768, 32767)));
        }
        return result;
    }
    case WaveFormat::Pcm16: {
        require(encoded.size() % 2 == 0,
                "custom PCM16 sample is not sample aligned");
        std::vector<std::int16_t> result;
        result.reserve(encoded.size() / 2);
        for (std::size_t i = 0; i < encoded.size(); i += 2) {
            result.push_back(static_cast<std::int16_t>(
                static_cast<std::uint16_t>(encoded[i])
                | static_cast<std::uint16_t>(encoded[i + 1]) << 8));
        }
        return result;
    }
    case WaveFormat::Adpcm: {
        const std::uint64_t count =
            (static_cast<std::uint64_t>(sample_start) * 4 - 4) * 2
            + static_cast<std::uint64_t>(sample_length) * 8;
        require(sample_start >= 1,
                "custom IMA-ADPCM sample starts before its decoder header");
        require(count <= std::numeric_limits<std::size_t>::max(),
                "custom IMA-ADPCM sample count overflows the host size");
        return decode_adpcm(encoded, static_cast<std::size_t>(count),
                            adpcm_rounding_error);
    }
    case WaveFormat::None:
        return {};
    }
    return {};
}

std::vector<Sample> parse_sample_table(Bytes bytes) {
    require_range(bytes, 0, 4, "sound sample table");
    const std::uint32_t count = u32(bytes, 0);
    require(count <= (bytes.size() - 4) / 4,
            "sound sample table contains too many offsets");

    std::vector<Sample> result(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t offset = u32(bytes, 4 + i * 4);
        result[i].id = i;
        result[i].offset = offset;
        if (offset == 0) {
            continue;
        }
        const Bytes header = bytes.subspan(offset);
        require_range(header, 0, 0x0C, "sound sample header");
        const auto raw_format = header[0];
        require(raw_format <= static_cast<std::uint8_t>(WaveFormat::Adpcm),
                "sound sample has an unsupported format");
        auto& sample = result[i];
        sample.present = true;
        sample.format = static_cast<WaveFormat>(raw_format);
        sample.loop = header[1] != 0;
        sample.sample_rate = u16(header, 2);
        sample.timer = u16(header, 4);
        sample.sample_start = u16(header, 6);
        sample.sample_length = u32(header, 8);
        const std::uint64_t data_size =
            (static_cast<std::uint64_t>(sample.sample_start)
             + sample.sample_length)
            * 4;
        require(data_size <= std::numeric_limits<std::size_t>::max(),
                "sound sample size overflows the host size");
        require_range(header, 0x0C, static_cast<std::size_t>(data_size),
                      "sound sample data");
        sample.encoded.assign(header.begin() + 0x0C,
                              header.begin() + 0x0C + data_size);
        if (sample.format == WaveFormat::Adpcm) {
            require(sample.sample_start >= 1,
                    "sound sample ADPCM start has no decoder header");
            sample.loop_start = (sample.sample_start * 4 - 4) * 2;
            sample.loop_length = sample.sample_length * 8;
        } else {
            sample.loop_start = sample.sample_start;
            sample.loop_length = sample.sample_length;
        }
    }
    return result;
}

std::vector<std::int16_t> FhSample::decode_pcm(
    bool adpcm_rounding_error) const {
    if (!present) {
        return {};
    }
    if (format == WaveFormat::Pcm8) {
        const std::uint64_t count64 =
            static_cast<std::uint64_t>(sample_start) + sample_length;
        require(count64 <= std::numeric_limits<std::size_t>::max(),
                "First Hunt PCM8 sample count overflows the host size");
        const std::size_t count = static_cast<std::size_t>(count64);
        require(count <= encoded.size(),
                "First Hunt PCM8 sample is shorter than its loop range");
        std::vector<std::int16_t> result;
        result.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            const auto signed_value = static_cast<std::int8_t>(encoded[i]);
            result.push_back(static_cast<std::int16_t>(
                std::clamp(static_cast<int>(signed_value) * 258,
                           -32768, 32767)));
        }
        return result;
    }
    if (format == WaveFormat::Adpcm) {
        require(sample_start >= 1,
                "First Hunt IMA-ADPCM sample starts before its decoder header");
        const std::uint64_t count =
            (static_cast<std::uint64_t>(sample_start) * 4 - 4) * 2
            + static_cast<std::uint64_t>(sample_length) * 8;
        require(count <= std::numeric_limits<std::size_t>::max(),
                "First Hunt IMA-ADPCM sample count overflows the host size");
        return decode_adpcm(encoded, static_cast<std::size_t>(count),
                            adpcm_rounding_error);
    }
    return {};
}

Stream Stream::parse(Bytes bytes, std::uint32_t id, std::string name) {
    require_range(bytes, 0, 0x40, "STRM header");
    require_tag(bytes, 0, "STRM", "STRM");
    require(u32(bytes, 4) == 0x0100FEFF,
            "STRM has an unexpected Nitro magic");
    require_tag(bytes, 0x10, "HEAD", "STRM HEAD");
    const auto raw_format = bytes[0x18];
    require(raw_format <= static_cast<std::uint8_t>(WaveFormat::Adpcm),
            "STRM has an unsupported format");
    const auto channels = bytes[0x1A];
    require(channels == 1 || channels == 2,
            "STRM has an unsupported channel count");

    Stream result;
    result.id = id;
    result.name = std::move(name);
    result.format = static_cast<WaveFormat>(raw_format);
    result.loop = bytes[0x19] != 0;
    result.channel_count = channels;
    result.sample_rate = u16(bytes, 0x1C);
    result.timer = u16(bytes, 0x1E);
    result.loop_start = u32(bytes, 0x20);
    result.loop_end = u32(bytes, 0x24);
    const std::uint32_t data_offset = u32(bytes, 0x28);
    const std::uint32_t block_count = u32(bytes, 0x2C);
    const std::uint32_t block_size = u32(bytes, 0x30);
    const std::uint32_t block_samples = u32(bytes, 0x34);
    const std::uint32_t last_block_size = u32(bytes, 0x38);
    const std::uint32_t last_block_samples = u32(bytes, 0x3C);
    require(block_count > 0, "STRM has no data blocks");
    require(block_size > 0 && last_block_size > 0,
            "STRM has an empty data block");

    const std::uint64_t total_samples =
        static_cast<std::uint64_t>(block_samples) * (block_count - 1)
        + last_block_samples;
    require(total_samples <= std::numeric_limits<std::size_t>::max(),
            "STRM sample count overflows the host size");
    const std::size_t bytes_per_sample = result.format == WaveFormat::Pcm8
        ? 1
        : 2;
    result.channels.resize(channels);
    for (std::size_t channel = 0; channel < channels; ++channel) {
        result.channels[channel].reserve(
            static_cast<std::size_t>(total_samples) * bytes_per_sample);
        std::uint64_t start = static_cast<std::uint64_t>(data_offset)
            + static_cast<std::uint64_t>(block_size) * channel;
        for (std::uint32_t block = 0; block < block_count; ++block) {
            const bool last = block == block_count - 1;
            const std::uint32_t size = last ? last_block_size : block_size;
            const std::uint32_t samples =
                last ? last_block_samples : block_samples;
            require(start <= std::numeric_limits<std::size_t>::max(),
                    "STRM block offset overflows the host size");
            const std::size_t block_offset = static_cast<std::size_t>(start);
            require_range(bytes, block_offset, size, "STRM data block");
            const auto decoded = decode_stream_block(
                bytes.subspan(block_offset, size), result.format, samples);
            result.channels[channel].insert(result.channels[channel].end(),
                                             decoded.begin(), decoded.end());

            if (!last) {
                std::uint64_t increment =
                    static_cast<std::uint64_t>(size) * channels;
                if (channel > 0 && block == block_count - 2) {
                    increment = static_cast<std::uint64_t>(size)
                        + last_block_size;
                }
                start += increment;
            }
        }
    }
    return result;
}

std::vector<std::uint8_t> Stream::interleaved_data() const {
    if (channels.empty()) {
        return {};
    }
    const std::size_t bytes_per_sample = format == WaveFormat::Pcm8 ? 1 : 2;
    require(channels[0].size() % bytes_per_sample == 0,
            "stream channel is not sample aligned");
    const std::size_t sample_count = channels[0].size() / bytes_per_sample;
    for (const auto& channel : channels) {
        require(channel.size() == channels[0].size(),
                "stream channels have different lengths");
    }

    std::vector<std::uint8_t> result(
        sample_count * channels.size() * bytes_per_sample);
    std::size_t destination = 0;
    for (std::size_t sample = 0; sample < sample_count; ++sample) {
        for (const auto& channel : channels) {
            const std::size_t source = sample * bytes_per_sample;
            std::copy_n(channel.begin() + source, bytes_per_sample,
                        result.begin() + destination);
            destination += bytes_per_sample;
        }
    }
    return result;
}

} // namespace fruityprime::sound
