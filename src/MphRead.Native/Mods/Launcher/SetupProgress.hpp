#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace MphRead::Mods::Launcher
{
    class SetupProgress final
    {
    public:
        SetupProgress() = default;
        SetupProgress(const SetupProgress&) = delete;
        SetupProgress& operator=(const SetupProgress&) = delete;
        SetupProgress(SetupProgress&&) = delete;
        SetupProgress& operator=(SetupProgress&&) = delete;

        [[nodiscard]] double Fraction() const noexcept;
        [[nodiscard]] const std::string& Stage() const noexcept;
        [[nodiscard]] bool Done() const noexcept;

        bool Observe(const std::string& line);
        void Finish(bool ok);
        [[nodiscard]] std::string Bar(std::int32_t width = 28) const;

    private:
        struct Band final
        {
            Band(double start, double end, double scale, std::string stage);

            double Start;
            double End;
            double Scale;
            std::string Stage;
        };

        static const Band _files;
        static const Band _archives;
        static const Band _sound;
        static const Band _binaries;
        static constexpr double _previewStart = 0.72;

        Band _band{0, 0.03, 1, "Starting"};
        std::int32_t _seen = 0;
        double _fraction = 0;
        std::string _stage = "Starting";
        bool _done = false;

        static bool TryPreviewCount(std::string_view line, std::int32_t& done, std::int32_t& total);
        [[nodiscard]] Band Classify(std::string_view line) const;
        bool Set(double fraction, const std::string& stage);
    };
}
