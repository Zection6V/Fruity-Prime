#pragma once

#include "../Formats/Types.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MphRead
{
    class Model;
}

namespace MphRead::Export
{
    class Images final
    {
    public:
        static void Screenshot(
            std::int32_t width,
            std::int32_t height,
            std::optional<std::string> name = std::nullopt);
        static void Record(std::int32_t width, std::int32_t height, const std::string& name);
        static void StopRecording();
        static void ExportImages(const Model& model);
        static void ExportPalettes(const Model& model);
        static void SaveTexture(
            const std::string& directory,
            const std::string& filename,
            std::uint16_t width,
            std::uint16_t height,
            const std::vector<ColorRgba>& pixels);
        static void ExportHudLayers();
        static void ExportHudObjects();

        Images() = delete;
        Images(const Images&) = delete;
        Images& operator=(const Images&) = delete;
        Images(Images&&) = delete;
        Images& operator=(Images&&) = delete;

    private:
        class TaskState;
        class QueueState;

        static std::atomic<std::shared_ptr<TaskState>> _task;
        static bool _recording;
        static QueueState _queue;

        static void ProcessQueue();
    };
}
