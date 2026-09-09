#include "Sound/sdat.hpp"
#include "Metadata/MetadataValues.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace fruityprime::sound {
namespace {

using Bytes = std::span<const std::uint8_t>;

[[noreturn]] void invalid(const std::string& message) {
    throw std::runtime_error("invalid Nitro Composer data: " + message);
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

[[nodiscard]] Bytes section(Bytes bytes, std::uint32_t offset,
                            std::string_view tag, std::string_view what) {
    require_range(bytes, offset, 8, what);
    require_tag(bytes, offset, tag, what);
    const std::uint32_t size = u32(bytes, offset + 4);
    require(size >= 8, std::string(what) + " has an invalid size");
    require_range(bytes, offset, size, what);
    return bytes.subspan(offset, size);
}

[[nodiscard]] std::vector<std::uint32_t> record_offsets(
    Bytes section_bytes, std::uint32_t relative_offset, std::string_view what) {
    if (relative_offset == 0) {
        return {};
    }
    require_range(section_bytes, relative_offset, 4, what);
    const std::uint32_t count = u32(section_bytes, relative_offset);
    require(count <= (section_bytes.size() - relative_offset - 4) / 4,
            std::string(what) + " contains too many entries");

    std::vector<std::uint32_t> result(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        result[i] = u32(section_bytes,
                         static_cast<std::size_t>(relative_offset) + 4
                             + static_cast<std::size_t>(i) * 4);
    }
    return result;
}

[[nodiscard]] Bytes info_entry(Bytes section_bytes, std::uint32_t offset,
                               std::size_t size, std::string_view what) {
    require(offset != 0, std::string(what) + " entry has no offset");
    require_range(section_bytes, offset, size, what);
    return section_bytes.subspan(offset, size);
}

void parse_symbol_record(Bytes section_bytes, std::uint32_t relative_offset,
                         std::vector<std::string>& names,
                         std::string_view what) {
    const auto offsets = record_offsets(section_bytes, relative_offset, what);
    names.assign(offsets.size(), {});
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        if (offsets[i] == 0) {
            continue;
        }
        require(offsets[i] < section_bytes.size(),
                std::string(what) + " name offset is outside its section");
        const std::size_t begin = offsets[i];
        std::size_t end = begin;
        while (end < section_bytes.size() && section_bytes[end] != 0) {
            ++end;
        }
        require(end < section_bytes.size(),
                std::string(what) + " name is not null terminated");
        names[i] = std::string(
            reinterpret_cast<const char*>(section_bytes.data() + begin),
            end - begin);
    }
}

// Sound.ReadSdat's seqarc walk.  The SEQARC record is not a list of name
// offsets like the others: each entry is two offsets, so it cannot go through
// parse_symbol_record.
void parse_seqarc_record(Bytes section_bytes, std::uint32_t relative_offset,
                         std::vector<SequenceArchiveNames>& archives,
                         std::string_view what) {
    archives.clear();
    if (relative_offset == 0) {
        return;
    }
    require(relative_offset + 4 <= section_bytes.size(),
            std::string(what) + " record is outside its section");
    const std::uint32_t count = u32(section_bytes, relative_offset);
    const std::size_t table = relative_offset + 4;
    require(table + static_cast<std::size_t>(count) * 8 <= section_bytes.size(),
            std::string(what) + " table is outside its section");
    const auto read_name = [&](std::uint32_t offset) {
        if (offset == 0 || offset >= section_bytes.size()) {
            return std::string();
        }
        std::size_t end = offset;
        while (end < section_bytes.size() && section_bytes[end] != 0) {
            ++end;
        }
        if (end >= section_bytes.size()) {
            return std::string();
        }
        return std::string(
            reinterpret_cast<const char*>(section_bytes.data() + offset),
            end - offset);
    };
    archives.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        SeqArcEntries entry;
        entry.entry_offset = u32(section_bytes, table + i * 8);
        entry.files_offset = u32(section_bytes, table + i * 8 + 4);
        SequenceArchiveNames archive;
        archive.name = read_name(entry.entry_offset);
        // The inner list is an ordinary name record, so it reuses the same
        // walk the other five symbol records use.
        if (entry.files_offset != 0) {
            parse_symbol_record(section_bytes, entry.files_offset,
                                archive.file_names, what);
        }
        archives.push_back(std::move(archive));
    }
}

