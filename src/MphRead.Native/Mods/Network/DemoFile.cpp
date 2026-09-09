#include "Mods/Network/demo.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace fruityprime::demo {
namespace {

constexpr std::array<std::uint8_t, 4> Magic{{'F', 'P', 'D', 'M'}};
constexpr std::size_t MaxInflatedBytes = 512U * 1024U * 1024U;

[[nodiscard]] std::vector<std::uint8_t> read_all(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open demo file: " + path.string());
    }
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<std::uintmax_t>(length)
        > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("demo file is too large: " + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read demo file: " + path.string());
        }
    }
    return bytes;
}

void put_u16(std::vector<std::uint8_t>& destination, std::uint16_t value) {
    destination.push_back(static_cast<std::uint8_t>(value & 0xffu));
    destination.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
}

void put_u32(std::vector<std::uint8_t>& destination, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        destination.push_back(static_cast<std::uint8_t>((value >> shift)
                                                         & 0xffu));
    }
}

[[nodiscard]] std::uint16_t get_u16(std::span<const std::uint8_t> source,
                                    std::size_t offset) {
    return static_cast<std::uint16_t>(source[offset])
        | static_cast<std::uint16_t>(source[offset + 1]) << 8;
}

[[nodiscard]] std::uint32_t get_u32(std::span<const std::uint8_t> source,
                                    std::size_t offset) {
    std::uint32_t value = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        value |= static_cast<std::uint32_t>(source[offset + shift / 8])
            << shift;
    }
    return value;
}

class BitReader {
public:
    explicit BitReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    [[nodiscard]] std::uint32_t read(int count) {
        if (count <= 0 || count > 32) {
            throw std::invalid_argument("invalid DEFLATE bit count");
        }
        while (bits_ < count) {
            if (offset_ >= bytes_.size()) {
                throw std::runtime_error("truncated DEFLATE stream");
            }
            buffer_ |= static_cast<std::uint64_t>(bytes_[offset_++]) << bits_;
            bits_ += 8;
        }
        const std::uint64_t mask = count == 32
            ? 0xffff'ffffULL : ((std::uint64_t{1} << count) - 1);
        const std::uint32_t value = static_cast<std::uint32_t>(buffer_ & mask);
        buffer_ >>= count;
        bits_ -= count;
        return value;
    }

    void align() noexcept {
        buffer_ = 0;
        bits_ = 0;
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t offset_ = 0;
    std::uint64_t buffer_ = 0;
    int bits_ = 0;
};

[[nodiscard]] std::uint16_t reverse_bits(std::uint16_t value,
                                         std::uint8_t count) noexcept {
    std::uint16_t result = 0;
    for (std::uint8_t i = 0; i < count; ++i) {
        result = static_cast<std::uint16_t>((result << 1) | (value & 1u));
        value >>= 1;
    }
    return result;
}

class Huffman {
public:
    void build(std::span<const std::uint8_t> lengths) {
        codes_.clear();
        std::array<std::uint16_t, 16> counts{};
        for (const std::uint8_t length : lengths) {
            if (length > 15) {
                throw std::runtime_error("invalid DEFLATE code length");
            }
            if (length != 0) {
                ++counts[length];
            }
        }
        // Validate the tree at each bit length. Checking the accumulated code
        // against 0x7fff is too strict for an incomplete but valid DEFLATE
        // tree and falsely rejects the dynamic trees emitted by zlib and
        // DeflateStream.
        int remaining = 1;
        for (std::uint8_t length = 1; length <= 15; ++length) {
            remaining = (remaining << 1) - counts[length];
            if (remaining < 0) {
                throw std::runtime_error("oversubscribed DEFLATE tree");
            }
        }

        std::array<std::uint32_t, 16> next{};
        std::uint32_t code = 0;
        for (std::uint8_t length = 1; length <= 15; ++length) {
            code = (code + counts[length - 1]) << 1;
            next[length] = code;
        }
        for (std::uint16_t symbol = 0; symbol < lengths.size(); ++symbol) {
            const std::uint8_t length = lengths[symbol];
            if (length == 0) {
                continue;
            }
            const std::uint16_t canonical = static_cast<std::uint16_t>(
                next[length]++);
            codes_.push_back(Code{
                reverse_bits(canonical, length), length, symbol
            });
        }
        if (codes_.empty()) {
            return;
        }
    }

