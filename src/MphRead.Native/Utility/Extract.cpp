#include "Utility/extract.hpp"
#include "Metadata/MetadataFacade.hpp"

#include "Assets/compression.hpp"
#include "Assets/game_assets.hpp"
#include "Assets/nds_rom.hpp"
#include "Formats/paths.hpp"
#include "Strings.hpp"
#include "Utility/archive.hpp"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fruityprime::utility::extract {
namespace {

[[nodiscard]] std::string value_after(int argc, char** argv,
                                      std::string_view flag) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == flag) {
            return argv[i + 1];
        }
    }
    return {};
}

void notify(const SetupReporter& report, const std::string& message) {
    if (report) {
        report(message);
    }
}

[[nodiscard]] bool supported_game(const nds::RomHeader& header,
                                  bool& first_hunt,
                                  std::string& error) {
    first_hunt = false;
    const bool mph = header.game_code == "AMHE"
        || header.game_code == "AMHP" || header.game_code == "AMHJ"
        || header.game_code == "AMHK" || header.game_code == "A76E";
    if (!mph) {
        first_hunt = header.game_code == "AMFE"
            || header.game_code == "AMFP";
        if (!first_hunt) {
            error = "The specified ROM file has invalid game code "
                + header.game_code + ".";
            return false;
        }
    }
    const bool valid_version = first_hunt
        ? header.version == 0
        : ((header.game_code == "AMHE" || header.game_code == "AMHP"
            || header.game_code == "AMHJ")
               ? header.version <= 1
               : header.version == 0);
    if (!valid_version) {
        error = "The specified " + header.game_code
            + " ROM has unexpected version "
            + std::to_string(header.version) + ".";
        return false;
    }
    return true;
}

[[nodiscard]] std::vector<std::uint8_t> read_file(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("Could not open " + path.string());
    }
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw std::runtime_error("Could not determine the size of "
                                 + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("Could not read " + path.string());
        }
    }
    return bytes;
}

void write_file(const std::filesystem::path& path,
                std::span<const std::uint8_t> bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Could not create " + path.string());
    }
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    if (!output) {
        throw std::runtime_error("Could not write " + path.string());
    }
}

[[nodiscard]] std::uint32_t read_u32(
    std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("Overlay table is truncated");
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 8
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 16
        | static_cast<std::uint32_t>(bytes[offset + 3]) << 24;
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24));
}

[[nodiscard]] std::string sequence_number(std::uint32_t value) {
    std::ostringstream stream;
    stream << std::uppercase << std::hex << std::setfill('0')
           << std::setw(4) << value;
    return stream.str();
}

[[nodiscard]] bool tag_at(std::span<const std::uint8_t> bytes,
                          std::size_t offset, std::string_view tag) {
    return offset <= bytes.size() && tag.size() <= bytes.size() - offset
        && std::equal(tag.begin(), tag.end(), bytes.begin() + offset);
}

[[nodiscard]] std::string null_string(
    std::span<const std::uint8_t> bytes, std::size_t offset,
    std::string_view what) {
    if (offset >= bytes.size()) {
        throw std::runtime_error(std::string(what) + " is outside SDAT");
    }
    std::size_t end = offset;
    while (end < bytes.size() && bytes[end] != 0) {
        ++end;
    }
    if (end == bytes.size()) {
        throw std::runtime_error(std::string(what) + " is not terminated");
    }
    return std::string(reinterpret_cast<const char*>(bytes.data() + offset),
                       end - offset);
}

struct SdatSequenceName {
    std::uint32_t index = 0;
    std::string original;
    std::string output;
};