[[nodiscard]] std::string numbered_name(std::string_view prefix,
                                        std::uint32_t value) {
    std::ostringstream stream;
    stream << prefix << std::uppercase << std::hex << std::setfill('0')
           << std::setw(4) << value;
    return stream.str();
}

void parse_standard_header(Bytes bytes, std::string_view tag,
                           std::string_view what) {
    require_range(bytes, 0, 0x10, what);
    require_tag(bytes, 0, tag, what);
    require(u32(bytes, 4) == 0x0100FEFF,
            std::string(what) + " has an unexpected Nitro magic");
    require(u16(bytes, 0x0C) == 0x10,
            std::string(what) + " has an unexpected header size");
}

void parse_instrument(Bytes bytes, std::size_t offset, Instrument& instrument) {
    require_range(bytes, offset, 0x0A, "SBNK instrument");
    instrument.swav = u16(bytes, offset);
    instrument.swar = u16(bytes, offset + 2);
    instrument.note_number = bytes[offset + 4];
    instrument.attack_rate = bytes[offset + 5];
    instrument.decay_rate = bytes[offset + 6];
    instrument.sustain_level = bytes[offset + 7];
    instrument.release_rate = bytes[offset + 8];
    instrument.pan = bytes[offset + 9];
}

void parse_entry_instruments(Bytes bytes, InstrumentEntry& entry) {
    if (entry.record == 0) {
        return;
    }
    const std::size_t offset = entry.offset;
    if (entry.record >= 1 && entry.record <= 5) {
        Instrument instrument;
        instrument.record = entry.record;
        instrument.low_note = 0;
        instrument.high_note = 127;
        parse_instrument(bytes, offset, instrument);
        entry.instruments.push_back(instrument);
        return;
    }

    if (entry.record == 16) {
        require_range(bytes, offset, 2, "SBNK drum-set header");
        const auto low_note = bytes[offset];
        const auto high_note = bytes[offset + 1];
        require(high_note >= low_note,
                "SBNK drum-set note range is inverted");
        const std::size_t count = high_note - low_note + 1;
        require(count <= (bytes.size() - offset - 2) / 12,
                "SBNK drum-set entries are truncated");
        entry.instruments.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t header = offset + 2 + i * 12;
            Instrument instrument;
            instrument.low_note = static_cast<std::uint8_t>(low_note + i);
            instrument.high_note = instrument.low_note;
            instrument.record = bytes[header];
            parse_instrument(bytes, header + 2, instrument);
            entry.instruments.push_back(instrument);
        }
        return;
    }

    if (entry.record == 17) {
        require_range(bytes, offset, 8, "SBNK key-split header");
        std::size_t count = 0;
        std::uint8_t low_note = 0;
        for (; count < 8 && bytes[offset + count] != 0; ++count) {
            const std::uint8_t high_note = bytes[offset + count];
            const std::size_t header = offset + 8 + count * 12;
            require_range(bytes, header, 2 + 0x0A,
                          "SBNK key-split entry");
            Instrument instrument;
            instrument.low_note = low_note;
            instrument.high_note = high_note;
            instrument.record = bytes[header];
            parse_instrument(bytes, header + 2, instrument);
            entry.instruments.push_back(instrument);
            low_note = static_cast<std::uint8_t>(high_note + 1);
        }
        return;
    }

    // Record types outside the standard instrument families are retained in
    // the entry but have no fixed-size definition that can be decoded here.
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

void decode_ima_nibble(int nibble, int& step_index, int& predicted_value,
                       std::vector<std::int16_t>& output) {
    const int step = ImaStepTable[step_index];
    step_index = std::clamp(
        step_index + metadata::ImaIndexTable[nibble], 0, 88);

    int difference = step >> 3;
    if ((nibble & 1) != 0) {
        difference += step >> 2;
    }
    if ((nibble & 2) != 0) {
        difference += step >> 1;
    }
    if ((nibble & 4) != 0) {
        difference += step;
    }

    if ((nibble & 8) == 0) {
        predicted_value = std::min(predicted_value + difference, 0x7FFF);
    } else {
        predicted_value = std::max(predicted_value - difference, -0x8000);
    }
    output.push_back(static_cast<std::int16_t>(predicted_value));
}

} // namespace

