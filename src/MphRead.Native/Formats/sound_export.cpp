#include "Formats/sound_export.hpp"

#include <fstream>
#include <iostream>
#include <iterator>

namespace fruityprime::sound {
namespace {

[[nodiscard]] std::vector<std::uint8_t> read_all(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw WaveExportError("could not open " + path.string());
    }
    const std::streamoff length = input.tellg();
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    }
    return bytes;
}

} // namespace

void write_tag(std::vector<std::uint8_t>& out, std::string_view tag) {
    out.insert(out.end(), tag.begin(), tag.end());
}

void write_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}

void write_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value >> 16));
    out.push_back(static_cast<std::uint8_t>(value >> 24));
}

void write_wav_header(std::vector<std::uint8_t>& out, std::uint32_t sample_count,
                      std::uint16_t sample_rate, WaveFormat format) {
    // ADPCM is decoded to 16-bit before it is written, so only PCM8 stays
    // eight bits wide.
    const std::uint32_t bits = format == WaveFormat::Pcm8 ? 8u : 16u;
    constexpr std::uint32_t HeaderSize = 0x2c;
    const std::uint32_t wave_size = sample_count * bits / 8 + HeaderSize;
    const std::uint32_t decoded_size = sample_count * (bits / 8);
    write_tag(out, "RIFF");
    write_u32(out, wave_size - 8);
    write_tag(out, "WAVE");
    write_tag(out, "fmt ");
    write_u32(out, 16);
    write_u16(out, 1);
    write_u16(out, 1);
    write_u32(out, sample_rate);
    write_u32(out, sample_rate * (bits / 8));
    write_u16(out, static_cast<std::uint16_t>(bits / 8));
    write_u16(out, static_cast<std::uint16_t>(bits));
    write_tag(out, "data");
    write_u32(out, decoded_size);
}

std::vector<std::uint8_t> build_wav(std::span<const std::uint8_t> wave_data,
                                    std::uint32_t sample_count,
                                    std::uint16_t sample_rate,
                                    WaveFormat format,
                                    std::string_view name,
    bool /*adpcm_rounding_error*/) {
    if (wave_data.empty()) {
        throw WaveExportError("Sample " + std::string(name)
                              + " contains no data.");
    }
    if (format != WaveFormat::Adpcm
        && format != WaveFormat::Pcm8
        && format != WaveFormat::Pcm16) {
        throw WaveExportError("Format " + std::to_string(
                                  static_cast<int>(format))
                              + " is unsupported.");
    }
    std::vector<std::uint8_t> out;
    write_wav_header(out, sample_count, sample_rate, format);
    out.insert(out.end(), wave_data.begin(), wave_data.end());
    return out;
}

namespace {

[[nodiscard]] std::string pad_id(std::uint32_t id) {
    std::string result = std::to_string(id);
    if (result.size() < 3) {
        result.insert(0, 3 - result.size(), '0');
    }
    return result;
}

// The exporter reads the same five fields from a First Hunt sample and from a
// cartridge one, and the two types are unrelated, so the body is shared here
// rather than written twice.
template <typename SampleType>
void export_one(const std::filesystem::path& directory,
                const SampleType& sample, std::string_view name,
                bool adpcm_rounding_error, std::string_view prefix) {
    std::vector<std::uint8_t> bytes;
    const std::uint64_t sample_count64 =
        static_cast<std::uint64_t>(sample.loop_start)
        + static_cast<std::uint64_t>(sample.loop_length);
    const auto sample_count = static_cast<std::uint32_t>(sample_count64);
    if (sample.format == WaveFormat::Adpcm) {
        // C# GetWaveData decodes ADPCM to signed little-endian PCM16 before
        // handing the bytes to ExportAudio.
        const auto pcm = sample.decode_pcm(adpcm_rounding_error);
        bytes.reserve(pcm.size() * 2);
        for (const std::int16_t value : pcm) {
            const auto raw = static_cast<std::uint16_t>(value);
            bytes.push_back(static_cast<std::uint8_t>(raw));
            bytes.push_back(static_cast<std::uint8_t>(raw >> 8));
        }
    } else {
        // SoundRead.GetWaveData XORs every non-ADPCM byte with 0x80.  This is
        // the PCM8 path used by First Hunt; retaining the same branch also
        // preserves the managed behavior for an explicitly supplied PCM16
        // sample.
        bytes.reserve(sample_count);
        for (std::size_t i = 0; i < sample_count; ++i) {
            bytes.push_back(static_cast<std::uint8_t>(sample.encoded.at(i)
                                                       ^ 0x80));
        }
    }
    const auto wav = build_wav(
        bytes, sample_count,
        static_cast<std::uint16_t>(sample.sample_rate), sample.format, name,
        adpcm_rounding_error);
    std::filesystem::create_directories(directory);
    const auto path = directory
        / (std::string(prefix) + std::string(name) + ".wav");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw WaveExportError("could not write " + path.string());
    }
    output.write(reinterpret_cast<const char*>(wav.data()),
                 static_cast<std::streamsize>(wav.size()));
}

} // namespace