[[nodiscard]] std::vector<SdatSequenceName> sdat_sequence_names(
    std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 0x40 || !tag_at(bytes, 0, "SDAT")) {
        throw std::runtime_error("SDAT header is invalid");
    }
    const std::uint32_t symb_offset = read_u32(bytes, 0x10);
    const std::uint32_t info_offset = read_u32(bytes, 0x18);
    if (info_offset > bytes.size() || bytes.size() - info_offset < 0x28
        || !tag_at(bytes, info_offset, "INFO")) {
        throw std::runtime_error("SDAT INFO section is invalid");
    }
    const std::uint32_t info_record = read_u32(bytes, info_offset + 8);
    if (info_record == 0
        || info_record > bytes.size() - info_offset
        || bytes.size() - info_offset - info_record < 4) {
        throw std::runtime_error("SDAT has no SSEQ INFO record");
    }
    const std::size_t info_base = info_offset + info_record;
    const std::uint32_t count = read_u32(bytes, info_base);
    if (count > (bytes.size() - info_base - 4) / 4) {
        throw std::runtime_error("SDAT SSEQ INFO record is truncated");
    }

    std::size_t symbol_base = 0;
    std::uint32_t symbol_count = 0;
    if (symb_offset != 0) {
        if (symb_offset > bytes.size() || bytes.size() - symb_offset < 0x28
            || !tag_at(bytes, symb_offset, "SYMB")) {
            throw std::runtime_error("SDAT SYMB section is invalid");
        }
        const std::uint32_t symbol_record = read_u32(bytes, symb_offset + 8);
        if (symbol_record != 0) {
            symbol_base = symb_offset + symbol_record;
            if (symbol_base > bytes.size() || bytes.size() - symbol_base < 4) {
                throw std::runtime_error("SDAT SSEQ SYMB record is invalid");
            }
            symbol_count = read_u32(bytes, symbol_base);
            if (symbol_count > (bytes.size() - symbol_base - 4) / 4) {
                throw std::runtime_error("SDAT SSEQ SYMB record is truncated");
            }
        }
    }

    std::vector<SdatSequenceName> names;
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint32_t entry_offset = read_u32(
            bytes, info_base + 4 + static_cast<std::size_t>(index) * 4);
        if (entry_offset == 0) {
            continue;
        }
        const std::size_t entry = info_offset + entry_offset;
        if (entry > bytes.size() || bytes.size() - entry < 0x0C) {
            throw std::runtime_error("SDAT SSEQ INFO entry is truncated");
        }
        const std::uint32_t file_id = read_u32(bytes, entry);
        std::string original = "SSEQ" + sequence_number(file_id);
        if (symbol_base != 0 && index < symbol_count) {
            const std::uint32_t name_offset = read_u32(
                bytes, symbol_base + 4
                    + static_cast<std::size_t>(index) * 4);
            if (name_offset != 0) {
                original = null_string(bytes, symb_offset + name_offset,
                                       "SDAT SSEQ name");
            }
        }
        names.push_back({index, original,
                         sequence_number(index) + " - " + original});
    }
    return names;
}

using NcsfTag = std::pair<std::string_view, std::string_view>;

void make_ncsf(const std::filesystem::path& path,
               std::span<const std::uint8_t> reserved,
               std::span<const std::uint8_t> program,
               std::span<const NcsfTag> tags = {}) {
    std::vector<std::uint8_t> compressed;
    if (!program.empty()) {
        uLongf size = compressBound(static_cast<uLong>(program.size()));
        compressed.resize(size);
        const int status = compress2(
            compressed.data(), &size,
            reinterpret_cast<const Bytef*>(program.data()),
            static_cast<uLong>(program.size()), Z_BEST_COMPRESSION);
        if (status != Z_OK) {
            throw std::runtime_error("Could not zlib-compress NCSF data");
        }
        compressed.resize(size);
    }

    std::vector<std::uint8_t> output;
    output.reserve(16 + reserved.size() + compressed.size() + 64);
    output.insert(output.end(), {'P', 'S', 'F', 0x25});
    append_u32(output, static_cast<std::uint32_t>(reserved.size()));
    append_u32(output, static_cast<std::uint32_t>(compressed.size()));
    const auto checksum = compressed.empty() ? 0U
        : static_cast<std::uint32_t>(crc32(
              crc32(0L, Z_NULL, 0), compressed.data(),
              static_cast<uInt>(compressed.size())));
    append_u32(output, checksum);
    output.insert(output.end(), reserved.begin(), reserved.end());
    output.insert(output.end(), compressed.begin(), compressed.end());
    if (!tags.empty()) {
        output.insert(output.end(), {'[', 'T', 'A', 'G', ']'});
        for (const auto& [name, value] : tags) {
            output.insert(output.end(), name.begin(), name.end());
            output.push_back('=');
            output.insert(output.end(), value.begin(), value.end());
            output.push_back('\n');
        }
    }
    write_file(path, output);
}

