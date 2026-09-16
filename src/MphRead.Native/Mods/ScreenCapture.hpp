#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace MphRead
{
    class Scene;

    namespace Mods
    {
        class ScreenCapture final
        {
        public:
            ScreenCapture() = delete;
            ScreenCapture(const ScreenCapture&) = delete;
            ScreenCapture& operator=(const ScreenCapture&) = delete;
            ScreenCapture(ScreenCapture&&) = delete;
            ScreenCapture& operator=(ScreenCapture&&) = delete;

            [[nodiscard]] static std::function<void(
                std::vector<std::uint8_t>&,
                std::int32_t,
                std::int32_t,
                const std::string&)> PngWriter();
            static void PngWriter(std::function<void(
                std::vector<std::uint8_t>&,
                std::int32_t,
                std::int32_t,
                const std::string&)> value);

            [[nodiscard]] static bool SaveWindow(Scene* scene, const std::string& path);
            [[nodiscard]] static bool Save(Scene* scene, const std::string& path);

            static void EnableDebugOutput(std::function<void(const std::string&)> report);
            [[nodiscard]] static std::string DescribeContext();
            [[nodiscard]] static double NonBlackFraction(Scene* scene);

        private:
            using PngWriterAction = std::function<void(
                std::vector<std::uint8_t>&,
                std::int32_t,
                std::int32_t,
                const std::string&)>;
            using ReportAction = std::function<void(const std::string&)>;
            using PixelBuffer = std::shared_ptr<std::vector<std::uint8_t>>;
            using ReadPixels = std::function<PixelBuffer(std::int32_t&, std::int32_t&)>;
            using DebugProc = std::function<void(
                std::uint32_t,
                std::uint32_t,
                std::uint32_t,
                std::uint32_t,
                std::int32_t,
                const char*,
                const void*)>;

            static constexpr double MinLitFraction = 0.01;

            [[nodiscard]] static bool Save(
                Scene* scene,
                const std::string& path,
                const ReadPixels& read);
            [[nodiscard]] static double LitFraction(const std::vector<std::uint8_t>& pixels);
            static void DebugThunk(
                std::uint32_t source,
                std::uint32_t type,
                std::uint32_t id,
                std::uint32_t severity,
                std::int32_t length,
                const char* message,
                const void* param);

            static PngWriterAction _pngWriter;
            static DebugProc _debugCallback;
            static std::int32_t _messagesLogged;
        };
    }
}
