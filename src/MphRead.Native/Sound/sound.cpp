#include "sound.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace MphReadNative::Sound {

bool Mixer::start() noexcept {
    if (running_) {
        return true;
    }
    if (!configuration_.enabled) {
        return false;
    }
    if (configuration_.backend == Backend::Null) {
        running_ = true;
        return true;
    }
    if (configuration_.backend != Backend::WinMm) {
        return false;
    }
    try {
        if (output_ == nullptr) {
            output_ = std::make_unique<Win32AudioOutput>();
        }
        running_ = output_->open();
    } catch (...) {
        output_.reset();
        running_ = false;
    }
    return running_;
}

void Mixer::stop() noexcept {
    if (output_ != nullptr) {
        output_->close();
    }
    running_ = false;
}

bool Mixer::submit(std::span<const std::int16_t> samples) noexcept {
    if (!running_) {
        return false;
    }
    bool accepted = true;
    if (configuration_.backend == Backend::WinMm) {
        if (output_ == nullptr) {
            return false;
        }
        if (configuration_.master_gain == 1.0F || samples.empty()) {
            accepted = output_->submit(samples);
        } else {
            std::vector<std::int16_t> adjusted(samples.size());
            const float gain = std::max(0.0F, configuration_.master_gain);
            for (std::size_t index = 0; index < samples.size(); ++index) {
                const float value = static_cast<float>(samples[index]) * gain;
                adjusted[index] = static_cast<std::int16_t>(std::clamp(
                    value, static_cast<float>(std::numeric_limits<
                        std::int16_t>::min()), static_cast<float>(
                            std::numeric_limits<std::int16_t>::max())));
            }
            accepted = output_->submit(adjusted);
        }
    }
    if (!accepted) {
        return false;
    }
    const auto remaining = std::numeric_limits<std::size_t>::max()
        - submitted_samples_;
    submitted_samples_ += samples.size() > remaining ? remaining
                                                      : samples.size();
    return true;
}

} // namespace MphReadNative::Sound