void extract_archives(const std::filesystem::path& game_root,
                      const SetupReporter& report) {
    const auto archive_dir = game_root / "archives";
    if (!std::filesystem::is_directory(archive_dir)) {
        return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(archive_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string extension = entry.path().extension().string();
        for (char& value : extension) {
            if (value >= 'A' && value <= 'Z') {
                value = static_cast<char>(value - 'A' + 'a');
            }
        }
        if (extension != ".arc") {
            continue;
        }
        const std::string name = entry.path().stem().string();
        try {
            notify(report, "Reading " + name + "...");
            std::vector<std::uint8_t> bytes = read_file(entry.path());
            if (!bytes.empty() && bytes[0] == 0x10) {
                notify(report, "Decompressing " + name + "...");
                bytes = compression::lz10_decompress(bytes);
            }
            const auto archive = archive::Archive::parse(bytes);
            const auto destination = game_root / "_archives" / name;
            const std::size_t count = archive.extract(destination);
            notify(report, "Extracted " + std::to_string(count) + " file"
                + (count == 1 ? "." : "s."));
        } catch (const std::exception&) {
            notify(report, "Failed to extract archive. Verify an archive exists at "
                + entry.path().string() + ".");
        }
    }
}

void extract_rom_binaries(const nds::Rom& rom,
                          const std::filesystem::path& game_root,
                          const SetupReporter& report) {
    const auto& header = rom.header();
    const auto ftc = game_root / "ftc";
    const auto bins = game_root / "_bin";
    std::filesystem::create_directories(ftc);
    std::filesystem::create_directories(bins);

    const auto arm9 = rom.raw_range(
        static_cast<std::uint32_t>(header.arm9_offset),
        static_cast<std::uint32_t>(header.arm9_size));
    const auto arm7 = rom.raw_range(
        static_cast<std::uint32_t>(header.arm7_offset),
        static_cast<std::uint32_t>(header.arm7_size));
    const auto fat = rom.raw_range(header.fat_offset, header.fat_size);
    const auto fnt = rom.raw_range(header.fnt_offset, header.fnt_size);
    const auto banner = rom.raw_range(
        static_cast<std::uint32_t>(header.banner_offset), 0x840);
    const auto overlay_info = rom.raw_range(
        static_cast<std::uint32_t>(header.overlay9_offset),
        static_cast<std::uint32_t>(header.overlay9_size));
    write_file(ftc / "arm9.bin", arm9);
    write_file(ftc / "arm7.bin", arm7);
    write_file(ftc / "fat.bin", fat);
    write_file(ftc / "fnt.bin", fnt);
    write_file(ftc / "banner.bin", banner);
    write_file(ftc / "y9.bin", overlay_info);

    if ((overlay_info.size() % 32) != 0) {
        throw std::runtime_error("ARM9 overlay table size is invalid");
    }
    for (std::size_t offset = 0; offset < overlay_info.size(); offset += 32) {
        const std::uint32_t overlay_id = read_u32(overlay_info, offset);
        const std::uint32_t file_id = read_u32(overlay_info, offset + 24);
        write_file(ftc / ("overlay9_" + std::to_string(overlay_id)),
                   rom.file(file_id));
    }

    for (const auto& entry : std::filesystem::directory_iterator(ftc)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string filename = entry.path().filename().string();
        if (filename != "arm9.bin" && !filename.starts_with("overlay9_")) {
            continue;
        }
        notify(report, "Decompressing " + filename + "...");
        write_file(bins / filename,
                   compression::lz_backward_decompress(read_file(entry.path())));
    }
}

struct RuntimeSlice {
    std::size_t font_model_offset = 0;
    std::size_t font_model_size = 0;
};

struct FontRuntimeSlice {
    std::string_view key;
    std::size_t widths;
    std::size_t offsets;
    std::size_t characters;
};

constexpr std::array<FontRuntimeSlice, 8> FontRuntimeSlices{{
    {"A76E0", 0x95C68, 0x95A88, 0x96348},
    {"AMHE0", 0xBF9B0, 0xBFB90, 0xC0270},
    {"AMHE1", 0xC020C, 0xC03EC, 0xC0ACC},
    {"AMHJ0", 0xC1754, 0xC1934, 0xC2014},
    {"AMHJ1", 0xC1714, 0xC18F4, 0xC1FD4},
    {"AMHP0", 0xC022C, 0xC040C, 0xC0AEC},
    {"AMHP1", 0xC02AC, 0xC048C, 0xC0B6C},
    {"AMHK0", 0xBD580, 0xBD760, 0xB9560},
}};

[[nodiscard]] RuntimeSlice runtime_slice(std::string_view game_key) {
    if (game_key == formats::A76E0) return {0x9D528, 0x8284};
    if (game_key == formats::AMHE0) return {0xC76D4, 0x8284};
    if (game_key == formats::AMHE1) return {0xC7F5C, 0x8284};
    if (game_key == formats::AMHJ0) return {0xC9510, 0x8284};
    if (game_key == formats::AMHJ1) return {0xC94D0, 0x8284};
    if (game_key == formats::AMHP0) return {0xC7F7C, 0x8284};
    if (game_key == formats::AMHP1) return {0xC7FFC, 0x8284};
    if (game_key == formats::AMHK0) return {0xC0D40, 0x8284};
    return {};
}

void extract_runtime_files(std::string_view game_key,
                           const std::filesystem::path& game_root) {
    const RuntimeSlice slice = runtime_slice(game_key);
    if (slice.font_model_size == 0) {
        return;
    }
    const auto arm9 = read_file(game_root / "_bin" / "arm9.bin");
    if (slice.font_model_offset > arm9.size()
        || slice.font_model_size > arm9.size() - slice.font_model_offset) {
        throw std::runtime_error("Font model range is outside arm9.bin");
    }
    write_file(game_root / "models" / "hudfont_Model.bin",
               std::span<const std::uint8_t>(arm9).subspan(
                   slice.font_model_offset, slice.font_model_size));
}

} // namespace

