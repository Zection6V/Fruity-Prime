#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace fruityprime::sound {

// Nintendo DS Nitro Composer (SDAT) records.  The parser keeps these values
// in the same index space as the INFO records, so a sequence can resolve its
// bank and the bank can resolve its four wave archives without a second
// extraction pass.
// Sound.SeqArcEntries: one entry in the SDAT symbol block's sequence-archive
// list.  It is a pair of offsets rather than a name -- the first points at the
// archive's own name and the second at the list of names of the sequences
// inside it.
struct SeqArcEntries {
    std::uint32_t entry_offset = 0;
    std::uint32_t files_offset = 0;
};

// A sequence archive: its name, and the name of every sequence packed in it.
struct SequenceArchiveNames {
    std::string name;
    std::vector<std::string> file_names;
};

struct FatRecord {
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};

struct SequenceInfo {
    bool present = false;
    std::uint32_t file_id = 0;
    std::uint16_t bank = 0;
    std::uint8_t volume = 0;
    std::uint8_t channel_priority = 0;
    std::uint8_t player_priority = 0;
    std::uint8_t player = 0;
    std::uint16_t reserved = 0;
};

struct BankInfo {
    bool present = false;
    std::uint32_t file_id = 0;
    std::array<std::uint16_t, 4> wave_archives{
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
};

struct WaveArchiveInfo {
    bool present = false;
    std::uint32_t file_id = 0;
    std::uint8_t flags = 0;
};

struct PlayerInfo {
    bool present = false;
    std::uint8_t max_sequences = 0;
    std::uint8_t padding = 0;
    std::uint16_t channel_mask = 0;
    std::uint32_t heap_size = 0;
};

struct StreamInfo {
    bool present = false;
    std::uint32_t file_id = 0;
    std::uint8_t volume = 0;
    std::uint8_t player_priority = 0;
    std::uint8_t player = 0;
    std::uint8_t flags = 0;
};

struct Sseq {
    std::string name;
    std::uint32_t file_id = 0;
    std::vector<std::uint8_t> data;

    [[nodiscard]] static Sseq parse(std::span<const std::uint8_t> bytes,
                                    std::uint32_t file_id = 0,
                                    std::string name = {});
};

enum class WaveType : std::uint8_t {
    Pcm8 = 0,
    Pcm16 = 1,
    ImaAdpcm = 2,
};

struct Swav {
    WaveType wave_type = WaveType::Pcm8;
    std::uint8_t loop = 0;
    std::uint16_t sample_rate = 0;
    std::uint16_t time = 0;
    std::uint16_t original_loop_offset = 0;
    std::uint32_t loop_offset = 0;
    std::uint32_t original_loop_length = 0;
    std::uint32_t loop_length = 0;
    std::vector<std::uint8_t> original_data;

    [[nodiscard]] static Swav parse(std::span<const std::uint8_t> bytes);
    [[nodiscard]] std::vector<std::int16_t> decode_pcm() const;
};

struct Swar {
    std::string name;
    std::uint32_t file_id = 0;
    std::vector<std::optional<Swav>> waves;

    [[nodiscard]] static Swar parse(std::span<const std::uint8_t> bytes,
                                    std::uint32_t file_id = 0,
                                    std::string name = {});
};

struct Instrument {
    std::uint8_t low_note = 0;
    std::uint8_t high_note = 127;
    std::uint8_t record = 0;
    std::uint16_t swav = 0;
    std::uint16_t swar = 0;
    std::uint8_t note_number = 0;
    std::uint8_t attack_rate = 0;
    std::uint8_t decay_rate = 0;
    std::uint8_t sustain_level = 0;
    std::uint8_t release_rate = 0;
    std::uint8_t pan = 0;
};

struct InstrumentEntry {
    std::uint8_t record = 0;
    std::uint16_t offset = 0;
    std::vector<Instrument> instruments;
};

struct Sbnk {
    std::string name;
    std::uint32_t file_id = 0;
    std::vector<InstrumentEntry> entries;

    [[nodiscard]] static Sbnk parse(std::span<const std::uint8_t> bytes,
                                    std::uint32_t file_id = 0,
                                    std::string name = {});
};

class Sdat {
public:
    [[nodiscard]] static Sdat parse(std::span<const std::uint8_t> bytes);
    [[nodiscard]] static Sdat read_file(const std::filesystem::path& path);

