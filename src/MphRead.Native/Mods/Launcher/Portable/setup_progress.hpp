#pragma once

#include <string>
#include <string_view>

namespace fruityprime::launcher {

// Progress is derived from the same textual milestones emitted by the
// managed extractor.  Extraction does not know its total file count up front,
// so this deliberately reports a monotonic, phase-based estimate.
class SetupProgress {
public:
    [[nodiscard]] bool observe(std::string_view line);
    void finish(bool ok) noexcept;

    [[nodiscard]] double fraction() const noexcept { return fraction_; }
    [[nodiscard]] const std::string& stage() const noexcept { return stage_; }
    [[nodiscard]] bool done() const noexcept { return done_; }
    [[nodiscard]] std::string bar(int width = 28) const;

private:
    struct Band {
        double start = 0.0;
        double end = 0.03;
        double scale = 1.0;
        std::string stage = "Starting";
    };

    [[nodiscard]] static bool preview_count(std::string_view line,
                                             int& done, int& total);
    [[nodiscard]] Band classify(std::string_view line) const;
    bool set(double fraction, std::string stage);

    Band band_{};
    int seen_ = 0;
    double fraction_ = 0.0;
    std::string stage_ = "Starting";
    bool done_ = false;
};

} // namespace fruityprime::launcher