Sseq Sseq::parse(Bytes bytes, std::uint32_t file_id, std::string name) {
    parse_standard_header(bytes, "SSEQ", "SSEQ");
    require_range(bytes, 0x10, 0x0C, "SSEQ DATA header");
    require_tag(bytes, 0x10, "DATA", "SSEQ DATA block");
    const std::uint32_t data_size = u32(bytes, 0x14);
    const std::uint32_t data_offset = u32(bytes, 0x18);
    require(data_size >= 0x0C, "SSEQ DATA block is too small");
    const std::size_t payload_size = data_size - 0x0C;
    require_range(bytes, data_offset, payload_size, "SSEQ sequence data");

    Sseq result;
    result.file_id = file_id;
    result.name = std::move(name);
    result.data.assign(bytes.begin() + data_offset,
                       bytes.begin() + data_offset + payload_size);
    return result;
}

Swav Swav::parse(Bytes bytes) {
    require_range(bytes, 0, 0x0C, "SWAV");
    Swav result;
    const auto raw_wave_type = bytes[0];
    result.wave_type = static_cast<WaveType>(raw_wave_type);
    result.loop = bytes[1];
    result.sample_rate = u16(bytes, 2);
    result.time = u16(bytes, 4);
    result.original_loop_offset = u16(bytes, 6);
    result.original_loop_length = u32(bytes, 8);

    const std::uint64_t raw_size =
        (static_cast<std::uint64_t>(result.original_loop_offset)
         + result.original_loop_length)
        * 4;
    require(raw_size <= std::numeric_limits<std::size_t>::max(),
            "SWAV sample size overflows the host size");
    const std::size_t sample_size = static_cast<std::size_t>(raw_size);
    require_range(bytes, 0x0C, sample_size, "SWAV sample data");
    result.original_data.assign(bytes.begin() + 0x0C,
                                bytes.begin() + 0x0C + sample_size);

    switch (result.wave_type) {
    case WaveType::Pcm8:
        result.loop_offset = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(result.original_loop_offset) * 4);
        result.loop_length = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(result.original_loop_length) * 4);
        break;
    case WaveType::Pcm16:
        result.loop_offset = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(result.original_loop_offset) * 2);
        result.loop_length = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(result.original_loop_length) * 2);
        break;
    case WaveType::ImaAdpcm:
        if (result.original_loop_offset != 0) {
            --result.original_loop_offset;
        }
        result.loop_offset = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(result.original_loop_offset) * 8);
        result.loop_length = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(result.original_loop_length) * 8);
        break;
    default:
        break;
    }
    return result;
}

std::vector<std::int16_t> Swav::decode_pcm() const {
    std::vector<std::int16_t> result;
    switch (wave_type) {
    case WaveType::Pcm8:
        result.reserve(original_data.size());
        for (const auto sample : original_data) {
            const auto signed_sample = static_cast<std::int8_t>(sample);
            const int scaled = static_cast<int>(signed_sample) * 258;
            result.push_back(static_cast<std::int16_t>(
                std::clamp(scaled, -32768, 32767)));
        }
        break;
    case WaveType::Pcm16:
        require(original_data.size() % 2 == 0,
                "SWAV PCM16 data is not sample aligned");
        result.reserve(original_data.size() / 2);
        for (std::size_t i = 0; i < original_data.size(); i += 2) {
            result.push_back(static_cast<std::int16_t>(
                static_cast<std::uint16_t>(original_data[i])
                | static_cast<std::uint16_t>(original_data[i + 1]) << 8));
        }
        break;
    case WaveType::ImaAdpcm:
        require(original_data.size() >= 4,
                "SWAV IMA-ADPCM data has no decoder header");
        result.reserve((original_data.size() - 4) * 2);
        {
            int predicted_value = static_cast<std::int16_t>(
                static_cast<std::uint16_t>(original_data[0])
                | static_cast<std::uint16_t>(original_data[1]) << 8);
            int step_index = std::clamp(
                static_cast<int>(static_cast<std::uint16_t>(original_data[2])
                                 | static_cast<std::uint16_t>(original_data[3])
                                       << 8),
                0, 88);
            for (std::size_t i = 4; i < original_data.size(); ++i) {
                const auto sample = original_data[i];
                decode_ima_nibble(sample & 0x0F, step_index, predicted_value,
                                  result);
                decode_ima_nibble((sample >> 4) & 0x0F, step_index,
                                  predicted_value, result);
            }
        }
        break;
    default:
        break;
    }
    return result;
}

