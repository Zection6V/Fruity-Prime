#include "movie_playback.hpp"

#include <algorithm>
#include <utility>

namespace fruityprime::movie {

namespace {

// A movie's frames may be any size; the DS screen constants say what a
// cartridge movie is, not what every decoder will hand back.
[[nodiscard]] std::size_t screen_bytes(const Player& player) noexcept {
    const auto& header = player.decoder().header();
    const auto width = static_cast<std::size_t>(header.frame_width);
    const auto height = static_cast<std::size_t>(header.frame_height);
    if (width == 0 || height == 0) {
        return static_cast<std::size_t>(MoviePlayback::FrameWidth)
            * MoviePlayback::FrameHeight * 3;
    }
    return width * height * 3;
}

// How many frames a decoder still has ahead of where it is playing.
[[nodiscard]] std::size_t queued_ahead(const Player& player) noexcept {
    const std::size_t count = player.frame_count();
    const std::size_t index = player.frame_index();
    return index >= count ? 0 : count - index;
}

} // namespace

void MoviePlayback::start_movie(Player top, std::optional<Player> bottom,
                                const Settings& settings) {
    top_.emplace(std::move(top));
    bottom_ = std::move(bottom);
    dual_screen_ = bottom_.has_value();
    settings_ = settings;
    skip_ = false;
    frame_index_ = 0;
    // The managed code asserts the two screens agree on length; the shorter
    // one is used so neither decoder is read past its end.
    frame_total_ = static_cast<int>(top_->frame_count());
    if (bottom_.has_value()) {
        frame_total_ = std::min(
            frame_total_, static_cast<int>(bottom_->frame_count()));
    }
    // The managed buffers are a fixed DS screen; here they are sized from the
    // decoder's own header, so a movie that is not 256x192 -- a test fixture,
    // or a replacement asset -- does not hand the host a buffer whose size
    // disagrees with the pixels in it.
    top_image_.assign(screen_bytes(*top_), 0);
    if (bottom_.has_value()) {
        bottom_image_.assign(screen_bytes(*bottom_), 0);
    } else {
        bottom_image_.clear();
    }
    top_->play();
    if (bottom_.has_value()) {
        bottom_->play();
    }
}

std::size_t MoviePlayback::frames_queued() const noexcept {
    if (!top_.has_value()) {
        return 0;
    }
    // The screen that is furthest behind decides whether playback can start:
    // showing a frame the other screen cannot match is the stutter the preroll
    // exists to avoid.
    std::size_t queued = queued_ahead(*top_);
    if (bottom_.has_value()) {
        queued = std::min(queued, queued_ahead(*bottom_));
    }
    return queued;
}

bool MoviePlayback::advance() noexcept {
    if (!movie_playing()) {
        return false;
    }
    if (skip_) {
        stop();
        return false;
    }
    ++frame_index_;
    if (frame_index_ >= frame_total_) {
        stop();
        return false;
    }
    top_->seek(static_cast<std::size_t>(frame_index_));
    if (bottom_.has_value()) {
        bottom_->seek(static_cast<std::size_t>(frame_index_));
    }
    return true;
}

void MoviePlayback::update_images() {
    if (!top_.has_value()) {
        return;
    }
    top_image_ = top_->image_rgb();
    if (bottom_.has_value()) {
        bottom_image_ = bottom_->image_rgb();
    }
}

void MoviePlayback::stop() noexcept {
    if (top_.has_value()) {
        top_->stop();
    }
    if (bottom_.has_value()) {
        bottom_->stop();
    }
    // -1 is what MoviePlaying reads to mean "nothing is playing".
    frame_index_ = -1;
    audio_handle_ = -1;
}

} // namespace fruityprime::movie