std::size_t convert_sdat(const std::filesystem::path& input_path,
                         const std::filesystem::path& output_directory) {
    const std::vector<std::uint8_t> sdat_bytes = read_file(input_path);
    const auto names = sdat_sequence_names(sdat_bytes);
    std::filesystem::create_directories(output_directory);

    make_ncsf(output_directory / "mph.ncsflib", {}, sdat_bytes);
    for (const auto& sequence : names) {
        const std::array<std::uint8_t, 4> number{
            static_cast<std::uint8_t>(sequence.index),
            static_cast<std::uint8_t>(sequence.index >> 8),
            static_cast<std::uint8_t>(sequence.index >> 16),
            static_cast<std::uint8_t>(sequence.index >> 24),
        };
        const std::array<NcsfTag, 4> tags{{
            {"_lib", "mph.ncsflib"},
            {"utf8", "1"},
            {"ncsfby", "MphRead"},
            {"origFilename", sequence.original},
        }};
        make_ncsf(output_directory / (sequence.output + ".minincsf"),
                  number, {}, tags);
    }
    return names.size();
}

SetupResult setup(const std::filesystem::path& root,
                  const std::filesystem::path& rom_path,
                  SetupReporter report) {
    SetupResult result;
    try {
        const nds::Rom rom = nds::Rom::read_file(rom_path);
        if (!supported_game(rom.header(), result.first_hunt, result.error)) {
            notify(report, result.error);
            return result;
        }
        result.game_key = rom.header().game_code
            + std::to_string(rom.header().version);
        result.game_root = root / "files" / result.game_key;
        notify(report, "Writing " + result.game_root.string() + "...");
        result.rom_file_count = rom.extract(result.game_root);
        if (!result.first_hunt) {
            extract_archives(result.game_root, report);
            notify(report, "Converting sound_data.sdat...");
            (void)convert_sdat(result.game_root / "data" / "sound"
                                   / "sound_data.sdat",
                               result.game_root / "_seq");
        }
        extract_rom_binaries(rom, result.game_root, report);
        extract_runtime_files(result.game_key, result.game_root);

        formats::Paths paths;
        paths.update(root);
        const auto selected = std::filesystem::absolute(result.game_root);
        if (!paths.set_path(result.game_key,
                            result.first_hunt ? selected / "data" : selected)) {
            throw std::runtime_error("The ROM version has no paths.txt key");
        }
        paths.choose_mph_path();
        paths.choose_fh_path();
        if (!paths.write(root)) {
            throw std::runtime_error("Could not write paths.txt.");
        }
        result.ok = true;
        notify(report, "Setup complete: " + result.game_key);
    } catch (const std::exception& exception) {
        result.error = exception.what();
        notify(report, result.error);
    }
    return result;
}