Swar Swar::parse(Bytes bytes, std::uint32_t file_id, std::string name) {
    parse_standard_header(bytes, "SWAR", "SWAR");
    require_range(bytes, 0x10, 0x2C, "SWAR DATA header");
    require_tag(bytes, 0x10, "DATA", "SWAR DATA block");
    const std::uint32_t count = u32(bytes, 0x38);
    require(count <= (bytes.size() - 0x3C) / 4,
            "SWAR contains too many wave offsets");

    Swar result;
    result.file_id = file_id;
    result.name = std::move(name);
    result.waves.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t offset = u32(bytes, 0x3C + i * 4);
        if (offset != 0) {
            require(offset < bytes.size(),
                    "SWAR wave offset is outside the file");
            result.waves[i] = Swav::parse(bytes.subspan(offset));
        }
    }
    return result;
}

Sbnk Sbnk::parse(Bytes bytes, std::uint32_t file_id, std::string name) {
    parse_standard_header(bytes, "SBNK", "SBNK");
    require_range(bytes, 0x10, 0x2C, "SBNK DATA header");
    require_tag(bytes, 0x10, "DATA", "SBNK DATA block");
    const std::uint32_t count = u32(bytes, 0x38);
    require(count <= (bytes.size() - 0x3C) / 4,
            "SBNK contains too many instrument offsets");

    Sbnk result;
    result.file_id = file_id;
    result.name = std::move(name);
    result.entries.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t offset = 0x3C + i * 4;
        result.entries[i].record = bytes[offset];
        result.entries[i].offset = u16(bytes, offset + 1);
    }
    for (auto& entry : result.entries) {
        parse_entry_instruments(bytes, entry);
    }
    return result;
}

Sdat Sdat::parse(Bytes bytes) {
    require_range(bytes, 0, 0x40, "SDAT header");
    require_tag(bytes, 0, "SDAT", "SDAT");
    require(u32(bytes, 4) == 0x0100FEFF,
            "SDAT has an unexpected Nitro magic");
    const std::uint32_t declared_size = u32(bytes, 8);
    require(declared_size >= 0x40, "SDAT has an invalid file size");
    require_range(bytes, 0, declared_size, "SDAT file");

    std::vector<std::uint8_t> owned(bytes.begin(), bytes.begin() + declared_size);
    Sdat result(std::move(owned));
    result.parse_sections();
    return result;
}

Sdat Sdat::read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open SDAT " + path.string());
    }
    const auto length = input.tellg();
    if (length < 0) {
        throw std::runtime_error("could not determine SDAT size "
                                 + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read SDAT " + path.string());
        }
    }
    return parse(bytes);
}

