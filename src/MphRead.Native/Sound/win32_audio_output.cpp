#include "Sound/win32_audio_output.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

namespace fruityprime::sound {
namespace {

struct Block {
    std::vector<std::int16_t> samples;
    WAVEHDR header{};
    bool queued = false;
};

} // namespace

struct Win32AudioOutput::Impl {
    HWAVEOUT device = nullptr;
    std::vector<std::unique_ptr<Block>> blocks;
    mutable std::mutex mutex;
};

Win32AudioOutput::Win32AudioOutput(std::uint32_t sample_rate,
                                   std::uint32_t block_frames,
                                   std::uint32_t block_count) noexcept
    : impl_(std::make_unique<Impl>()),
      sample_rate_(sample_rate == 0 ? 32728 : sample_rate),
      block_frames_(block_frames == 0 ? 1024 : block_frames),
      block_count_(block_count == 0 ? 4 : block_count) {}

Win32AudioOutput::~Win32AudioOutput() {
    close();
}

bool Win32AudioOutput::open() noexcept {
    if (running_ || impl_ == nullptr) {
        return running_;
    }

    std::lock_guard lock(impl_->mutex);
    if (impl_->device != nullptr) {
        return false;
    }
    if (block_frames_ > std::numeric_limits<DWORD>::max()
        || block_count_ > 64
        || sample_rate_ > std::numeric_limits<DWORD>::max()) {
        return false;
    }

    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = sample_rate_;
    format.wBitsPerSample = 16;
    format.nBlockAlign = static_cast<WORD>(
        format.nChannels * (format.wBitsPerSample / 8));
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    const MMRESULT result = waveOutOpen(
        &impl_->device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) {
        impl_->device = nullptr;
        return false;
    }

    try {
        impl_->blocks.clear();
        impl_->blocks.reserve(block_count_);
        const std::size_t sample_count = static_cast<std::size_t>(
            block_frames_) * 2;
        for (std::uint32_t i = 0; i < block_count_; ++i) {
            auto block = std::make_unique<Block>();
            block->samples.resize(sample_count);
            impl_->blocks.push_back(std::move(block));
            Block& prepared_block = *impl_->blocks.back();
            prepared_block.header.lpData = reinterpret_cast<LPSTR>(
                prepared_block.samples.data());
            prepared_block.header.dwBufferLength = static_cast<DWORD>(
                prepared_block.samples.size() * sizeof(std::int16_t));
            const MMRESULT prepare = waveOutPrepareHeader(
                impl_->device, &prepared_block.header,
                sizeof(prepared_block.header));
            if (prepare != MMSYSERR_NOERROR) {
                throw std::runtime_error("waveOutPrepareHeader failed");
            }
        }
    } catch (...) {
        for (auto& block : impl_->blocks) {
            waveOutUnprepareHeader(impl_->device, &block->header,
                                   sizeof(block->header));
        }
        impl_->blocks.clear();
        waveOutClose(impl_->device);
        impl_->device = nullptr;
        return false;
    }
    running_ = true;
    return true;
}

void Win32AudioOutput::close() noexcept {
    if (impl_ == nullptr) {
        running_ = false;
        return;
    }

    std::lock_guard lock(impl_->mutex);
    running_ = false;
    if (impl_->device == nullptr) {
        impl_->blocks.clear();
        return;
    }
    waveOutReset(impl_->device);
    for (auto& block : impl_->blocks) {
        waveOutUnprepareHeader(impl_->device, &block->header,
                               sizeof(block->header));
        block->queued = false;
    }
    impl_->blocks.clear();
    waveOutClose(impl_->device);
    impl_->device = nullptr;
}

bool Win32AudioOutput::submit(
    std::span<const std::int16_t> samples) noexcept {
    if (!running_ || impl_ == nullptr || samples.empty()) {
        return running_ && samples.empty();
    }
    std::lock_guard lock(impl_->mutex);
    if (!running_ || impl_->device == nullptr) {
        return false;
    }
    // CALLBACK_NULL intentionally avoids driver callback code running while
    // the game is shutting down.  Polling WHDR_DONE here gives the same
    // fixed-queue behaviour and makes the buffer lifetime explicit.
    for (const auto& block : impl_->blocks) {
        if (block->queued && (block->header.dwFlags & WHDR_DONE) != 0) {
            block->queued = false;
        }
    }
    auto found = std::find_if(impl_->blocks.begin(), impl_->blocks.end(),
                              [](const std::unique_ptr<Block>& block) {
        return !block->queued;
    });
    if (found == impl_->blocks.end()) {
        return false;
    }
    Block& block = **found;
    std::fill(block.samples.begin(), block.samples.end(), 0);
    const std::size_t copy_count = std::min(samples.size(),
                                            block.samples.size());
    std::copy_n(samples.begin(), copy_count, block.samples.begin());
    block.header.dwBufferLength = static_cast<DWORD>(
        block.samples.size() * sizeof(std::int16_t));
    block.queued = true;
    const MMRESULT result = waveOutWrite(
        impl_->device, &block.header, sizeof(block.header));
    if (result != MMSYSERR_NOERROR) {
        block.queued = false;
        return false;
    }
    return true;
}

std::size_t Win32AudioOutput::queued_blocks() const noexcept {
    if (impl_ == nullptr) {
        return 0;
    }
    std::lock_guard lock(impl_->mutex);
    return static_cast<std::size_t>(std::count_if(
        impl_->blocks.begin(), impl_->blocks.end(),
        [](const std::unique_ptr<Block>& block) {
            return block->queued;
        }));
}

} // namespace fruityprime::sound

#else

namespace fruityprime::sound {

struct Win32AudioOutput::Impl {};

Win32AudioOutput::Win32AudioOutput(std::uint32_t sample_rate,
                                   std::uint32_t block_frames,
                                   std::uint32_t block_count) noexcept
    : impl_(std::make_unique<Impl>()),
      sample_rate_(sample_rate == 0 ? 32728 : sample_rate),
      block_frames_(block_frames == 0 ? 1024 : block_frames),
      block_count_(block_count == 0 ? 4 : block_count) {}

Win32AudioOutput::~Win32AudioOutput() = default;

bool Win32AudioOutput::open() noexcept {
    running_ = false;
    return false;
}

void Win32AudioOutput::close() noexcept {
    running_ = false;
}

bool Win32AudioOutput::submit(
    std::span<const std::int16_t> samples) noexcept {
    return running_ && samples.empty();
}

std::size_t Win32AudioOutput::queued_blocks() const noexcept {
    return 0;
}

} // namespace fruityprime::sound

#endif
