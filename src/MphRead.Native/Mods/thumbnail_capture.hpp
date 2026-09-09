#pragma once

namespace fruityprime::mods::thumbnail {

// Frame scheduling and retry policy shared by desktop and mobile thumbnail
// hosts.  The managed ThumbnailCapture owns a real GameWindow; this class
// keeps the stateful part independent of OpenTK/GLFW so every native host
// makes the same capture decision and handles a hidden-window retry alike.
struct CaptureFrameResult final {
    bool captured = false;
    bool show_window = false;
    bool close_window = false;
    int attempt = 0;
};

class CaptureState final {
public:
    static constexpr int SettleFrames = 12;
    static constexpr int RetryFrames = 20;
    static constexpr int MaxAttempts = 3;

    CaptureState() noexcept = default;

    // Call once after each scene update.  The first 12 frames only let the
    // intro camera/fade settle; a failed capture gets the same 20-frame
    // delay before the next eligible frame.
    [[nodiscard]] bool capture_due() noexcept;

    // Submit the result of the eligible frame.  `window_visible` is sampled
    // before the host changes visibility, matching the managed retry path.
    // The caller must invoke this only after capture_due() returned true.
    [[nodiscard]] CaptureFrameResult finish_capture(
        bool saved, bool window_visible) noexcept;

    [[nodiscard]] bool captured() const noexcept { return captured_; }
    [[nodiscard]] bool gave_up() const noexcept { return gave_up_; }
    [[nodiscard]] int attempts() const noexcept { return attempts_; }
    [[nodiscard]] int pending_frames() const noexcept {
        return settle_frames_ > 0 ? settle_frames_ : 0;
    }

private:
    int settle_frames_ = SettleFrames;
    int attempts_ = 0;
    bool captured_ = false;
    bool gave_up_ = false;
};

} // namespace fruityprime::mods::thumbnail
