#include "Formats/sound_export.hpp"
#include <chrono>
#include "Assets/nds_rom.hpp"
#include "Assets/game_assets.hpp"
#include "GameState.hpp"
#include "Sound/music.hpp"
#include "Sound/music_runtime.hpp"
#include "Sound/sdat.hpp"
#include "Formats/sound_catalog.hpp"
#include "Sound/sound_resources.hpp"
#include "Sound/sseq_player.hpp"
#include "Sound/software_mixer.hpp"
#include "Mods/Sound/sfx_mixer.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool has_music_command(
    const std::vector<fruityprime::sound::MusicCommand>& commands,
    fruityprime::sound::MusicCommandType type, std::int32_t sequence = -1) {
    return std::any_of(commands.begin(), commands.end(),
                       [type, sequence](const auto& command) {
                           return command.type == type
                               && (sequence < 0
                                   || command.sequence_id == sequence);
                       });
}

void put16(std::vector<std::uint8_t>& bytes, std::size_t offset,
           std::uint16_t value) {
    bytes.at(offset) = static_cast<std::uint8_t>(value);
    bytes.at(offset + 1) = static_cast<std::uint8_t>(value >> 8);
}

void put32(std::vector<std::uint8_t>& bytes, std::size_t offset,
           std::uint32_t value) {
    bytes.at(offset) = static_cast<std::uint8_t>(value);
    bytes.at(offset + 1) = static_cast<std::uint8_t>(value >> 8);
    bytes.at(offset + 2) = static_cast<std::uint8_t>(value >> 16);
    bytes.at(offset + 3) = static_cast<std::uint8_t>(value >> 24);
}

void put_tag(std::vector<std::uint8_t>& bytes, std::size_t offset,
             const char* tag) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes.at(offset + i) = static_cast<std::uint8_t>(tag[i]);
    }
}

void standard_header(std::vector<std::uint8_t>& bytes, const char* tag) {
    put_tag(bytes, 0, tag);
    put32(bytes, 4, static_cast<std::uint32_t>(bytes.size()));
    put16(bytes, 0x0C, 0x10);
    put16(bytes, 0x0E, 1);
    put32(bytes, 4, 0x0100FEFF);
    put32(bytes, 8, static_cast<std::uint32_t>(bytes.size()));
}

std::vector<std::uint8_t> make_sseq() {
    std::vector<std::uint8_t> bytes(0x20, 0);
    standard_header(bytes, "SSEQ");
    put_tag(bytes, 0x10, "DATA");
    put32(bytes, 0x14, 0x10);
    put32(bytes, 0x18, 0x1C);
    bytes[0x1C] = 0x90;
    bytes[0x1D] = 0x3C;
    bytes[0x1E] = 0x7F;
    bytes[0x1F] = 0xFF;
    return bytes;
}

std::vector<std::uint8_t> make_swav(fruityprime::sound::WaveType type) {
    std::vector<std::uint8_t> bytes(0x14, 0);
    bytes[0] = static_cast<std::uint8_t>(type);
    bytes[1] = 1;
    put16(bytes, 2, 32768);
    put16(bytes, 4, 1000);
    put16(bytes, 6, 1);
    put32(bytes, 8, 1);
    if (type == fruityprime::sound::WaveType::Pcm8) {
        bytes[0x0C] = 0x80;
        bytes[0x0D] = 0x00;
        bytes[0x0E] = 0x7F;
    } else if (type == fruityprime::sound::WaveType::Pcm16) {
        put16(bytes, 0x0C, 0x8000);
        put16(bytes, 0x0E, 0x7FFF);
    } else {
        // Four compressed bytes follow the two-word IMA decoder header.
        put16(bytes, 0x0C, 0);
        put16(bytes, 0x0E, 0);
        std::fill(bytes.begin() + 0x10, bytes.end(), 0);
    }
    return bytes;
}

std::vector<std::uint8_t> make_swar() {
    const auto swav = make_swav(fruityprime::sound::WaveType::Pcm8);
    std::vector<std::uint8_t> bytes(0x40 + swav.size(), 0);
    standard_header(bytes, "SWAR");
    put_tag(bytes, 0x10, "DATA");
    put32(bytes, 0x14, static_cast<std::uint32_t>(0x2C + 4 + swav.size()));
    put32(bytes, 0x38, 1);
    put32(bytes, 0x3C, 0x40);
    std::copy(swav.begin(), swav.end(), bytes.begin() + 0x40);
    return bytes;
}

std::vector<std::uint8_t> make_sbnk() {
    std::vector<std::uint8_t> bytes(0x4C, 0);
    standard_header(bytes, "SBNK");
    put_tag(bytes, 0x10, "DATA");
    put32(bytes, 0x14, 0x3A);
    put32(bytes, 0x38, 1);
    bytes[0x3C] = 1;
    put16(bytes, 0x3D, 0x40);
    put16(bytes, 0x40, 0);
    put16(bytes, 0x42, 0);
    bytes[0x44] = 60;
    bytes[0x45] = 127;
    bytes[0x46] = 4;
    bytes[0x47] = 100;
    bytes[0x48] = 5;
    bytes[0x49] = 64;
    return bytes;
}

std::vector<std::uint8_t> make_sample_table() {
    std::vector<std::uint8_t> bytes(0x34, 0);
    put32(bytes, 0, 2);
    put32(bytes, 4, 0x0C);
    put32(bytes, 8, 0x20);

    bytes[0x0C] = 0;
    bytes[0x0D] = 1;
    put16(bytes, 0x0E, 22050);
    put16(bytes, 0x10, 0x1234);
    put16(bytes, 0x12, 0);
    put32(bytes, 0x14, 2);
    for (std::size_t i = 0; i < 8; ++i) {
        bytes[0x18 + i] = static_cast<std::uint8_t>(i);
    }

    bytes[0x20] = 2;
    bytes[0x21] = 0;
    put16(bytes, 0x22, 16000);
    put16(bytes, 0x24, 0);
    put16(bytes, 0x26, 1);
    put32(bytes, 0x28, 1);
    put16(bytes, 0x2C, 0);
    put16(bytes, 0x2E, 0);
    return bytes;
}

