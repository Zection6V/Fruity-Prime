#include "Mods/Launcher/Portable/setup_progress.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <string>
#include <utility>

namespace fruityprime::launcher {
namespace {

[[nodiscard]] bool starts_with(std::string_view value,
                               std::string_view prefix) noexcept {
    return value.size() >= prefix.size()
        && value.substr(0, prefix.size()) == prefix;
}

} // namespace

bool SetupProgress::preview_count(std::string_view line, int& done,
                                  int& total) {
    constexpr std::string_view Prefix = "[thumbnails] ";
    if (!starts_with(line, Prefix)) {
        return false;
    }
    const std::string_view rest = line.substr(Prefix.size());
    const std::size_t slash = rest.find('/');
    if (slash == 0 || slash == std::string_view::npos) {
        return false;
    }
    const std::string_view done_text = rest.substr(0, slash);
    const std::string_view after = rest.substr(slash + 1);
    std::size_t count = 0;
    while (count < after.size()
           && after[count] >= '0' && after[count] <= '9') {
        ++count;
    }
    if (count == 0) {
        return false;
    }
    const auto parse = [](std::string_view value, int& output) {
        const auto result = std::from_chars(value.data(),
                                             value.data() + value.size(),
                                             output);
        return result.ec == std::errc{}
            && result.ptr == value.data() + value.size();
    };
    return parse(done_text, done) && parse(after.substr(0, count), total);
}

SetupProgress::Band SetupProgress::classify(std::string_view line) const {
    const Band files{0.03, 0.40, 45.0, "Writing game files"};
    const Band archives{0.40, 0.57, 25.0, "Unpacking archives"};
    const Band sound{0.57, 0.63, 3.0, "Converting music"};
    const Band binaries{0.63, 0.72, 6.0, "Decompressing code"};
    if (starts_with(line, "Writing ")) {
        return files;
    }
    if (starts_with(line, "Reading ") || starts_with(line, "Extracted ")) {
        return archives;
    }
    if (starts_with(line, "Converting ")) {
        return sound;
    }
    if (starts_with(line, "Decompressing ")) {
        return binaries;
    }
    return band_;
}

bool SetupProgress::observe(std::string_view line) {
    if (done_) {
        return false;
    }
    int done = 0;
    int total = 0;
    if (preview_count(line, done, total) && total > 0) {
        band_ = Band{0.72, 1.0, 1.0,
                     "Rendering map previews (" + std::to_string(done)
                         + "/" + std::to_string(total) + ")"};
        return set(0.72 + 0.28 * static_cast<double>(done)
                       / static_cast<double>(total), band_.stage);
    }

    const Band next = classify(line);
    if (next.stage != band_.stage) {
        band_ = next;
        seen_ = 0;
    }
    ++seen_;
    const double eased = 1.0 - std::exp(-static_cast<double>(seen_)
                                        / band_.scale);
    return set(band_.start + (band_.end - band_.start) * eased,
               band_.stage);
}

void SetupProgress::finish(bool ok) noexcept {
    done_ = true;
    fraction_ = 1.0;
    stage_ = ok ? "Ready to play" : "Setup did not finish";
}

bool SetupProgress::set(double fraction, std::string stage) {
    const double clamped = std::clamp(fraction, 0.0, 0.99);
    const bool changed = clamped > fraction_ + 0.0005 || stage != stage_;
    fraction_ = std::max(fraction_, clamped);
    stage_ = std::move(stage);
    return changed;
}

std::string SetupProgress::bar(int width) const {
    width = std::max(width, 0);
    const int filled = static_cast<int>(std::round(fraction_ * width));
    std::string result;
    result.reserve(static_cast<std::size_t>(width) + 8);
    result.push_back('[');
    result.append(static_cast<std::size_t>(std::max(filled, 0)), '#');
    result.append(static_cast<std::size_t>(std::max(0, width - filled)), '-');
    result += "] ";
    result += std::to_string(static_cast<int>(std::round(fraction_ * 100.0)));
    result += '%';
    return result;
}

} // namespace fruityprime::launcher
