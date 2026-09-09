#include "Formats/sound_layouts.hpp"
#include "Sound/sound_resources.hpp"

#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fruityprime::sound {
namespace {

using Bytes = std::span<const std::uint8_t>;

[[noreturn]] void invalid_fh(std::string_view message) {
    throw std::runtime_error("invalid First Hunt sound data: "
                             + std::string(message));
}

void require_fh(bool condition, std::string_view message) {
    if (!condition) {
        invalid_fh(message);
    }
}

void require_fh_range(Bytes bytes, std::size_t offset, std::size_t length,
                      std::string_view what) {
    if (offset > bytes.size() || length > bytes.size() - offset) {
        invalid_fh(std::string(what) + " is outside its file");
    }
}

[[nodiscard]] std::uint16_t fh_u16(Bytes bytes, std::size_t offset) {
    require_fh_range(bytes, offset, 2, "16-bit value");
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1]) << 8;
}

[[nodiscard]] std::uint32_t fh_u32(Bytes bytes, std::size_t offset) {
    require_fh_range(bytes, offset, 4, "32-bit value");
    return static_cast<std::uint32_t>(bytes[offset])
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 8
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 16
        | static_cast<std::uint32_t>(bytes[offset + 3]) << 24;
}

} // namespace

std::vector<FhSample> parse_fh_sound_file(Bytes bytes) {
    require_fh_range(bytes, 0, 4, "First Hunt sound file");
    const std::uint32_t count = fh_u32(bytes, 0);
    require_fh(count <= (bytes.size() - 4) / 4,
              "First Hunt sound file contains too many offsets");

    std::vector<FhSample> result(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t offset = fh_u32(bytes, 4 + i * 4);
        auto& sample = result[i];
        sample.id = i;
        sample.offset = offset;
        if (offset == 0) {
            continue;
        }
        require_fh_range(bytes, offset, 24,
                         "First Hunt sound sample header");
        sample.data_size = fh_u32(bytes, offset);
        sample.data_pointer = fh_u32(bytes, offset + 4);
        sample.sample_rate = fh_u32(bytes, offset + 8);
        sample.volume = fh_u16(bytes, offset + 12);
        sample.field_e = bytes[offset + 14];
        const std::uint8_t raw_format = bytes[offset + 15];
        const std::uint32_t loop_begin = fh_u32(bytes, offset + 16);
        const std::uint32_t loop_end = fh_u32(bytes, offset + 20);

        // The managed reader treats <=4 bytes as a null entry. This is
        // common in the menu/global banks and is not a malformed header.
        if (sample.data_size <= 4) {
            sample.offset = 0;
            continue;
        }
        require_fh(loop_end >= loop_begin,
                   "First Hunt sound loop end precedes its start");
        require_fh_range(bytes, static_cast<std::size_t>(offset) + 24,
                         sample.data_size, "First Hunt sound sample data");
        sample.present = true;
        sample.encoded.assign(bytes.begin() + offset + 24,
                              bytes.begin() + offset + 24 + sample.data_size);
        if (raw_format == 4) {
            sample.format = WaveFormat::Adpcm;
            sample.loop = loop_begin > 1;
            sample.sample_start = loop_begin;
            sample.sample_length = loop_end - loop_begin;
            if (static_cast<std::uint64_t>(loop_end) * 4
                > sample.data_size && sample.sample_length > 0) {
                // A few original headers count one padding nibble in the
                // end value; this matches the managed First Hunt reader.
                --sample.sample_length;
            }
            sample.loop_start = (sample.sample_start * 4 - 4) * 2;
            sample.loop_length = sample.sample_length * 8;
        } else if (raw_format == 0) {
            sample.format = WaveFormat::Pcm8;
            sample.loop = loop_begin > 0;
            sample.sample_start = loop_begin;
            sample.sample_length = loop_end - loop_begin;
            sample.loop_start = sample.sample_start;
            sample.loop_length = sample.sample_length;
        } else {
            invalid_fh("First Hunt sound has an unexpected format");
        }
    }
    return result;
}

} // namespace fruityprime::sound