    [[nodiscard]] std::uint16_t decode(BitReader& reader) const {
        std::uint16_t bits = 0;
        for (std::uint8_t length = 1; length <= 15; ++length) {
            bits = static_cast<std::uint16_t>(
                bits | static_cast<std::uint16_t>(reader.read(1)
                                                   << (length - 1)));
            for (const Code& code : codes_) {
                if (code.length == length && code.bits == bits) {
                    return code.symbol;
                }
            }
        }
        throw std::runtime_error("invalid DEFLATE Huffman code");
    }

private:
    struct Code {
        std::uint16_t bits;
        std::uint8_t length;
        std::uint16_t symbol;
    };

    std::vector<Code> codes_;
};

[[nodiscard]] Huffman fixed_literal_tree() {
    std::array<std::uint8_t, 288> lengths{};
    for (int symbol = 0; symbol <= 143; ++symbol) {
        lengths[symbol] = 8;
    }
    for (int symbol = 144; symbol <= 255; ++symbol) {
        lengths[symbol] = 9;
    }
    for (int symbol = 256; symbol <= 279; ++symbol) {
        lengths[symbol] = 7;
    }
    for (int symbol = 280; symbol < 288; ++symbol) {
        lengths[symbol] = 8;
    }
    Huffman tree;
    tree.build(lengths);
    return tree;
}

[[nodiscard]] Huffman fixed_distance_tree() {
    std::array<std::uint8_t, 32> lengths{};
    lengths.fill(5);
    Huffman tree;
    tree.build(lengths);
    return tree;
}

void append_byte(std::vector<std::uint8_t>& output, std::uint8_t value) {
    if (output.size() >= MaxInflatedBytes) {
        throw std::runtime_error("DEFLATE output exceeds the safety limit");
    }
    output.push_back(value);
}

void append_match(std::vector<std::uint8_t>& output, std::size_t distance,
                  std::size_t length) {
    if (distance == 0 || distance > output.size()) {
        throw std::runtime_error("invalid DEFLATE distance");
    }
    if (length > MaxInflatedBytes - output.size()) {
        throw std::runtime_error("DEFLATE output exceeds the safety limit");
    }
    for (std::size_t i = 0; i < length; ++i) {
        output.push_back(output[output.size() - distance]);
    }
}

void read_dynamic_trees(BitReader& reader, Huffman& literal_tree,
                        Huffman& distance_tree) {
    const int literal_count = static_cast<int>(reader.read(5)) + 257;
    const int distance_count = static_cast<int>(reader.read(5)) + 1;
    const int code_length_count = static_cast<int>(reader.read(4)) + 4;
    static constexpr std::array<int, 19> CodeLengthOrder{
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };
    std::array<std::uint8_t, 19> code_length_lengths{};
    for (int i = 0; i < code_length_count; ++i) {
        code_length_lengths[CodeLengthOrder[i]] =
            static_cast<std::uint8_t>(reader.read(3));
    }
    Huffman code_length_tree;
    code_length_tree.build(code_length_lengths);

    std::vector<std::uint8_t> lengths;
    lengths.reserve(static_cast<std::size_t>(literal_count + distance_count));
    const std::size_t total = lengths.capacity();
    while (lengths.size() < total) {
        const std::uint16_t symbol = code_length_tree.decode(reader);
        if (symbol <= 15) {
            lengths.push_back(static_cast<std::uint8_t>(symbol));
            continue;
        }
        if (symbol == 16) {
            if (lengths.empty()) {
                throw std::runtime_error("DEFLATE repeat has no predecessor");
            }
            const std::size_t repeat = static_cast<std::size_t>(reader.read(2)) + 3;
            if (repeat > total - lengths.size()) {
                throw std::runtime_error("DEFLATE repeat exceeds code lengths");
            }
            lengths.insert(lengths.end(), repeat, lengths.back());
        } else if (symbol == 17) {
            const std::size_t repeat = static_cast<std::size_t>(reader.read(3)) + 3;
            if (repeat > total - lengths.size()) {
                throw std::runtime_error("DEFLATE zero repeat exceeds code lengths");
            }
            lengths.insert(lengths.end(), repeat, 0);
        } else if (symbol == 18) {
            const std::size_t repeat = static_cast<std::size_t>(reader.read(7)) + 11;
            if (repeat > total - lengths.size()) {
                throw std::runtime_error("DEFLATE long zero repeat exceeds code lengths");
            }
            lengths.insert(lengths.end(), repeat, 0);
        } else {
            throw std::runtime_error("invalid DEFLATE code-length symbol");
        }
    }
    literal_tree.build(std::span<const std::uint8_t>(
        lengths.data(), static_cast<std::size_t>(literal_count)));
    distance_tree.build(std::span<const std::uint8_t>(
        lengths.data() + literal_count, static_cast<std::size_t>(distance_count)));
}

[[nodiscard]] std::vector<std::uint8_t> inflate_raw(
    std::span<const std::uint8_t> compressed, bool& had_error) {
    std::vector<std::uint8_t> output;
    BitReader reader(compressed);
    had_error = false;
    try {
        bool final_block = false;
        do {
            final_block = reader.read(1) != 0;
            const std::uint32_t type = reader.read(2);
            if (type == 0) {
                reader.align();
                const std::uint16_t length = static_cast<std::uint16_t>(reader.read(16));
                const std::uint16_t inverse = static_cast<std::uint16_t>(reader.read(16));
                if (static_cast<std::uint16_t>(length ^ 0xffffu) != inverse) {
                    throw std::runtime_error("invalid stored DEFLATE block");
                }
                for (std::uint16_t i = 0; i < length; ++i) {
                    append_byte(output, static_cast<std::uint8_t>(reader.read(8)));
                }
                continue;
            }
            if (type == 3) {
                throw std::runtime_error("reserved DEFLATE block type");
            }

            Huffman literal_tree;
            Huffman distance_tree;
            if (type == 1) {
                literal_tree = fixed_literal_tree();
                distance_tree = fixed_distance_tree();
            } else {
                read_dynamic_trees(reader, literal_tree, distance_tree);
            }

            static constexpr std::array<std::uint16_t, 29> LengthBase{
                3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
                31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
            };
            static constexpr std::array<std::uint8_t, 29> LengthExtra{
                0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
            };
            static constexpr std::array<std::uint32_t, 30> DistanceBase{
                1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                6145, 8193, 12289, 16385, 24577
            };
            static constexpr std::array<std::uint8_t, 30> DistanceExtra{
                0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
            };

            while (true) {
                const std::uint16_t symbol = literal_tree.decode(reader);
                if (symbol < 256) {
                    append_byte(output, static_cast<std::uint8_t>(symbol));
                    continue;
                }
                if (symbol == 256) {
                    break;
                }
                if (symbol < 257 || symbol > 285) {
                    throw std::runtime_error("invalid DEFLATE length symbol");
                }
                const std::size_t length_index = symbol - 257;
                std::size_t length = LengthBase[length_index];
                if (LengthExtra[length_index] != 0) {
                    length += reader.read(LengthExtra[length_index]);
                }
                const std::uint16_t distance_symbol = distance_tree.decode(reader);
                if (distance_symbol >= DistanceBase.size()) {
                    throw std::runtime_error("invalid DEFLATE distance symbol");
                }
                std::size_t distance = DistanceBase[distance_symbol];
                if (DistanceExtra[distance_symbol] != 0) {
                    distance += reader.read(DistanceExtra[distance_symbol]);
                }
                append_match(output, distance, length);
            }
        } while (!final_block);
    } catch (...) {
        // A process can die after a stored/deflate block has been flushed. The
        // managed reader treats the complete prefix as the end of the demo;
        // retain that prefix and expose the diagnostic to callers.
        had_error = true;
    }
    return output;
}

} // namespace