void Sdat::parse_sections() {
    const Bytes bytes(bytes_);
    header_size_ = u16(bytes, 0x0C);
    blocks_ = u16(bytes, 0x0E);
    require(header_size_ >= 0x40, "SDAT header is too small");
    file_size_ = u32(bytes, 8);
    symb_offset_ = u32(bytes, 0x10);
    symb_size_ = u32(bytes, 0x14);
    info_offset_ = u32(bytes, 0x18);
    info_size_ = u32(bytes, 0x1C);
    fat_offset_ = u32(bytes, 0x20);
    fat_size_ = u32(bytes, 0x24);
    file_offset_ = u32(bytes, 0x28);
    file_section_size_ = u32(bytes, 0x2C);
    source_count_ = u16(bytes, 0x30);

    sequences_.clear();
    banks_.clear();
    wave_archives_.clear();
    players_.clear();
    streams_.clear();
    sequence_names_.clear();
    bank_names_.clear();
    sequence_archive_names_.clear();
    wave_archive_names_.clear();
    player_names_.clear();
    stream_names_.clear();
    fat_records_.clear();

    std::array<std::uint32_t, 8> info_record_offsets{};
    if (symb_offset_ != 0) {
        const auto symb = section(bytes, symb_offset_, "SYMB", "SDAT SYMB");
        std::array<std::uint32_t, 8> symbol_record_offsets{};
        for (std::size_t i = 0; i < symbol_record_offsets.size(); ++i) {
            symbol_record_offsets[i] = u32(symb, 0x08 + i * 4);
        }
        parse_symbol_record(symb, symbol_record_offsets[0], sequence_names_,
                            "SDAT SYMB SEQ");
        parse_seqarc_record(symb, symbol_record_offsets[1],
                            sequence_archive_names_,
                            "SDAT SYMB SEQARC");
        parse_symbol_record(symb, symbol_record_offsets[2], bank_names_,
                            "SDAT SYMB BANK");
        parse_symbol_record(symb, symbol_record_offsets[3],
                            wave_archive_names_, "SDAT SYMB WAVEARC");
        parse_symbol_record(symb, symbol_record_offsets[4], player_names_,
                            "SDAT SYMB PLAYER");
        parse_symbol_record(symb, symbol_record_offsets[7], stream_names_,
                            "SDAT SYMB STRM");
    }

    const auto info = section(bytes, info_offset_, "INFO", "SDAT INFO");
    for (std::size_t i = 0; i < info_record_offsets.size(); ++i) {
        info_record_offsets[i] = u32(info, 0x08 + i * 4);
    }

    const auto sequence_offsets =
        record_offsets(info, info_record_offsets[0], "SDAT INFO SEQ");
    sequences_.resize(sequence_offsets.size());
    for (std::size_t i = 0; i < sequence_offsets.size(); ++i) {
        if (sequence_offsets[i] == 0) {
            continue;
        }
        const auto entry = info_entry(info, sequence_offsets[i], 0x0C,
                                      "SDAT INFO SEQ entry");
        auto& target = sequences_[i];
        target.present = true;
        target.file_id = u32(entry, 0);
        target.bank = u16(entry, 4);
        target.volume = entry[6];
        target.channel_priority = entry[7];
        target.player_priority = entry[8];
        target.player = entry[9];
        target.reserved = u16(entry, 0x0A);
    }

    const auto bank_offsets =
        record_offsets(info, info_record_offsets[2], "SDAT INFO BANK");
    banks_.resize(bank_offsets.size());
    for (std::size_t i = 0; i < bank_offsets.size(); ++i) {
        if (bank_offsets[i] == 0) {
            continue;
        }
        const auto entry = info_entry(info, bank_offsets[i], 0x0C,
                                      "SDAT INFO BANK entry");
        auto& target = banks_[i];
        target.present = true;
        target.file_id = u32(entry, 0);
        for (std::size_t j = 0; j < target.wave_archives.size(); ++j) {
            target.wave_archives[j] = u16(entry, 4 + j * 2);
        }
    }

    const auto wave_archive_offsets = record_offsets(
        info, info_record_offsets[3], "SDAT INFO WAVEARC");
    wave_archives_.resize(wave_archive_offsets.size());
    for (std::size_t i = 0; i < wave_archive_offsets.size(); ++i) {
        if (wave_archive_offsets[i] == 0) {
            continue;
        }
        const auto entry = info_entry(info, wave_archive_offsets[i], 4,
                                      "SDAT INFO WAVEARC entry");
        auto& target = wave_archives_[i];
        target.present = true;
        target.file_id = u32(entry, 0) & 0x00FFFFFF;
        target.flags = entry[3];
    }

    const auto player_offsets =
        record_offsets(info, info_record_offsets[4], "SDAT INFO PLAYER");
    players_.resize(player_offsets.size());
    for (std::size_t i = 0; i < player_offsets.size(); ++i) {
        if (player_offsets[i] == 0) {
            continue;
        }
        const auto entry = info_entry(info, player_offsets[i], 8,
                                      "SDAT INFO PLAYER entry");
        auto& target = players_[i];
        target.present = true;
        target.max_sequences = entry[0];
        target.padding = entry[1];
        target.channel_mask = u16(entry, 2);
        target.heap_size = u32(entry, 4);
    }

    const auto stream_offsets =
        record_offsets(info, info_record_offsets[7], "SDAT INFO STRM");
    streams_.resize(stream_offsets.size());
    for (std::size_t i = 0; i < stream_offsets.size(); ++i) {
        if (stream_offsets[i] == 0) {
            continue;
        }
        const auto entry = info_entry(info, stream_offsets[i], 8,
                                      "SDAT INFO STRM entry");
        auto& target = streams_[i];
        target.present = true;
        target.file_id = u32(entry, 0);
        target.volume = entry[4];
        target.player_priority = entry[5];
        target.player = entry[6];
        target.flags = entry[7];
    }

    const auto fat = section(bytes, fat_offset_, "FAT ", "SDAT FAT");
    const std::uint32_t fat_count = u32(fat, 8);
    require(fat_count <= (fat.size() - 0x0C) / 0x10,
            "SDAT FAT contains too many records");
    fat_records_.resize(fat_count);
    for (std::uint32_t i = 0; i < fat_count; ++i) {
        const std::size_t offset = 0x0C + i * 0x10;
        fat_records_[i].offset = u32(fat, offset);
        fat_records_[i].size = u32(fat, offset + 4);
        require_range(bytes, fat_records_[i].offset, fat_records_[i].size,
                      "SDAT FAT file");
    }

    if (file_offset_ != 0) {
        require_tag(bytes, file_offset_, "FILE", "SDAT FILE");
        require_range(bytes, file_offset_, 8, "SDAT FILE");
    }
}