std::vector<std::uint8_t> make_fh_sound_file() {
    constexpr std::size_t first_offset = 16;
    constexpr std::size_t first_data_size = 5;
    constexpr std::size_t second_offset = first_offset + 24 + first_data_size;
    constexpr std::size_t second_data_size = 8;
    std::vector<std::uint8_t> bytes(second_offset + 24 + second_data_size, 0);
    put32(bytes, 0, 3);
    put32(bytes, 4, static_cast<std::uint32_t>(first_offset));
    put32(bytes, 8, static_cast<std::uint32_t>(second_offset));
    put32(bytes, 12, 0);

    put32(bytes, first_offset, static_cast<std::uint32_t>(first_data_size));
    put32(bytes, first_offset + 4, 0);
    put32(bytes, first_offset + 8, 22050);
    put16(bytes, first_offset + 12, 96);
    bytes[first_offset + 14] = 0xA5;
    bytes[first_offset + 15] = 0;
    put32(bytes, first_offset + 16, 1);
    put32(bytes, first_offset + 20, 4);
    bytes[first_offset + 24] = 0x80;
    bytes[first_offset + 25] = 0xC0;
    bytes[first_offset + 26] = 0x00;
    bytes[first_offset + 27] = 0x40;
    bytes[first_offset + 28] = 0x7F;

    put32(bytes, second_offset, static_cast<std::uint32_t>(second_data_size));
    put32(bytes, second_offset + 4, 0);
    put32(bytes, second_offset + 8, 16000);
    put16(bytes, second_offset + 12, 127);
    bytes[second_offset + 14] = 0;
    bytes[second_offset + 15] = 4;
    put32(bytes, second_offset + 16, 1);
    put32(bytes, second_offset + 20, 2);
    // One IMA decoder header followed by four compressed bytes.
    put16(bytes, second_offset + 24, 0);
    put16(bytes, second_offset + 26, 0);
    std::fill(bytes.begin() + second_offset + 28, bytes.end(), 0);
    return bytes;
}

std::vector<std::uint8_t> make_stream() {
    std::vector<std::uint8_t> bytes(0x74, 0);
    standard_header(bytes, "STRM");
    put_tag(bytes, 0x10, "HEAD");
    put32(bytes, 0x14, 0x50);
    bytes[0x18] = 0;
    bytes[0x19] = 1;
    bytes[0x1A] = 2;
    put16(bytes, 0x1C, 32000);
    put16(bytes, 0x1E, 1);
    put32(bytes, 0x20, 2);
    put32(bytes, 0x24, 6);
    put32(bytes, 0x28, 0x68);
    put32(bytes, 0x2C, 2);
    put32(bytes, 0x30, 4);
    put32(bytes, 0x34, 4);
    put32(bytes, 0x38, 2);
    put32(bytes, 0x3C, 2);
    // Full blocks are interleaved by channel, followed by the final block
    // for each channel, matching the native game's stream layout.
    bytes[0x68] = 0x80;
    bytes[0x69] = 0x81;
    bytes[0x6A] = 0x82;
    bytes[0x6B] = 0x83;
    bytes[0x6C] = 0x90;
    bytes[0x6D] = 0x91;
    bytes[0x6E] = 0x92;
    bytes[0x6F] = 0x93;
    bytes[0x70] = 0x84;
    bytes[0x71] = 0x85;
    bytes[0x72] = 0x94;
    bytes[0x73] = 0x95;
    return bytes;
}

std::vector<std::uint8_t> make_select_list() {
    std::vector<std::uint8_t> bytes(4 + 2 * 16 + 8, 0);
    put32(bytes, 0, 2);
    put16(bytes, 4, 7);
    put16(bytes, 6, 1);
    put32(bytes, 8, 0x11);
    put32(bytes, 12, 0x22);
    put32(bytes, 16, 0x33);
    put16(bytes, 20, 9);
    put16(bytes, 22, 3);
    put32(bytes, 24, 0x44);
    put32(bytes, 28, 0x55);
    put32(bytes, 32, 0x66);
    const std::string names("bgm\0sfx\0", 8);
    std::copy(names.begin(), names.end(), bytes.begin() + 36);
    return bytes;
}

std::vector<std::uint8_t> make_sound_3d_list() {
    std::vector<std::uint8_t> bytes(12, 0);
    put32(bytes, 0, 1);
    put32(bytes, 4, 100);
    put32(bytes, 8, 400);
    return bytes;
}

std::vector<std::uint8_t> make_sound_tables() {
    std::vector<std::uint8_t> bytes(54, 0);
    put32(bytes, 0, 2);
    put16(bytes, 4, 0);
    bytes[6] = 0;
    bytes[7] = 2;
    bytes[8] = 100;
    bytes[9] = 5;
    put16(bytes, 10, 12);
    put32(bytes, 12, 0x1234);
    put16(bytes, 16, 0xFFFF);
    bytes[18] = 1;
    bytes[19] = 1;
    bytes[20] = 80;
    bytes[21] = 4;
    put16(bytes, 22, 8);
    put32(bytes, 24, 0x5678);
    const std::string names("laser\0idle\0", 11);
    const std::string categories("weapon\0ambient\0", 15);
    std::copy(names.begin(), names.end(), bytes.begin() + 28);
    std::copy(categories.begin(), categories.end(), bytes.begin() + 39);
    return bytes;
}

std::vector<std::uint8_t> make_assign_music() {
    std::vector<std::uint8_t> bytes(12, 0);
    put32(bytes, 0, 1);
    put16(bytes, 4, 0x123);
    put16(bytes, 6, 1);
    put16(bytes, 8, 2);
    put16(bytes, 10, 0xFFFF);
    return bytes;
}