void export_sample(const std::filesystem::path& directory,
                   const FhSample& sample, std::string_view name,
                   bool adpcm_rounding_error, std::string_view prefix) {
    export_one(directory, sample, name, adpcm_rounding_error, prefix);
}

void export_sample(const std::filesystem::path& directory,
                   const Sample& sample, std::string_view name,
                   bool adpcm_rounding_error, std::string_view prefix) {
    export_one(directory, sample, name, adpcm_rounding_error, prefix);
}

std::size_t export_samples(const std::filesystem::path& directory,
                           std::span<const FhSample> samples,
                           bool adpcm_rounding_error,
                           std::string_view prefix) {
    std::size_t written = 0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        try {
            export_sample(directory, samples[i],
                          pad_id(samples[i].id),
                          adpcm_rounding_error, prefix);
            ++written;
        } catch (const WaveExportError& ex) {
            std::cout << "[" << samples[i].id << "] WaveExportException: "
                      << ex.what() << '\n';
        }
    }
    return written;
}

std::size_t export_samples(const std::filesystem::path& directory,
                           std::span<const Sample> samples,
                           bool adpcm_rounding_error,
                           std::string_view prefix) {
    std::size_t written = 0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        try {
            export_sample(directory, samples[i],
                          pad_id(samples[i].id),
                          adpcm_rounding_error, prefix);
            ++written;
        } catch (const WaveExportError& ex) {
            std::cout << "[" << samples[i].id << "] WaveExportException: "
                      << ex.what() << '\n';
        }
    }
    return written;
}

void export_sound_sample(const std::filesystem::path& directory,
                         std::span<const Sample> samples, int id,
                         bool adpcm_rounding_error) {
    if (id < 0 || static_cast<std::size_t>(id) >= samples.size()) {
        return;
    }
    export_sample(directory, samples[static_cast<std::size_t>(id)],
                  pad_id(static_cast<std::uint32_t>(id)),
                  adpcm_rounding_error);
}

void export_wfs_sample(const std::filesystem::path& directory,
                       std::span<const Sample> wfs_samples, int id,
                       bool adpcm_rounding_error) {
    if (id < 0 || static_cast<std::size_t>(id) >= wfs_samples.size()) {
        return;
    }
    export_sample(directory, wfs_samples[static_cast<std::size_t>(id)],
                  pad_id(static_cast<std::uint32_t>(id)),
                  adpcm_rounding_error);
}

std::size_t export_wfs_samples(const std::filesystem::path& directory,
                               std::span<const Sample> wfs_samples,
                               bool adpcm_rounding_error) {
    return export_samples(directory, wfs_samples, adpcm_rounding_error,
                          WfsExportPrefix);
}

std::size_t export_streams(const std::filesystem::path& directory,
                           std::span<const Stream> streams) {
    std::size_t written = 0;
    for (const Stream& stream : streams) {
        const std::string id = pad_id(stream.id);
        for (std::size_t channel_index = 0;
             channel_index < stream.channels.size(); ++channel_index) {
            std::string suffix;
            if (stream.channels.size() == 2) {
                suffix = channel_index == 0 ? "_L" : "_R";
            }
            const std::string name = id + "_" + stream.name + suffix;
            std::size_t sample_count = stream.channels[channel_index].size();
            if (stream.format == WaveFormat::Adpcm) {
                sample_count /= 2;
            }
            const auto wav = build_wav(stream.channels[channel_index],
                                       static_cast<std::uint32_t>(sample_count),
                                       stream.sample_rate, stream.format, name);
            std::filesystem::create_directories(directory);
            const auto path = directory / (name + ".wav");
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output) {
                throw WaveExportError("could not write " + path.string());
            }
            output.write(reinterpret_cast<const char*>(wav.data()),
                         static_cast<std::streamsize>(wav.size()));
            ++written;
        }
    }
    return written;
}

std::vector<FhSample> read_fh_sound_file(const std::filesystem::path& fh_root,
                                         std::string_view filename) {
    const auto bytes = read_all(fh_root / "sound" / std::string(filename));
    return parse_fh_sound_file(bytes);
}

std::size_t export_fh_sound_file(const std::filesystem::path& fh_root,
                                 const std::filesystem::path& destination,
                                 const FhSoundFile& file,
                                 bool adpcm_rounding_error) {
    const auto samples = read_fh_sound_file(fh_root, file.filename);
    return export_samples(destination, samples, adpcm_rounding_error,
                          file.export_prefix);
}

std::size_t export_all_fh(const std::filesystem::path& fh_root,
                          const std::filesystem::path& destination,
                          bool adpcm_rounding_error) {
    // The managed ExportAllFh runs these four in this order.
    std::size_t written = 0;
    for (const FhSoundFile& file :
         {FhBgmFile, FhGlobalSfxFile, FhMenuSfxFile, FhSfxFile}) {
        written += export_fh_sound_file(fh_root, destination, file,
                                        adpcm_rounding_error);
    }
    return written;
}

} // namespace fruityprime::sound