std::vector<std::uint8_t> Sdat::file(std::uint32_t file_id) const {
    if (file_id >= fat_records_.size()) {
        throw std::out_of_range("SDAT file ID is outside the FAT");
    }
    const auto& record = fat_records_[file_id];
    const Bytes bytes(bytes_);
    require_range(bytes, record.offset, record.size, "SDAT FAT file");
    return std::vector<std::uint8_t>(bytes.begin() + record.offset,
                                    bytes.begin() + record.offset + record.size);
}

Sseq Sdat::sequence(std::size_t index) const {
    if (index >= sequences_.size()) {
        throw std::out_of_range("SDAT sequence index is outside INFO");
    }
    const auto& info = sequences_[index];
    if (!info.present) {
        throw std::runtime_error("SDAT sequence entry is empty");
    }
    std::string name = index < sequence_names_.size()
        ? sequence_names_[index]
        : std::string{};
    if (name.empty()) {
        name = numbered_name("SSEQ", info.file_id);
    }
    return Sseq::parse(file(info.file_id), info.file_id, std::move(name));
}

Sbnk Sdat::bank(std::size_t index) const {
    if (index >= banks_.size()) {
        throw std::out_of_range("SDAT bank index is outside INFO");
    }
    const auto& info = banks_[index];
    if (!info.present) {
        throw std::runtime_error("SDAT bank entry is empty");
    }
    std::string name = index < bank_names_.size() ? bank_names_[index]
                                                   : std::string{};
    if (name.empty()) {
        name = numbered_name("SBNK", info.file_id);
    }
    return Sbnk::parse(file(info.file_id), info.file_id, std::move(name));
}

Swar Sdat::wave_archive(std::size_t index) const {
    if (index >= wave_archives_.size()) {
        throw std::out_of_range("SDAT wave archive index is outside INFO");
    }
    const auto& info = wave_archives_[index];
    if (!info.present) {
        throw std::runtime_error("SDAT wave archive entry is empty");
    }
    std::string name = index < wave_archive_names_.size()
        ? wave_archive_names_[index]
        : std::string{};
    if (name.empty()) {
        name = numbered_name("SWAR", info.file_id);
    }
    return Swar::parse(file(info.file_id), info.file_id, std::move(name));
}

} // namespace fruityprime::sound