std::vector<std::uint8_t> make_inter_music_info() {
    std::vector<std::uint8_t> bytes(12, 0);
    put32(bytes, 0, 1);
    put16(bytes, 4, 0xFFFF);
    put16(bytes, 6, 10);
    put16(bytes, 8, 3);
    put16(bytes, 10, 20);
    return bytes;
}

std::vector<std::uint8_t> make_sfx_script_files() {
    constexpr std::size_t blob = 0x20;
    std::vector<std::uint8_t> bytes(blob + 4 + 12 + 5, 0);
    put32(bytes, 0, 1);
    put32(bytes, 4, static_cast<std::uint32_t>(blob));
    put16(bytes, 8, 16);
    bytes[10] = 96;
    bytes[11] = 2;
    put32(bytes, blob, 1);
    put16(bytes, blob + 4, 17);
    put16(bytes, blob + 6, 30);
    bytes[blob + 8] = 100;
    bytes[blob + 9] = 127;
    put16(bytes, blob + 10, 0x200);
    put32(bytes, blob + 12, 0);
    const std::string name("shot\0", 5);
    std::copy(name.begin(), name.end(), bytes.begin() + blob + 16);
    return bytes;
}

std::vector<std::uint8_t> make_dgn_files() {
    constexpr std::size_t blob = 0x20;
    constexpr std::size_t data_offset = 0x28;
    std::vector<std::uint8_t> bytes(blob + 0x38 + 5, 0);
    put32(bytes, 0, 1);
    put32(bytes, 4, static_cast<std::uint32_t>(blob));
    put16(bytes, 8, 0x38);
    bytes[10] = 80;
    bytes[11] = 3;
    put32(bytes, blob, 1);
    put16(bytes, blob + 4, 0);
    put16(bytes, blob + 6, 29);
    for (std::size_t group = 0; group < 4; ++group) {
        const std::size_t entry = blob + 8 + group * 8;
        put32(bytes, entry, 1);
        put32(bytes, entry + 4,
              static_cast<std::uint32_t>(data_offset + group * 4));
        const std::size_t data = blob + data_offset + group * 4;
        put16(bytes, data, static_cast<std::uint16_t>(group + 1));
        put16(bytes, data + 2, static_cast<std::uint16_t>(0x4000 + group));
    }
    const std::string name("door\0", 5);
    std::copy(name.begin(), name.end(), bytes.begin() + blob + 0x38);
    return bytes;
}

bool has_tag(std::span<const std::uint8_t> bytes, const char* tag) {
    if (bytes.size() < 4) {
        return false;
    }
    for (std::size_t i = 0; i < 4; ++i) {
        if (bytes[i] != static_cast<std::uint8_t>(tag[i])) {
            return false;
        }
    }
    return true;
}

std::filesystem::path configured_rom(int argc, char** argv) {
    if (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0') {
        return argv[1];
    }
    if (const char* value = std::getenv("FRUITY_PRIME_TEST_NDS");
        value != nullptr && value[0] != '\0') {
        return value;
    }
    return {};
}