bool load_runtime_data(const assets::Store& assets) {
    const auto key = assets.game_key();
    if (!key.has_value()) {
        return false;
    }
    const auto found = std::find_if(
        FontRuntimeSlices.begin(), FontRuntimeSlices.end(),
        [&key](const FontRuntimeSlice& item) { return item.key == *key; });
    if (found == FontRuntimeSlices.end()) {
        return false;
    }
    const auto arm9 = assets.bytes("_bin/arm9.bin");
    constexpr std::size_t WidthSize = 480;
    constexpr std::size_t OffsetSize = 480;
    constexpr std::size_t CharacterSize = 0x4000;
    const auto range = [&arm9](std::size_t offset, std::size_t size) {
        if (offset > arm9.size() || size > arm9.size() - offset) {
            throw std::out_of_range(
                "Extract.LoadRuntimeData range is outside arm9.bin");
        }
        return std::span<const std::uint8_t>(arm9).subspan(offset, size);
    };
    strings::Font::normal().set_data(
        range(found->widths, WidthSize),
        range(found->offsets, OffsetSize),
        range(found->characters, CharacterSize), 32);
    const auto sound_metadata = sound::SoundMetadata::load(assets);
    if (!sound_metadata.has_value()) {
        return false;
    }
    metadata::Metadata::InstallSoundMetadata(*sound_metadata);
    return true;
}

int run_command(int argc, char** argv) {
    const std::string info_path = value_after(argc, argv, "-rominfo");
    const std::string extract_path = value_after(argc, argv, "-extract-rom");
    const std::string source = !info_path.empty() ? info_path : extract_path;
    if (source.empty()) {
        throw std::invalid_argument("ROM command needs a file");
    }
    const auto rom = nds::Rom::read_file(source);
    if (!info_path.empty()) {
        const auto& header = rom.header();
        std::cout << "title=\"" << header.title << "\""
                  << " game_code=" << header.game_code
                  << " version=" << static_cast<int>(header.version)
                  << " files=" << rom.files().size() << '\n';
        return EXIT_SUCCESS;
    }
    const std::string requested_output = value_after(argc, argv, "-out");
    const std::filesystem::path root = requested_output.empty()
        ? std::filesystem::current_path()
        : std::filesystem::path(requested_output);
    const SetupResult result = setup(root, source,
        [](std::string_view line) { std::cout << line << '\n'; });
    return result.ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace fruityprime::utility::extract
