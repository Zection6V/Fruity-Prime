#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace fruityprime::sound {

// Small platform boundary for the native SFX mixer.  The mixer produces
// signed interleaved stereo PCM; this class owns only the Windows output
// queue, so gameplay and the resource readers remain platform independent.
// On non-Windows builds the object is a safe unavailable sink.
class Win32AudioOutput final {
public:
    explicit Win32AudioOutput(std::uint32_t sample_rate = 32728,
                              std::uint32_t block_frames = 1024,
                              std::uint32_t block_count = 4) noexcept;
    ~Win32AudioOutput();

    Win32AudioOutput(const Win32AudioOutput&) = delete;
    Win32AudioOutput& operator=(const Win32AudioOutput&) = delete;

    [[nodiscard]] bool open() noexcept;
    void close() noexcept;

    // Queues one interleaved stereo block.  A shorter block is zero padded;
    // a longer block is clipped to the configured block size.  Returning
    // false means the device is unavailable or all blocks are still queued.
    [[nodiscard]] bool submit(std::span<const std::int16_t> samples) noexcept;

    [[nodiscard]] bool running() const noexcept { return running_; }
    [[nodiscard]] std::uint32_t sample_rate() const noexcept {
        return sample_rate_;
    }
    [[nodiscard]] std::uint32_t block_frames() const noexcept {
        return block_frames_;
    }
    [[nodiscard]] std::size_t queued_blocks() const noexcept;

private:
    struct Impl;

    std::unique_ptr<Impl> impl_;
    std::uint32_t sample_rate_;
    std::uint32_t block_frames_;
    std::uint32_t block_count_;
    bool running_ = false;
};

} // namespace fruityprime::sound
