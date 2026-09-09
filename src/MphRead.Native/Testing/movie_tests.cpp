#include "Formats/movie.hpp"

#include "Export/export.hpp"
#include "Assets/nds_rom.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

class BitWriter {
public:
    void bit(unsigned value) {
        if (bit_count_ % 8U == 0U) {
            bytes_.push_back(0);
        }
        if (value != 0U) {
            bytes_.back() |= static_cast<std::uint8_t>(
                1U << (7U - bit_count_ % 8U));
        }
        ++bit_count_;
    }

    void unsigned_exp_golomb(unsigned value) {
        const unsigned code = value + 1U;
        unsigned width = 0;
        for (unsigned remaining = code; remaining > 1U; remaining >>= 1U) {
            ++width;
        }
        for (unsigned i = 0; i < width; ++i) {
            bit(0);
        }
        for (int i = static_cast<int>(width); i >= 0; --i) {
            bit((code >> static_cast<unsigned>(i)) & 1U);
        }
    }

    void align_word() {
        while (bit_count_ % 16U != 0U) {
            bit(0);
        }
    }

    [[nodiscard]] std::vector<std::uint8_t> bytes() const { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
    unsigned bit_count_ = 0;
};

void put_u16_le(std::vector<std::uint8_t>& bytes, std::size_t offset,
                std::uint16_t value) {
    bytes[offset + 0] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put_i32_le(std::vector<std::uint8_t>& bytes, std::size_t offset,
                std::int32_t value) {
    const auto unsigned_value = static_cast<std::uint32_t>(value);
    bytes[offset + 0] = static_cast<std::uint8_t>(unsigned_value);
    bytes[offset + 1] = static_cast<std::uint8_t>(unsigned_value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(unsigned_value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(unsigned_value >> 24);
}

[[nodiscard]] std::vector<std::uint8_t> synthetic_movie() {
    BitWriter writer;
    // One 16x16 macroblock: mode 15 selects 4x4 prediction.  The sixteen
    // inherited mode bits all retain DC mode 2, and the following UE(0)
    // selects DC prediction for both chroma planes.  The result is a neutral
    // 128 gray frame and exercises the same word alignment as a real frame.
    writer.unsigned_exp_golomb(15);
    for (int i = 0; i < 16; ++i) {
        writer.bit(1);
    }
    writer.unsigned_exp_golomb(0);
    writer.align_word();
    std::vector<std::uint8_t> logical_payload = writer.bytes();
    std::vector<std::uint8_t> stored_payload(logical_payload.size());
    for (std::size_t i = 0; i < logical_payload.size(); i += 2) {
        stored_payload[i + 0] = logical_payload[i + 1];
        stored_payload[i + 1] = logical_payload[i + 0];
    }

    constexpr std::size_t header_size = 48;
    constexpr std::size_t frame_record_size = 4;
    const std::size_t frame_end = header_size + frame_record_size
        + stored_payload.size();
    const std::size_t extradata_offset = frame_end;
    constexpr std::size_t extradata_size = 3 * 64 * 8 * 2 + 8 * 2 + 8 * 4 + 4;
    const std::size_t seek_offset = extradata_offset + extradata_size;
    std::vector<std::uint8_t> bytes(seek_offset + 8, 0);
    bytes[0] = 'V';
    bytes[1] = 'X';
    bytes[2] = 'D';
    bytes[3] = 'S';
    put_i32_le(bytes, 4, 1);       // frame count
    put_i32_le(bytes, 8, 16);      // width
    put_i32_le(bytes, 12, 16);     // height
    put_i32_le(bytes, 16, 15 << 16); // frame rate
    put_i32_le(bytes, 20, 12);     // quantizer
    put_i32_le(bytes, 24, 0);      // no audio sample stream
    put_i32_le(bytes, 28, 0);      // audio stream count
    put_i32_le(bytes, 32, static_cast<std::int32_t>(stored_payload.size() + 2));
    put_i32_le(bytes, 36, static_cast<std::int32_t>(extradata_offset));
    put_i32_le(bytes, 40, static_cast<std::int32_t>(seek_offset));
    put_i32_le(bytes, 44, 1);      // seek table count
    put_u16_le(bytes, header_size, static_cast<std::uint16_t>(
        stored_payload.size() + 2));
    put_u16_le(bytes, header_size + 2, 0);
    std::copy(stored_payload.begin(), stored_payload.end(),
              bytes.begin() + header_size + frame_record_size);
    put_i32_le(bytes, seek_offset, 0);
    put_i32_le(bytes, seek_offset + 4, static_cast<std::int32_t>(header_size));
    return bytes;
}

template <typename Function>
void assert_throws(Function&& function) {
    bool thrown = false;
    try {
        function();
    } catch (const std::exception&) {
        thrown = true;
    }
    assert(thrown);
}

void test_synthetic_movie() {
    auto decoder = fruityprime::movie::VxDecoder::from_bytes(synthetic_movie());
    const std::array<char, 4> expected_magic{'V', 'X', 'D', 'S'};
    const std::array<int, 3> expected_quantizer{40, 52, 64};
    assert(decoder.header().magic == expected_magic);
    assert(decoder.header().frame_count == 1);
    assert(decoder.header().frame_rate == 15.0F);
    assert(decoder.seek_table().size() == 1);
    assert(decoder.seek_table()[0].frame_offset == 48);
    assert(decoder.quantizer_table() == expected_quantizer);
    decoder.decode();
    assert(decoder.frame_count() == 1);
    assert(decoder.audio_frame_total() == 0);
    const auto image = decoder.image_rgb(0);
    assert(image.size() == 16U * 16U * 3U);
    for (std::uint8_t value : image) {
        assert(value == 128);
    }
    assert(decoder.audio_samples().empty());
    decoder.reset();
    assert(decoder.frame_count() == 0);
    assert_throws([&decoder]() { static_cast<void>(decoder.image_rgb(0)); });

    const auto invalid = [] {
        auto bytes = synthetic_movie();
        bytes[0] = 'N';
        return bytes;
    }();
    assert_throws([&invalid]() {
        static_cast<void>(fruityprime::movie::VxDecoder::from_bytes(invalid));
    });

    fruityprime::movie::VideoFrame frame{
        16, 16, std::vector<std::uint8_t>(16 * 16, 128),
        std::vector<std::uint8_t>(8 * 8, 128),
        std::vector<std::uint8_t>(8 * 8, 128)
    };
    const auto pixel = frame.pixel_rgb(0, 0);
    assert(pixel.red == 128 && pixel.green == 128 && pixel.blue == 128);

    decoder.decode();
    const auto output = std::filesystem::temp_directory_path()
        / ("fruity-prime-native-movie-test-"
           + std::to_string(std::chrono::steady_clock::now()
                               .time_since_epoch().count()));
    const auto export_stats = fruityprime::exporter::write_movie(
        decoder, output);
    assert(export_stats.frames == 1 && export_stats.audio_frames == 0);
    const auto png = output / "0000.png";
    assert(std::filesystem::is_regular_file(png));
    std::ifstream png_input(png, std::ios::binary);
    std::array<std::uint8_t, 8> png_magic{};
    png_input.read(reinterpret_cast<char*>(png_magic.data()),
                   static_cast<std::streamsize>(png_magic.size()));
    constexpr std::array<std::uint8_t, 8> expected_png_magic{
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a
    };
    assert(png_magic == expected_png_magic);
    png_input.close();
    std::error_code cleanup_error;
    std::filesystem::remove_all(output, cleanup_error);
    assert(!cleanup_error);

    auto player = fruityprime::movie::Player::from_bytes(synthetic_movie());
    assert(player.state() == fruityprime::movie::PlaybackState::Stopped);
    player.play();
    assert(player.state() == fruityprime::movie::PlaybackState::Playing);
    player.update(0.02);
    assert(player.state() == fruityprime::movie::PlaybackState::Playing);
    player.pause();
    player.update(1.0);
    assert(player.state() == fruityprime::movie::PlaybackState::Paused);
    player.play();
    player.update(0.1);
    assert(player.state() == fruityprime::movie::PlaybackState::Finished);
    assert(player.frame_index() == 0);
    assert(player.image_rgb().size() == 16U * 16U * 3U);
    player.reset();
    assert(player.state() == fruityprime::movie::PlaybackState::Stopped);
    assert(player.image_rgb().size() == 16U * 16U * 3U);
}

void test_real_rom_movie() {
    const char* configured = std::getenv("FRUITY_PRIME_TEST_NDS");
    if (configured == nullptr || *configured == '\0') {
        std::cout << "real VX movie test skipped: set FRUITY_PRIME_TEST_NDS\n";
        return;
    }
    const auto rom = fruityprime::nds::Rom::read_file(configured);
    const auto& files = rom.files();
    const fruityprime::nds::FileEntry* movie = nullptr;
    for (const auto& file : files) {
        if (file.path.size() >= 3
            && file.path.rfind("movies/", 0) == 0
            && file.path.ends_with(".vx")) {
            movie = &file;
            break;
        }
    }
    assert(movie != nullptr);
    auto decoder = fruityprime::movie::VxDecoder::from_bytes(
        rom.file(movie->file_id));
    decoder.decode();
    assert(decoder.frame_count() ==
           static_cast<std::size_t>(decoder.header().frame_count));
    assert(!decoder.image_rgb(0).empty());
    std::cout << "real VX movie: " << movie->path
              << ", frames=" << decoder.frame_count()
              << ", audio_frames=" << decoder.audio_frame_total() << '\n';
}

} // namespace

int main() {
    test_synthetic_movie();
    test_real_rom_movie();
    std::cout << "movie tests passed\n";
    return 0;
}