    [[nodiscard]] std::uint32_t file_size() const noexcept {
        return file_size_;
    }
    [[nodiscard]] std::uint16_t header_size() const noexcept {
        return header_size_;
    }
    [[nodiscard]] std::uint16_t blocks() const noexcept { return blocks_; }
    [[nodiscard]] bool has_symbols() const noexcept {
        return symb_offset_ != 0;
    }
    [[nodiscard]] std::uint32_t symb_offset() const noexcept {
        return symb_offset_;
    }
    [[nodiscard]] std::uint32_t info_offset() const noexcept {
        return info_offset_;
    }
    [[nodiscard]] std::uint32_t fat_offset() const noexcept {
        return fat_offset_;
    }
    [[nodiscard]] std::uint32_t file_offset() const noexcept {
        return file_offset_;
    }
    [[nodiscard]] std::uint32_t file_section_size() const noexcept {
        return file_section_size_;
    }
    [[nodiscard]] std::uint16_t source_count() const noexcept {
        return source_count_;
    }

    [[nodiscard]] const std::vector<FatRecord>& fat_records() const noexcept {
        return fat_records_;
    }
    [[nodiscard]] const std::vector<SequenceInfo>& sequences() const noexcept {
        return sequences_;
    }
    [[nodiscard]] const std::vector<BankInfo>& banks() const noexcept {
        return banks_;
    }
    [[nodiscard]] const std::vector<WaveArchiveInfo>& wave_archives() const
        noexcept {
        return wave_archives_;
    }
    [[nodiscard]] const std::vector<PlayerInfo>& players() const noexcept {
        return players_;
    }
    [[nodiscard]] const std::vector<StreamInfo>& streams() const noexcept {
        return streams_;
    }
    [[nodiscard]] const std::vector<std::string>& sequence_names() const
        noexcept {
        return sequence_names_;
    }
    // The SDAT symbol block's SEQARC record, which the managed reader walks
    // to name the sequences inside each archive.  Empty when the file has no
    // symbol block at all.
    [[nodiscard]] const std::vector<SequenceArchiveNames>&
        sequence_archive_names() const noexcept {
        return sequence_archive_names_;
    }
    [[nodiscard]] const std::vector<std::string>& bank_names() const noexcept {
        return bank_names_;
    }
    [[nodiscard]] const std::vector<std::string>& wave_archive_names() const
        noexcept {
        return wave_archive_names_;
    }
    [[nodiscard]] const std::vector<std::string>& player_names() const noexcept {
        return player_names_;
    }
    [[nodiscard]] const std::vector<std::string>& stream_names() const noexcept {
        return stream_names_;
    }

    // Returns one file from the SDAT FILE section, excluding any padding
    // after the FAT-record size.
    [[nodiscard]] std::vector<std::uint8_t> file(std::uint32_t file_id) const;

    [[nodiscard]] Sseq sequence(std::size_t index) const;
    [[nodiscard]] Sbnk bank(std::size_t index) const;
    [[nodiscard]] Swar wave_archive(std::size_t index) const;

private:
    explicit Sdat(std::vector<std::uint8_t> bytes)
        : bytes_(std::move(bytes)) {}

    void parse_sections();

    std::vector<std::uint8_t> bytes_;
    std::uint32_t file_size_ = 0;
    std::uint16_t header_size_ = 0;
    std::uint16_t blocks_ = 0;
    std::uint32_t symb_offset_ = 0;
    std::uint32_t symb_size_ = 0;
    std::uint32_t info_offset_ = 0;
    std::uint32_t info_size_ = 0;
    std::uint32_t fat_offset_ = 0;
    std::uint32_t fat_size_ = 0;
    std::uint32_t file_offset_ = 0;
    std::uint32_t file_section_size_ = 0;
    std::uint16_t source_count_ = 0;

    std::vector<FatRecord> fat_records_;
    std::vector<SequenceInfo> sequences_;
    std::vector<BankInfo> banks_;
    std::vector<WaveArchiveInfo> wave_archives_;
    std::vector<PlayerInfo> players_;
    std::vector<StreamInfo> streams_;
    std::vector<std::string> sequence_names_;
    std::vector<std::string> bank_names_;
    std::vector<SequenceArchiveNames> sequence_archive_names_;
    std::vector<std::string> wave_archive_names_;
    std::vector<std::string> player_names_;
    std::vector<std::string> stream_names_;
};

} // namespace fruityprime::sound
