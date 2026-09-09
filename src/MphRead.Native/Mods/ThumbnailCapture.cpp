#include "Mods/thumbnail_capture.hpp"

namespace fruityprime::mods::thumbnail {

bool CaptureState::capture_due() noexcept {
    if (captured_ || gave_up_) {
        return false;
    }
    if (settle_frames_ > 0) {
        --settle_frames_;
        return false;
    }
    return true;
}

CaptureFrameResult CaptureState::finish_capture(
    bool saved, bool window_visible) noexcept {
    CaptureFrameResult result;
    if (captured_ || gave_up_) {
        result.captured = captured_;
        result.close_window = captured_ || gave_up_;
        result.attempt = attempts_;
        return result;
    }

    ++attempts_;
    result.attempt = attempts_;
    if (saved) {
        captured_ = true;
        result.captured = true;
        result.close_window = true;
        return result;
    }

    // This is intentionally set even on the final failed attempt.  It is the
    // managed behavior: a hidden surface is made visible before the host
    // closes, which gives a platform head a chance to report the real cause.
    result.show_window = !window_visible;
    settle_frames_ = RetryFrames;
    if (attempts_ >= MaxAttempts) {
        gave_up_ = true;
        result.close_window = true;
    }
    return result;
}

} // namespace fruityprime::mods::thumbnail