std::optional<fruityprime::nds::FileEntry> find_file(
    const fruityprime::nds::Rom& rom, std::string target) {
    std::transform(target.begin(), target.end(), target.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    for (const auto& entry : rom.files()) {
        std::string path = entry.path;
        std::transform(path.begin(), path.end(), path.begin(),
                       [](unsigned char value) {
                           return static_cast<char>(std::tolower(value));
                       });
        if (path == target || (path.size() > target.size()
                               && path.ends_with("/" + target))) {
            return entry;
        }
    }
    return std::nullopt;
}

void synthetic_tests() {
    const auto sequence = fruityprime::sound::Sseq::parse(make_sseq(), 7,
                                                           "fixture");
    require(sequence.file_id == 7 && sequence.name == "fixture",
            "SSEQ metadata was not retained");
    require(sequence.data.size() == 4 && sequence.data[1] == 0x3C,
            "SSEQ DATA block was not decoded");

    const auto bank = fruityprime::sound::Sbnk::parse(make_sbnk(), 3, "bank");
    require(bank.entries.size() == 1 && bank.entries[0].record == 1,
            "SBNK entry header was not decoded");
    require(bank.entries[0].instruments.size() == 1
                && bank.entries[0].instruments[0].note_number == 60,
            "SBNK instrument was not decoded");

    const auto archive = fruityprime::sound::Swar::parse(make_swar(), 4,
                                                          "waves");
    require(archive.waves.size() == 1 && archive.waves[0].has_value(),
            "SWAR wave table was not decoded");
    const auto pcm8 = archive.waves[0]->decode_pcm();
    require(pcm8.size() == 8 && pcm8.front() == -32768,
            "SWAV PCM8 decoding was not converted to signed PCM");

    auto pcm16_bytes = make_swav(fruityprime::sound::WaveType::Pcm16);
    const auto pcm16 = fruityprime::sound::Swav::parse(pcm16_bytes).decode_pcm();
    require(pcm16.size() == 4 && pcm16.front() == -32768
                && pcm16[1] == 32767,
            "SWAV PCM16 decoding was not converted to signed PCM");

    auto adpcm_bytes = make_swav(fruityprime::sound::WaveType::ImaAdpcm);
    const auto adpcm = fruityprime::sound::Swav::parse(adpcm_bytes).decode_pcm();
    require(adpcm.size() == 8, "SWAV IMA-ADPCM sample count is wrong");

    const auto samples = fruityprime::sound::parse_sample_table(
        make_sample_table());
    require(samples.size() == 2 && samples[0].present && samples[1].present,
            "custom sound sample table slots were not retained");
    require(samples[0].decode_pcm().size() == 8
                && samples[1].decode_pcm().size() == 8,
            "custom PCM8/ADPCM samples were not decoded");

    const auto fh_samples = fruityprime::sound::parse_fh_sound_file(
        make_fh_sound_file());
    require(fh_samples.size() == 3 && fh_samples[0].present
                && fh_samples[1].present && !fh_samples[2].present,
            "First Hunt sound slots were not retained");
    require(fh_samples[0].format == fruityprime::sound::WaveFormat::Pcm8
                && fh_samples[0].sample_rate == 22050
                && fh_samples[0].loop_start == 1
                && fh_samples[0].decode_pcm().size() == 4,
            "First Hunt PCM8 sound was not decoded");
    require(fh_samples[1].format == fruityprime::sound::WaveFormat::Adpcm
                && fh_samples[1].decode_pcm().size() == 8,
            "First Hunt ADPCM sound was not decoded");

    const auto stream = fruityprime::sound::Stream::parse(
        make_stream(), 9, "fixture-stream");
    require(stream.id == 9 && stream.name == "fixture-stream"
                && stream.channels.size() == 2,
            "STRM metadata was not decoded");
    require(stream.channels[0].size() == 6 && stream.channels[1].size() == 6,
            "STRM block layout was not decoded");
    const auto interleaved = stream.interleaved_data();
    require(interleaved.size() == 12 && interleaved[0] == 0
                && interleaved[1] == 0x10,
            "STRM channels were not interleaved");

    bool rejected = false;
    try {
        auto malformed = make_sseq();
        malformed.resize(0x1D);
        (void)fruityprime::sound::Sseq::parse(malformed);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected, "truncated SSEQ was accepted");

    const auto select = fruityprime::sound::parse_select_list(
        make_select_list());
    require(select.size() == 2 && select[0].name == "bgm"
                && select[1].field_c == 0x66,
            "sound select list was not decoded");

    const auto sound_3d = fruityprime::sound::parse_sound_3d_list(
        make_sound_3d_list());
    require(sound_3d.size() == 1 && sound_3d[0].max_distance == 400,
            "3D sound list was not decoded");

    const auto tables = fruityprime::sound::parse_sound_tables(
        make_sound_tables());
    require(tables.entries.size() == 2, "sound table entry count is wrong");
    require(tables.categories.size() == 2,
            "sound table category count is wrong");
    require(tables.entries[1].category == "ambient",
            "sound table category name is wrong");
    require(tables.entries[0].data == 0x1234,
            "sound table data field is wrong");

    const auto room_music = fruityprime::sound::parse_assign_music(
        make_assign_music());
    require(room_music.size() == 1 && room_music[0].track_ids[2] == 0xFFFF,
            "room music assignments were not decoded");

    const auto inter_music = fruityprime::sound::parse_inter_music_info(
        make_inter_music_info());
    require(inter_music.size() == 1 && !inter_music[0].sequence_id.has_value()
                && inter_music[0].tracks == 3,
            "intermission music table was not decoded");

    const auto scripts = fruityprime::sound::parse_sfx_script_files(
        make_sfx_script_files());
    require(scripts.size() == 1 && scripts[0].name == "shot"
                && scripts[0].entries.size() == 1
                && scripts[0].entries[0].sfx_id == 17,
            "SFX script file was not decoded");

    const auto dgn = fruityprime::sound::parse_dgn_files(make_dgn_files());
    require(dgn.size() == 1 && dgn[0].name == "door"
                && dgn[0].entries.size() == 1
                && dgn[0].entries[0].data[3][0].value == 0x4003,
            "DGN file was not decoded");
}

void software_mixer_tests() {
    fruityprime::sound::SoftwareMixer mixer(22050);
    const auto buffer = mixer.create_buffer();
    const std::vector<std::uint8_t> mono8{128, 192, 255, 128};
    require(mixer.fill_buffer(buffer,
                              fruityprime::sound::AudioBufferFormat::Mono8,
                              mono8, 22050),
            "software mixer rejected PCM8");

    const auto source = mixer.create_source();
    mixer.set_buffer(source, buffer);
    mixer.play(source);
    require(mixer.source_state(source)
                == fruityprime::sound::AudioSourceState::Playing,
            "software mixer did not start a source");
    std::vector<float> output(8, 0.0F);
    mixer.mix_stereo(output);
    require(std::abs(output[0]) < 0.0001F && output[2] > 0.45F
                && output[3] > 0.45F && output[4] > 0.9F,
            "software mixer PCM8 output is wrong");
    mixer.mix_stereo(output);
    require(mixer.source_state(source)
                == fruityprime::sound::AudioSourceState::Stopped,
            "software mixer did not stop at the end of a buffer");

    const auto loop_source = mixer.create_source();
    mixer.set_buffer(loop_source, buffer);
    mixer.set_loop_points(buffer, 1, 3);
    mixer.set_looping(loop_source, true);
    mixer.play(loop_source);
    std::vector<float> loop_output(16, 0.0F);
    mixer.mix_stereo(loop_output);
    require(mixer.source_state(loop_source)
                == fruityprime::sound::AudioSourceState::Playing
                && loop_output[6] > 0.45F && loop_output[8] > 0.45F,
            "software mixer loop points are wrong");
    mixer.pause(loop_source);
    require(mixer.source_state(loop_source)
                == fruityprime::sound::AudioSourceState::Paused,
            "software mixer did not pause");
    mixer.play(loop_source);
    mixer.stop(loop_source);
    require(mixer.source_state(loop_source)
                == fruityprime::sound::AudioSourceState::Stopped,
            "software mixer did not stop");

    const auto stereo_buffer = mixer.create_buffer();
    const std::vector<std::uint8_t> stereo16{
        0x00, 0x40, 0x00, 0xC0, 0x00, 0x20, 0x00, 0xE0
    };
    require(mixer.fill_buffer(
                stereo_buffer,
                fruityprime::sound::AudioBufferFormat::Stereo16,
                stereo16, 22050),
            "software mixer rejected stereo PCM16");
    const auto spatial_source = mixer.create_source();
    mixer.set_buffer(spatial_source, stereo_buffer);
    mixer.set_relative(spatial_source, true);
    mixer.set_position(spatial_source, {10.0F, 0.0F, 0.0F});
    mixer.set_reference_distance(spatial_source, 1.0F);
    mixer.set_max_distance(spatial_source, 11.0F);
    mixer.play(spatial_source);
    std::vector<float> spatial_output(4, 0.0F);
    mixer.mix_stereo(spatial_output);
    require(std::abs(spatial_output[0]) < 0.001F
                && std::abs(spatial_output[1]) > 0.01F,
            "software mixer stereo placement is wrong");
}

void sseq_player_tests() {
    auto sequence = fruityprime::sound::Sseq{
        "fixture-sequence", 7,
        {
            0xFE, 0x02, 0x00,       // Allocate logical track 1.
            0x93, 0x01, 0x0B, 0x00, 0x00, // Open track 1 at offset 0x0B.
            0x80, 0x02,             // Track 0 waits two ticks.
            0xFF,                   // Track 0 ends.
            0x81, 0x00,             // Program 0.
            0x3C, 0x7F, 0x04,       // C4, velocity 127, four ticks.
            0xFF                    // Track 1 ends.
        }
    };
    auto bank = fruityprime::sound::Sbnk{
        "fixture-bank", 3,
        {fruityprime::sound::InstrumentEntry{
            1, 0x40,
            {fruityprime::sound::Instrument{
                0, 127, 1, 0, 0, 60, 255, 255, 255, 255, 64
            }}
        }}
    };
    auto archive = fruityprime::sound::Swar::parse(make_swar(), 4, "waves");
    fruityprime::sound::SseqPlayer player(
        std::move(sequence), std::move(bank), {std::move(archive)});

    const auto timeline = player.timeline(32, 32);
    require(timeline.stats.ended && !timeline.stats.truncated,
            "SSEQ timeline did not finish its track graph");
    require(timeline.stats.commands >= 6 && timeline.notes.size() == 1,
            "SSEQ timeline did not execute OpenTrack and note commands");
    require(timeline.notes.front().track == 1
                && timeline.notes.front().key == 60
                && timeline.notes.front().duration_ticks == 4,
            "SSEQ note event fields are wrong");

    const auto audio = player.render(0.1, fruityprime::sound::SseqRenderOptions{
        32728, 1.0F, 32, 32
    });
    require(audio.size() == 6546,
            "SSEQ renderer returned the wrong stereo frame count");
    require(std::any_of(audio.begin(), audio.end(), [](float sample) {
        return std::abs(sample) > 0.001F;
    }), "SSEQ PCM voice rendered silence");
    const auto masked_audio = player.render(
        0.1, fruityprime::sound::SseqRenderOptions{32728, 1.0F, 32, 32, 1});
    require(std::none_of(masked_audio.begin(), masked_audio.end(),
                         [](float sample) {
                             return std::abs(sample) > 0.001F;
                         }),
            "SSEQ track mask did not silence the excluded track");

    const auto control_sequence = fruityprime::sound::Sseq{
        "control-sequence", 8,
        {
            0xB0, 0x00, 0x01, 0x00, // variable 0 = 1
            0xB8, 0x00, 0x01, 0x00, // compare variable 0 == 1
            0xA2, 0x81, 0x00,       // conditional program 0
            0xE1, 0xF0, 0x00,       // tempo 240
            0xD4, 0x02,             // loop twice
            0x3E, 0x7F, 0x01,       // D4, one tick
            0xFC, 0xFF
        }
    };
    fruityprime::sound::SseqPlayer control_player(
        control_sequence, fruityprime::sound::Sbnk::parse(make_sbnk()),
        {fruityprime::sound::Swar::parse(make_swar())});
    const auto control_timeline = control_player.timeline(32, 32);
    require(control_timeline.stats.ended
                && control_timeline.stats.final_tempo == 240
                && control_timeline.notes.size() == 2,
            "SSEQ variable/conditional/tempo/loop commands are wrong");
}

void music_controller_tests() {
    fruityprime::sound::Catalog catalog;
    catalog.music_tracks.resize(57);
    auto set_track = [&catalog](std::size_t id, std::uint16_t sequence,
                                std::uint16_t tracks) {
        catalog.music_tracks[id] = fruityprime::sound::MusicTrack{
            static_cast<std::uint32_t>(id), sequence, 6, tracks, 9};
    };
    set_track(0, 10, 0x0001);
    set_track(3, 30, 0x021f);
    set_track(18, 40, 0x0003);
    set_track(51, 51, 0x0001);
    set_track(55, 55, 0x0001);
    set_track(56, 56, 0x0001);
    catalog.room_music.push_back(fruityprime::sound::RoomMusic{
        0x123, {0, 3, 18}});

    fruityprime::sound::MusicController music(catalog);
    music.init();
    require(!music.snapshot().playing && music.volume() == 1.0F,
            "music controller did not initialize its state");

    // Music.cs owns one process-wide state. Property assignment and the
    // clamping SetUserVolume method intentionally have different effects.
    fruityprime::sound::Music::BindRuntime(nullptr);
    fruityprime::sound::Music::UserVolume(1.25F);
    fruityprime::game::State music_state;
    fruityprime::sound::Music::BindRuntime(&music, &music_state);
    require(fruityprime::sound::Music::UserVolume() == 1.25F
                && music.snapshot().user_volume == 1.25F,
            "Music.UserVolume property did not retain its process value");
    fruityprime::sound::Music::SetUserVolume(0.5F);
    require(std::abs(fruityprime::sound::Music::Volume() - 0.5F) < 0.0001F,
            "Music.SetUserVolume did not clamp and apply its value");
    fruityprime::sound::Music::Init();
    require(music.snapshot().user_volume == 0.5F,
            "Music.Init incorrectly reset the user's volume");

    music_state.escape_timer = 20.0F;
    music_state.escape_state = fruityprime::game::EscapeState::Escape;
    fruityprime::sound::Music::TryPlayRoomMusic(0x123, 1);
    require(music.snapshot().current_music_id == 0,
            "Music.TryPlayRoomMusic ignored the escape-state guard");
    music_state.escape_state = fruityprime::game::EscapeState::None;
    fruityprime::sound::Music::TryPlayRoomMusic(0x123, 1);
    require(music.snapshot().current_music_id == 3,
            "Music.TryPlayRoomMusic did not resolve an allowed room track");
    fruityprime::sound::Music::UpdateMusic();
    fruityprime::sound::Music::Pause();
    fruityprime::sound::Music::UpdateMusicIdIfPaused(
        fruityprime::formats::MusicId::SEQ_GUARDIAN_M18);
    require(music.snapshot().current_music_id == 18,
            "Music.UpdateMusicIdIfPaused did not update a paused track");
    fruityprime::sound::Music::MusicToResume(
        fruityprime::formats::MusicId::SEQ_GUMBO_M3);
    require(fruityprime::sound::Music::MusicToResume()
                == fruityprime::formats::MusicId::SEQ_GUMBO_M3,
            "Music.MusicToResume did not preserve its static property");

    music.init();
    music.play_music(51);
    music.update(0.0F);
    music.update_event_music(20.0F);
    music.update(1.0F / 30.0F);
    require(music.snapshot().tempo == 384,
            "UpdateEventMusic did not interpret its argument as seconds");
    music.update_tempo(307, 0.0F);
    require(music.snapshot().tempo == 307,
            "UpdateTempo zero-time update was not immediate");

    music.init();
    music.set_user_volume(0.5F);
    require(std::abs(music.volume() - 0.5F) < 0.0001F,
            "music user volume was not applied");
    music.play_music(3);
    auto snapshot = music.snapshot();
    require(snapshot.queued && !snapshot.playing
                && snapshot.current_music_id == 3,
            "music track did not enter the queued state");
    auto commands = music.take_commands();
    (void)commands;

    music.update(0.0F);
    snapshot = music.snapshot();
    require(snapshot.playing && !snapshot.queued
                && snapshot.current_sequence_id == 30
                && snapshot.active_tracks == 0x021f,
            "queued music did not start on update");
    commands = music.take_commands();
    require(has_music_command(
                commands, fruityprime::sound::MusicCommandType::StartSequence,
                30)
                && has_music_command(
                    commands, fruityprime::sound::MusicCommandType::SetTempo,
                    30),
            "music start commands were not emitted");

    music.fade_volume(0.0F, 1.0F, true);
    music.update(0.5F);
    require(music.snapshot().music_volume > 0.45F
                && music.snapshot().music_volume < 0.55F,
            "music volume fade did not interpolate");
    music.update(0.5F);
    snapshot = music.snapshot();
    commands = music.take_commands();
    require(has_music_command(commands,
                                     fruityprime::sound::MusicCommandType::Stop),
            "music volume fade did not stop at its target");

    music.init();
    music.play_room_music(0x123, 2);
    music.update(0.0F);
    require(music.snapshot().current_music_id == 18
                && music.snapshot().current_sequence_id == 40,
            "room music did not resolve its track table");

    music.init();
    music.play_music(3);
    music.update(0.0F);
    (void)music.take_commands();
    music.play_encounter(fruityprime::metadata::Hunter::Kanden);
    snapshot = music.snapshot();
    require((snapshot.encounter_suspension & (1 << 1)) != 0,
            "hunter encounter music did not set its suspension bit");
    music.update_encounter(1);
    require((music.snapshot().encounter_suspension & (1 << 1)) == 0,
            "hunter encounter music did not clear its suspension bit");

    music.init();
    music.play_sequence(52, 0x0001, false, false);
    music.update_escape_music(1700);
    require(music.snapshot().current_music_id == 56,
            "escape music did not switch at the one-minute threshold");

    music.init();
    music.play_sequence(52, 0x0001, false, false);
    music.update_escape_music(2000);
    music.update(1.0F / 30.0F);
    require(music.snapshot().tempo >= 280 && music.snapshot().tempo <= 282,
            "escape music tempo ramp was not applied");

    fruityprime::mods::sound::SfxMixer player_mixer(32728);
    fruityprime::sound::MusicRuntime player_runtime(
        catalog, music, player_mixer);
    fruityprime::sound::MusicPlayer::BindRuntime(&player_runtime);
    const auto format = fruityprime::sound::MusicPlayer::Format();
    require(fruityprime::sound::MusicPlayer::Available()
                && !fruityprime::sound::MusicPlayer::Loading()
                && format.sample_rate == 32728 && format.channels == 2,
            "MusicPlayer static runtime/format contract is wrong");
    fruityprime::sound::MusicPlayer::Volume(0.25F);
    fruityprime::sound::MusicPlayer::Tempo(384);
    require(std::abs(fruityprime::sound::MusicPlayer::Volume() - 0.25F)
                < 0.0001F
                && fruityprime::sound::MusicPlayer::Tempo() == 384
                && fruityprime::sound::MusicPlayer::State()
                    == fruityprime::sound::PlaybackState::Stopped,
            "MusicPlayer volume/tempo/state contract is wrong");
    fruityprime::sound::MusicPlayer::Remove();
    fruityprime::sound::MusicPlayer::BindRuntime(nullptr);
    fruityprime::sound::Music::BindRuntime(nullptr);
}

void real_rom_tests(const std::filesystem::path& path) {
    if (path.empty()) {
        std::cout << "sound ROM test skipped: set FRUITY_PRIME_TEST_NDS\n";
        return;
    }
    require(std::filesystem::is_regular_file(path),
            "configured NDS ROM is not a regular file: " + path.string());
    const auto rom = fruityprime::nds::Rom::read_file(path);
    std::size_t sdat_count = 0;
    std::size_t sequence_count = 0;
    std::size_t bank_count = 0;
    std::size_t wave_archive_count = 0;
    std::size_t decoded_wave_count = 0;
    std::size_t stream_count = 0;
    bool sseq_timeline_smoke = false;
    bool sseq_audio_smoke = false;

    for (const auto& file : rom.files()) {
        const auto bytes = rom.file(file.file_id);
        if (!has_tag(bytes, "SDAT")) {
            continue;
        }
        ++sdat_count;
        const auto sdat = fruityprime::sound::Sdat::parse(bytes);
        require(!sdat.fat_records().empty(),
                "real SDAT has no FAT records");

        // The SEQARC symbol record names the sequences packed inside each
        // sequence archive.  It is the one symbol record whose entries are a
        // pair of offsets rather than a single name offset, so a parser that
        // walks it like the others reads garbage rather than failing.
        const auto& archives = sdat.sequence_archive_names();
        require(!archives.empty(),
                "real SDAT has no sequence archives");
        std::size_t named_archives = 0;
        std::size_t archived_files = 0;
        for (const auto& archive : archives) {
            if (!archive.name.empty()) {
                ++named_archives;
            }
            archived_files += archive.file_names.size();
            for (const auto& file_name : archive.file_names) {
                require(file_name.find('\0') == std::string::npos,
                        "sequence archive file name is not terminated");
            }
        }
        require(named_archives > 0,
                "real SDAT sequence archives are all unnamed");
        require(archived_files > 0,
                "real SDAT sequence archives contain no sequences");

        for (std::size_t i = 0; i < sdat.sequences().size(); ++i) {
            if (!sdat.sequences()[i].present) {
                continue;
            }
            const auto sequence = sdat.sequence(i);
            require(!sequence.data.empty(),
                    "real SDAT contains an empty SSEQ payload");
            ++sequence_count;

            if (!sseq_timeline_smoke
                && sdat.sequences()[i].bank < sdat.banks().size()
                && sdat.banks()[sdat.sequences()[i].bank].present) {
                fruityprime::sound::SseqPlayer player(sdat, i);
                const auto timeline = player.timeline(4096, 10000);
                require(timeline.stats.commands > 0,
                        "real SSEQ timeline executed no commands");
                sseq_timeline_smoke = true;
                if (!timeline.notes.empty()) {
                    const auto rendered = player.render(
                        0.1, fruityprime::sound::SseqRenderOptions{
                            22050, 1.0F, 4096, 10000
                        });
                    require(rendered.size() == 4410,
                            "real SSEQ renderer returned a wrong frame count");
                    sseq_audio_smoke = std::any_of(
                        rendered.begin(), rendered.end(), [](float sample) {
                            return std::abs(sample) > 0.0001F;
                        });
                    std::cout << "SSEQ native: index " << i
                              << ", ticks " << timeline.stats.ticks
                              << ", commands " << timeline.stats.commands
                              << ", notes " << timeline.notes.size()
                              << ", rendered " << rendered.size() / 2
                              << " frames\n";
                }
            }
        }
        for (std::size_t i = 0; i < sdat.banks().size(); ++i) {
            if (!sdat.banks()[i].present) {
                continue;
            }
            const auto bank = sdat.bank(i);
            require(!bank.entries.empty(),
                    "real SDAT contains an empty SBNK");
            ++bank_count;
        }
        for (std::size_t i = 0; i < sdat.wave_archives().size(); ++i) {
            if (!sdat.wave_archives()[i].present) {
                continue;
            }
            const auto archive = sdat.wave_archive(i);
            ++wave_archive_count;
            for (const auto& wave : archive.waves) {
                if (!wave.has_value()) {
                    continue;
                }
                const auto pcm = wave->decode_pcm();
                if (!pcm.empty()) {
                    ++decoded_wave_count;
                }
            }
        }

        for (std::size_t i = 0; i < sdat.fat_records().size(); ++i) {
            const auto payload = sdat.file(static_cast<std::uint32_t>(i));
            if (!has_tag(payload, "STRM")) {
                continue;
            }
            const auto stream = fruityprime::sound::Stream::parse(
                payload, static_cast<std::uint32_t>(i));
            require(stream.channels.size() == stream.channel_count,
                    "real SDAT STRM channel count is inconsistent");
            require(!stream.interleaved_data().empty(),
                    "real SDAT STRM has no decoded samples");
            ++stream_count;
        }

        std::cout << "SDAT " << file.path << ": FAT "
                  << sdat.fat_records().size() << ", SEQ "
                  << sdat.sequences().size() << ", BANK "
                  << sdat.banks().size() << ", WAVEARC "
                  << sdat.wave_archives().size() << '\n';
    }

    require(sdat_count > 0, "real ROM contains no SDAT file");
    require(sequence_count > 0, "real SDAT contains no usable SSEQ");
    require(bank_count > 0, "real SDAT contains no usable SBNK");
    require(wave_archive_count > 0,
            "real SDAT contains no usable wave archive");
    require(decoded_wave_count > 0, "real SDAT contains no decodable wave");
    require(sseq_timeline_smoke,
            "real SDAT contains no SSEQ usable by the native timeline");
    require(sseq_audio_smoke,
            "real SDAT contains no audible PCM SSEQ event");
    std::cout << "real SDAT: files " << sdat_count << ", sequences "
              << sequence_count << ", banks " << bank_count
              << ", wave archives " << wave_archive_count
              << ", decoded waves " << decoded_wave_count
              << ", streams " << stream_count << '\n';

    for (const auto table_name : {"SNDSAMPLES.DAT", "WFSSNDSAMPLES.DAT"}) {
        const auto entry = find_file(rom, table_name);
        if (!entry.has_value()) {
            continue;
        }
        const auto samples = fruityprime::sound::parse_sample_table(
            rom.file(entry->file_id));
        std::size_t present = 0;
        std::size_t decoded = 0;
        for (const auto& sample : samples) {
            if (!sample.present) {
                continue;
            }
            ++present;
            if (!sample.decode_pcm().empty()) {
                ++decoded;
            }
        }
        require(present > 0 && decoded > 0,
                std::string("real ") + table_name
                    + " contains no decodable samples");
        std::cout << table_name << ": slots " << samples.size()
                  << ", present " << present << ", decoded " << decoded
                  << '\n';
    }

    const auto catalog = fruityprime::sound::Catalog::load(
        fruityprime::assets::Store::from_rom(path));
    require(catalog.samples.size() > 0 && catalog.wfs_samples.size() > 0,
            "real sound catalog has no sample tables");

    // Sound.ExportWfsSample / ExportWfsSamples.  The WFS table repeats the
    // main table's indices, so the bulk export prefixes its files -- without
    // that, exporting both tables into one directory silently overwrites half
    // of them.  Only a handful are written here; the point is the naming and
    // that a .wav actually comes out.
    const auto export_dir = std::filesystem::temp_directory_path()
        / ("fruity-prime-wfs-export-"
           + std::to_string(std::chrono::steady_clock::now()
                                .time_since_epoch().count()));
    fruityprime::sound::export_wfs_sample(export_dir, catalog.wfs_samples, 0);
    const auto single = export_dir / "0.wav";
    require(std::filesystem::is_regular_file(single),
            "ExportWfsSample wrote no file");
    require(std::filesystem::file_size(single) > 44,
            "ExportWfsSample wrote a header with no samples");

    std::vector<fruityprime::sound::Sample> few(
        catalog.wfs_samples.begin(),
        catalog.wfs_samples.begin() + std::min<std::size_t>(
            3, catalog.wfs_samples.size()));
    const std::size_t written =
        fruityprime::sound::export_wfs_samples(export_dir, few);
    require(written == few.size(),
            "ExportWfsSamples skipped a present sample");
    require(std::filesystem::is_regular_file(export_dir / "mph_wfs_0.wav"),
            "ExportWfsSamples did not use the WFS prefix");

    // An index outside the table writes nothing rather than reading past it.
    fruityprime::sound::export_wfs_sample(export_dir, catalog.wfs_samples,
                                          -1);
    fruityprime::sound::export_wfs_sample(
        export_dir, catalog.wfs_samples,
        static_cast<int>(catalog.wfs_samples.size()));
    std::error_code cleanup;
    std::filesystem::remove_all(export_dir, cleanup);
    require(!catalog.bgm_select.empty() && !catalog.sfx_select.empty(),
            "real sound catalog has no select lists");
    require(!catalog.sound_3d.empty() && !catalog.sound_tables.entries.empty(),
            "real sound catalog has no sound tables");
    require(!catalog.room_music.empty() && !catalog.music_tracks.empty(),
            "real sound catalog has no music tables");
    require(!catalog.sfx_script_files.empty() && !catalog.dgn_files.empty(),
            "real sound catalog has no script/DGN tables");
    require(catalog.sdat.has_value() && !catalog.streams.empty(),
            "real sound catalog has no SDAT streams");
    require(catalog.sdat->streams().size() == catalog.streams.size(),
            "SDAT stream INFO and decoded stream counts differ");
    require(!catalog.streams.front().name.empty(),
            "SDAT stream symbol name was not retained");
    require(catalog.streams.front().volume >= 0.0F
                && catalog.streams.front().volume <= 1.0F,
            "SDAT stream volume was not converted");

    fruityprime::sound::MusicController music(catalog);
    music.init();
    fruityprime::mods::sound::SfxMixer music_mixer(22050);
    require(music_mixer.open(), "music mixer failed to open");
    fruityprime::sound::MusicRuntime music_runtime(
        catalog, music, music_mixer);
    bool music_runtime_smoke = false;
    for (const auto& room : catalog.room_music) {
        for (int track = 0; track < 3; ++track) {
            music.try_play_room_music(room.room_id, track);
            music_runtime.update(0.0F);
            if (music_runtime.active()) {
                music_runtime_smoke = true;
                break;
            }
        }
        if (music_runtime_smoke) {
            break;
        }
    }
    require(music_runtime_smoke && music_runtime.rendered_frames() > 0,
            "real music runtime could not start an SSEQ voice");
    const auto started_sequence = music_runtime.sequence_id();
    const auto rendered_music_frames = music_runtime.rendered_frames();
    music.stop();
    music_runtime.update(0.0F);
    require(!music_runtime.active(), "music runtime did not stop its voice");
    music_mixer.close();
    std::cout << "music runtime: sequence " << started_sequence
              << ", rendered " << rendered_music_frames << " frames\n";
    std::cout << "sound catalog: BGM " << catalog.bgm_select.size()
              << ", SFX " << catalog.sfx_select.size()
              << ", 3D " << catalog.sound_3d.size()
              << ", tables " << catalog.sound_tables.entries.size()
              << ", room music " << catalog.room_music.size()
              << ", tracks " << catalog.music_tracks.size()
              << ", scripts " << catalog.sfx_script_files.size()
              << ", DGN " << catalog.dgn_files.size()
              << ", streams " << catalog.streams.size() << '\n';
}

} // namespace

int main(int argc, char** argv) {
    try {
        synthetic_tests();
        software_mixer_tests();
        sseq_player_tests();
        music_controller_tests();
        real_rom_tests(configured_rom(argc, argv));
        std::cout << "native sound tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native sound tests failed: " << error.what() << '\n';
        return 1;
    }
}