Writer::Writer(const std::filesystem::path& path, std::uint8_t protocol) {
    std::error_code directory_error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), directory_error);
        if (directory_error) {
            throw std::runtime_error("could not create demo directory: "
                                     + path.parent_path().string());
        }
    }
    stream_.open(path, std::ios::binary | std::ios::trunc);
    if (!stream_) {
        throw std::runtime_error("could not create demo file: " + path.string());
    }
    stream_.write(reinterpret_cast<const char*>(Magic.data()),
                  static_cast<std::streamsize>(Magic.size()));
    stream_.put(static_cast<char>(FormatVersion));
    stream_.put(static_cast<char>(protocol));
    if (!stream_) {
        throw std::runtime_error("could not write demo header: " + path.string());
    }
}

Writer::~Writer() {
    try {
        close();
    } catch (...) {
        // Destructors must not terminate a running game when a recording disk
        // becomes unavailable. The caller can use close() for a checked path.
    }
}

void Writer::write_stored_blocks(std::span<const std::uint8_t> bytes) {
    std::size_t offset = 0;
    do {
        const std::size_t count = std::min<std::size_t>(
            bytes.size() - offset, std::numeric_limits<std::uint16_t>::max());
        stream_.put(static_cast<char>(0)); // non-final, stored block
        const auto length = static_cast<std::uint16_t>(count);
        const auto inverse = static_cast<std::uint16_t>(length ^ 0xffffu);
        stream_.put(static_cast<char>(length & 0xffu));
        stream_.put(static_cast<char>((length >> 8) & 0xffu));
        stream_.put(static_cast<char>(inverse & 0xffu));
        stream_.put(static_cast<char>((inverse >> 8) & 0xffu));
        if (count != 0) {
            stream_.write(reinterpret_cast<const char*>(bytes.data() + offset),
                          static_cast<std::streamsize>(count));
        }
        if (!stream_) {
            throw std::runtime_error("could not write demo record");
        }
        offset += count;
    } while (offset < bytes.size());
}

