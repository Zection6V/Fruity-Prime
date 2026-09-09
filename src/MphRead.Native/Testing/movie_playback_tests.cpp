#include "../Formats/movie_playback.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

namespace {

// The same bit writer movie_tests.cpp uses to build a frame the decoder will
// accept: one 16x16 macroblock in 4x4 intra mode, every sub-block inheriting
// DC prediction, which decodes to a neutral gray frame.
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

[[nodiscard]] std::vector<std::uint8_t> synthetic_movie(int frame_count) {
    BitWriter writer;
    writer.unsigned_exp_golomb(15);
    for (int i = 0; i < 16; ++i) {
        writer.bit(1);
    }
    writer.unsigned_exp_golomb(0);
    writer.align_word();
    const std::vector<std::uint8_t> logical_payload = writer.bytes();
    std::vector<std::uint8_t> stored_payload(logical_payload.size());
    for (std::size_t i = 0; i < logical_payload.size(); i += 2) {
        stored_payload[i + 0] = logical_payload[i + 1];
        stored_payload[i + 1] = logical_payload[i + 0];
    }

    constexpr std::size_t header_size = 48;
    constexpr std::size_t frame_record_size = 4;
    const std::size_t frame_size = frame_record_size + stored_payload.size();
    const std::size_t frame_end
        = header_size + frame_size * static_cast<std::size_t>(frame_count);
    const std::size_t extradata_offset = frame_end;
    constexpr std::size_t extradata_size = 3 * 64 * 8 * 2 + 8 * 2 + 8 * 4 + 4;
    const std::size_t seek_offset = extradata_offset + extradata_size;
    std::vector<std::uint8_t> bytes(
        seek_offset + 8 * static_cast<std::size_t>(frame_count), 0);
    bytes[0] = static_cast<std::uint8_t>('V');
    bytes[1] = static_cast<std::uint8_t>('X');
    bytes[2] = static_cast<std::uint8_t>('D');
    bytes[3] = static_cast<std::uint8_t>('S');
    put_i32_le(bytes, 4, frame_count);
    put_i32_le(bytes, 8, 16);
    put_i32_le(bytes, 12, 16);
    put_i32_le(bytes, 16, 15 << 16);
    put_i32_le(bytes, 20, 12);
    put_i32_le(bytes, 24, 0);
    put_i32_le(bytes, 28, 0);
    put_i32_le(bytes, 32, static_cast<std::int32_t>(stored_payload.size() + 2));
    put_i32_le(bytes, 36, static_cast<std::int32_t>(extradata_offset));
    put_i32_le(bytes, 40, static_cast<std::int32_t>(seek_offset));
    put_i32_le(bytes, 44, frame_count);
    for (int index = 0; index < frame_count; ++index) {
        const std::size_t record = header_size
            + frame_size * static_cast<std::size_t>(index);
        put_u16_le(bytes, record,
                   static_cast<std::uint16_t>(stored_payload.size() + 2));
        put_u16_le(bytes, record + 2, 0);
        std::copy(stored_payload.begin(), stored_payload.end(),
                  bytes.begin()
                      + static_cast<std::ptrdiff_t>(record + frame_record_size));
        put_i32_le(bytes, seek_offset + 8 * static_cast<std::size_t>(index),
                   index);
        put_i32_le(bytes, seek_offset + 8 * static_cast<std::size_t>(index) + 4,
                   static_cast<std::int32_t>(record));
    }
    return bytes;
}

[[nodiscard]] fruityprime::movie::Player make_player(int frame_count) {
    return fruityprime::movie::Player::from_bytes(synthetic_movie(frame_count));
}

using fruityprime::movie::AfterMovie;
using fruityprime::movie::MoviePlayback;

// Nothing is playing until a movie is started, and stopping puts it back.
void test_idle_state() {
    MoviePlayback playback;
    assert(!playback.movie_playing());
    assert(playback.frame_index() == -1);
    assert(playback.frames_queued() == 0);
    assert(!playback.ready_to_show());
    // Advancing an idle player must not walk the frame index off -1 -- that is
    // what makes MoviePlaying answer wrongly for the rest of the session.
    assert(!playback.advance());
    assert(playback.frame_index() == -1);
    playback.update_images();
    assert(playback.top_image().empty());
}

// A dual-screen movie shows nothing until both decoders have queued enough,
// and the screen that is furthest behind is the one that decides.
void test_preroll_gate() {
    MoviePlayback playback;
    MoviePlayback::Settings settings;
    settings.movie_id = 3;
    settings.after_movie_id = 12;
    settings.after_movie_action = AfterMovie::LoadRoom;
    playback.start_movie(make_player(6), make_player(6), settings);

    assert(playback.movie_playing());
    assert(playback.dual_screen());
    assert(playback.frame_total() == 6);
    assert(playback.settings().after_movie_id == 12);
    assert(playback.frames_queued() == 6);
    assert(playback.ready_to_show());

    // Three frames left is below the preroll, which is the state the first
    // frames of a real movie are in while the decoder catches up.
    for (int i = 0; i < 3; ++i) {
        assert(playback.advance());
    }
    assert(playback.frames_queued() == 3);
    assert(!playback.ready_to_show());
}

// A movie whose two screens disagree on length plays the shorter one, so
// neither decoder is ever read past its end.
void test_mismatched_lengths() {
    MoviePlayback playback;
    playback.start_movie(make_player(6), make_player(4),
                         MoviePlayback::Settings{});
    assert(playback.frame_total() == 4);
    assert(playback.frames_queued() == 4);
}

// A single-screen movie has no bottom image at all, rather than a black one.
void test_single_screen() {
    MoviePlayback playback;
    playback.start_movie(make_player(3), std::nullopt,
                         MoviePlayback::Settings{});
    assert(!playback.dual_screen());
    assert(playback.bottom_image().empty());
    playback.update_images();
    assert(playback.top_image().size() == 16U * 16U * 3U);
    assert(playback.bottom_image().empty());
}

// advance() returns false exactly once, at the end -- that false is what runs
// the after-movie action, so a movie that kept returning true would never load
// the room behind it.
void test_runs_to_end() {
    MoviePlayback playback;
    MoviePlayback::Settings settings;
    settings.after_movie_action = AfterMovie::StartGame;
    playback.start_movie(make_player(4), make_player(4), settings);

    int advanced = 0;
    while (playback.advance()) {
        ++advanced;
        assert(advanced < 100);
    }
    // Four frames means three advances that land on a frame and one that ends.
    assert(advanced == 3);
    assert(!playback.movie_playing());
    assert(playback.frame_index() == -1);
    assert(playback.movie_audio_handle() == -1);
    // The settings survive the movie, because the caller reads them after it
    // has ended to decide what to do next.
    assert(playback.settings().after_movie_action == AfterMovie::StartGame);
}

// Skipping ends the movie on the next frame rather than immediately, which is
// what leaves the after-movie action to run normally.
void test_skip() {
    MoviePlayback playback;
    playback.start_movie(make_player(60), make_player(60),
                         MoviePlayback::Settings{});
    assert(playback.advance());
    assert(!playback.skipped());
    playback.skip_movie();
    assert(playback.skipped());
    assert(!playback.advance());
    assert(!playback.movie_playing());

    // Starting another movie clears the skip, or the next cutscene would be
    // skipped by a button pressed during the previous one.
    playback.start_movie(make_player(4), std::nullopt,
                         MoviePlayback::Settings{});
    assert(!playback.skipped());
    assert(playback.advance());
}

// The image buffers follow the frame the movie is on, not the one it started
// on, and the audio handle is the host's to set and the movie's to clear.
void test_images_and_audio_handle() {
    MoviePlayback playback;
    playback.start_movie(make_player(5), make_player(5),
                         MoviePlayback::Settings{});
    playback.set_movie_audio_handle(7);
    assert(playback.movie_audio_handle() == 7);

    // Sized before anything is decoded into them, so the host can upload a
    // texture on the first frame without a special case.
    assert(playback.top_image().size() == 16U * 16U * 3U);
    assert(playback.bottom_image().size() == 16U * 16U * 3U);

    assert(playback.advance());
    playback.update_images();
    assert(playback.top_image().size() == 16U * 16U * 3U);
    assert(playback.bottom_image().size() == 16U * 16U * 3U);
    for (std::uint8_t value : playback.top_image()) {
        assert(value == 128);
    }

    playback.stop();
    assert(playback.movie_audio_handle() == -1);
}

} // namespace

int main() {
    test_idle_state();
    test_preroll_gate();
    test_mismatched_lengths();
    test_single_screen();
    test_runs_to_end();
    test_skip();
    test_images_and_audio_handle();
    std::cout << "movie playback tests passed\n";
    return 0;
}
