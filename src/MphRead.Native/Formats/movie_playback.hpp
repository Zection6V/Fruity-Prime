#pragma once

// Native counterpart of the Scene-side cutscene playback in Formats/Movie.cs.
//
// A DS cutscene is two VX files, one per screen, decoded independently and
// shown in lockstep.  The managed Scene keeps a decoder per screen, waits
// until each has queued a few frames before showing anything -- otherwise the
// first frames stutter while the decoder catches up -- and remembers what to
// do when the movie ends, because a landing movie loads a room and an ending
// movie does not.

#include "Formats/movie.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fruityprime::movie {

// Movie.AfterMovie: what happens once the cutscene finishes.
enum class AfterMovie : std::uint8_t { StartGame, LoadRoom, EndGame };

class MoviePlayback final {
public:
    static constexpr int FrameWidth = 256;
    static constexpr int FrameHeight = 192;
    // The managed code waits for this many queued frames before it shows the
    // first one.
    static constexpr std::size_t PrerollFrames = 4;

    struct Settings {
        int movie_id = -1;
        int after_movie_id = -1;
        AfterMovie after_movie_action = AfterMovie::LoadRoom;
        float fade_to_length = 0.0F;
        float fade_from_length = 0.0F;
    };

    // Scene.MoviePlaying: a frame index of -1 means nothing is playing.
    [[nodiscard]] bool movie_playing() const noexcept {
        return frame_index_ != -1;
    }
    [[nodiscard]] int frame_index() const noexcept { return frame_index_; }
    [[nodiscard]] int frame_total() const noexcept { return frame_total_; }
    [[nodiscard]] bool dual_screen() const noexcept { return dual_screen_; }
    [[nodiscard]] const Settings& settings() const noexcept {
        return settings_;
    }
    // Scene.MovieAudioHandle: -1 when no audio stream is open.
    [[nodiscard]] int movie_audio_handle() const noexcept {
        return audio_handle_;
    }
    void set_movie_audio_handle(int handle) noexcept { audio_handle_ = handle; }

    // Scene.StartMovie / StartMovies.  `bottom` is absent for a movie that
    // only uses the top screen.
    void start_movie(Player top, std::optional<Player> bottom,
                     const Settings& settings);
    void start_movies(Player top, std::optional<Player> bottom,
                      const Settings& settings) {
        start_movie(std::move(top), std::move(bottom), settings);
    }

    // Scene.SkipMovie: the player pressed through it.  The movie stops at the
    // end rather than immediately, so the after-movie action still runs.
    void skip_movie() noexcept { skip_ = true; }
    [[nodiscard]] bool skipped() const noexcept { return skip_; }

    // VxDecoder.FramesQueued, across both screens: how many frames are ready
    // on the screen that is furthest behind.
    [[nodiscard]] std::size_t frames_queued() const noexcept;

    // True once both screens have queued enough to start without stuttering.
    [[nodiscard]] bool ready_to_show() const noexcept {
        return frames_queued() >= PrerollFrames;
    }

    // Advance by one frame; returns false once the movie has ended, which is
    // when the caller runs the after-movie action.
    [[nodiscard]] bool advance() noexcept;

    void stop() noexcept;

    // Scene._topImageBuffer / _botImageBuffer, as RGB triples.
    [[nodiscard]] const std::vector<std::uint8_t>& top_image() const noexcept {
        return top_image_;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& bottom_image()
        const noexcept {
        return bottom_image_;
    }

    // Refresh the image buffers from the decoders at the current frame.
    void update_images();

private:
    std::optional<Player> top_;
    std::optional<Player> bottom_;
    Settings settings_{};
    int frame_index_ = -1;
    int frame_total_ = 0;
    int audio_handle_ = -1;
    bool dual_screen_ = true;
    bool skip_ = false;
    std::vector<std::uint8_t> top_image_;
    std::vector<std::uint8_t> bottom_image_;
};

} // namespace fruityprime::movie
