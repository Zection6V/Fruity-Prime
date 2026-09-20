#pragma once

#if !defined(__ANDROID__)
#error "PreviewRun is only valid for the Android native target."
#endif

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace MphRead::Droid
{
    class AndroidInput;

    class PreviewRun final
    {
    public:
        static constexpr std::int32_t Width = 640;
        static constexpr std::int32_t Height = 360;

        [[nodiscard]] static std::int32_t Render(
            const std::vector<std::string>& rooms,
            std::int32_t width,
            std::int32_t height,
            const std::function<void(const std::string&)>& report,
            const std::function<bool()>& cancelled = {}
        );

    private:
        static constexpr std::int32_t SettleFrames = 12;

        [[nodiscard]] static bool RenderOne(
            const std::string& room,
            AndroidInput& input,
            std::int32_t width,
            std::int32_t height,
            const std::function<void(const std::string&)>& report
        );

        static void Silence(
            const std::function<void(const std::string&)>& report
        );

        PreviewRun() = delete;
        ~PreviewRun() = delete;
        PreviewRun(const PreviewRun&) = delete;
        PreviewRun& operator=(const PreviewRun&) = delete;
        PreviewRun(PreviewRun&&) = delete;
        PreviewRun& operator=(PreviewRun&&) = delete;
    };
}