void Writer::write_record(std::uint32_t frame,
                          std::span<const std::uint8_t> data) {
    if (closed_) {
        throw std::logic_error("cannot write a closed demo");
    }
    if (data.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::length_error("demo record exceeds 65535 bytes");
    }
    if (frame < last_frame_) {
        frame = last_frame_;
    }
    const std::uint32_t delta = frame - last_frame_;
    last_frame_ = frame;

    std::vector<std::uint8_t> record;
    record.reserve(data.size() + 7);
    if (delta < LongGap) {
        record.push_back(static_cast<std::uint8_t>(delta));
    } else {
        record.push_back(LongGap);
        put_u32(record, delta);
    }
    put_u16(record, static_cast<std::uint16_t>(data.size()));
    record.insert(record.end(), data.begin(), data.end());
    write_stored_blocks(record);
}

void Writer::close() {
    if (closed_) {
        return;
    }
    // A final empty stored block terminates the raw DEFLATE stream. Records
    // are already complete blocks, so a reader can still recover a prefix if
    // the process dies before this call.
    stream_.put(static_cast<char>(1)); // final, stored block
    stream_.put(static_cast<char>(0));
    stream_.put(static_cast<char>(0));
    stream_.put(static_cast<char>(0xff));
    stream_.put(static_cast<char>(0xff));
    stream_.flush();
    if (!stream_) {
        throw std::runtime_error("could not close demo file");
    }
    closed_ = true;
}

std::optional<Reader> Reader::open(const std::filesystem::path& path) {
    std::vector<std::uint8_t> bytes;
    try {
        bytes = read_all(path);
    } catch (...) {
        return std::nullopt;
    }
    if (bytes.size() < HeaderSize
        || !std::equal(Magic.begin(), Magic.end(), bytes.begin())
        || bytes[4] != FormatVersion) {
        return std::nullopt;
    }
    bool deflate_error = false;
    const auto decompressed = inflate_raw(
        std::span<const std::uint8_t>(bytes).subspan(HeaderSize),
        deflate_error);
    return Reader(bytes[5], decompressed, deflate_error);
}

std::optional<Record> Reader::read_next() {
    if (cursor_ >= decompressed_.size()) {
        return std::nullopt;
    }
    const std::uint8_t short_delta = decompressed_[cursor_++];
    std::uint32_t delta = short_delta;
    if (short_delta == LongGap) {
        if (decompressed_.size() - cursor_ < 4) {
            cursor_ = decompressed_.size();
            return std::nullopt;
        }
        delta = get_u32(decompressed_, cursor_);
        cursor_ += 4;
    }
    if (decompressed_.size() - cursor_ < 2) {
        cursor_ = decompressed_.size();
        return std::nullopt;
    }
    const std::uint16_t length = get_u16(decompressed_, cursor_);
    cursor_ += 2;
    if (decompressed_.size() - cursor_ < length) {
        cursor_ = decompressed_.size();
        return std::nullopt;
    }
    Record record;
    record.frame = frame_ + delta;
    frame_ = record.frame;
    record.data.assign(decompressed_.begin() + static_cast<std::ptrdiff_t>(cursor_),
                       decompressed_.begin() + static_cast<std::ptrdiff_t>(cursor_ + length));
    cursor_ += length;
    return record;
}

} // namespace fruityprime::demo
