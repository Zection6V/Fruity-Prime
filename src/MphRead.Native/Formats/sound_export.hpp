#pragma once

// Native counterpart of the WAV export in Formats/Sound.cs and the First Hunt
// entry points in Formats/FhSound.cs.
//
// The cartridge stores samples as PCM8, PCM16 or IMA ADPCM; a .wav can only
// carry the first two, so an ADPCM sample is decoded on the way out.  The
// header is written by hand rather than through a library because its field
// order is part of what the managed exporter produces byte for byte.

#include "Sound/sound_resources.hpp"
#include "Formats/Types.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::sound {

// Sound.WaveExportException
class WaveExportError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Sound.BinaryWriterExtensions: WriteC writes the four characters of a RIFF
// tag with no length prefix, Write2/Write4 write little-endian integers.
void write_tag(std::vector<std::uint8_t>& out, std::string_view tag);
void write_u16(std::vector<std::uint8_t>& out, std::uint16_t value);
void write_u32(std::vector<std::uint8_t>& out, std::uint32_t value);

// Sound.WriteWavHeader
void write_wav_header(std::vector<std::uint8_t>& out, std::uint32_t sample_count,
                      std::uint16_t sample_rate, WaveFormat format);

// Sound.ExportAudio: the decoded PCM plus its header, ready to write out.
[[nodiscard]] std::vector<std::uint8_t> build_wav(
    std::span<const std::uint8_t> wave_data, std::uint32_t sample_count,
    std::uint16_t sample_rate, WaveFormat format,
    std::string_view name, bool adpcm_rounding_error = false);

// Sound.ExportSample / ExportSamples
void export_sample(const std::filesystem::path& directory,
                   const FhSample& sample, std::string_view name,
                   bool adpcm_rounding_error = false,
                   std::string_view prefix = {});
std::size_t export_samples(const std::filesystem::path& directory,
                           std::span<const FhSample> samples,
                           bool adpcm_rounding_error = false,
                           std::string_view prefix = {});

// The same two, for the cartridge's own SNDSAMPLES/WFSSNDSAMPLES tables.  A
// Sound.SoundSample carries the same fields the exporter reads, but it is a
// different type from a First Hunt sample and the managed code keeps the two
// paths apart, so these are overloads rather than a shared entry point.
void export_sample(const std::filesystem::path& directory,
                   const Sample& sample, std::string_view name,
                   bool adpcm_rounding_error = false,
                   std::string_view prefix = {});
std::size_t export_samples(const std::filesystem::path& directory,
                           std::span<const Sample> samples,
                           bool adpcm_rounding_error = false,
                           std::string_view prefix = {});

// Sound.ExportSample(int id): one sample from SNDSAMPLES.DAT, named by its
// index.  An index outside the table, or an empty slot, writes nothing.
void export_sound_sample(const std::filesystem::path& directory,
                         std::span<const Sample> samples, int id,
                         bool adpcm_rounding_error = false);

// Sound.ExportWfsSample(int id): the same for WFSSNDSAMPLES.DAT.  The managed
// single-sample form deliberately writes it with no prefix, unlike the bulk
// export below -- exporting one by hand names it after the id alone.
void export_wfs_sample(const std::filesystem::path& directory,
                       std::span<const Sample> wfs_samples, int id,
                       bool adpcm_rounding_error = false);

// Sound.ExportWfsSamples: the whole WFS table, prefixed so it cannot collide
// with the main one, which uses the same indices.
inline constexpr std::string_view WfsExportPrefix = "mph_wfs_";
std::size_t export_wfs_samples(const std::filesystem::path& directory,
                               std::span<const Sample> wfs_samples,
                               bool adpcm_rounding_error = false);

// FhSound.ReadFhSfx / ReadFhBgm / ReadFhMenuSfx / ReadFhGlobalSfx.  The four
// First Hunt sound files sit beside each other under `sound`; the managed
// code names them one function each, and the export prefixes differ, so the
// name is part of the contract rather than an implementation detail.
struct FhSoundFile {
    std::string_view filename;
    std::string_view export_prefix;
};

inline constexpr FhSoundFile FhSfxFile{"SFXDATA.BIN", "fh_"};
inline constexpr FhSoundFile FhBgmFile{"BGMDATA.BIN", "fh_bgm_"};
inline constexpr FhSoundFile FhMenuSfxFile{"MENUSFXDATA.BIN", "fh_menu_"};
inline constexpr FhSoundFile FhGlobalSfxFile{"GLOBALSFXDATA.BIN", "fh_lid_"};

// FhSound.ReadFhSoundFile
[[nodiscard]] std::vector<FhSample> read_fh_sound_file(
    const std::filesystem::path& fh_root, std::string_view filename);

[[nodiscard]] inline std::vector<FhSample> read_fh_sfx(
    const std::filesystem::path& root) {
    return read_fh_sound_file(root, FhSfxFile.filename);
}
[[nodiscard]] inline std::vector<FhSample> read_fh_bgm(
    const std::filesystem::path& root) {
    return read_fh_sound_file(root, FhBgmFile.filename);
}
[[nodiscard]] inline std::vector<FhSample> read_fh_menu_sfx(
    const std::filesystem::path& root) {
    return read_fh_sound_file(root, FhMenuSfxFile.filename);
}
[[nodiscard]] inline std::vector<FhSample> read_fh_global_sfx(
    const std::filesystem::path& root) {
    return read_fh_sound_file(root, FhGlobalSfxFile.filename);
}

// FhSound.ExportFhSfx / ExportFhBgm / ExportFhMenuSfx / ExportFhGlobalSfx
std::size_t export_fh_sound_file(const std::filesystem::path& fh_root,
                                 const std::filesystem::path& destination,
                                 const FhSoundFile& file,
                                 bool adpcm_rounding_error = false);

// FhSound.ExportAllFh
std::size_t export_all_fh(const std::filesystem::path& fh_root,
                          const std::filesystem::path& destination,
                          bool adpcm_rounding_error = false);

} // namespace fruityprime::sound
