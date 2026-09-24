// The two small platform services the launcher asks for outside a window: the
// application icon Avalonia loads from its own assets, and the thumbnail host
// that renders the map previews.

#include "HostDrawing.hpp"
#include "HostTask.hpp"

#include "../Stb/Image.hpp"

#include "../../Mods/Launcher/Gui/GuiTheme.hpp"
#include "../../Mods/ThumbnailBatch.hpp"
#include "../../Mods/ThumbnailGenerator.hpp"
#include "../../Mods/ThumbnailHost.hpp"

#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace MphRead::NativeRuntime::Avalonia
{
    namespace
    {
        // The icon's own pixels, which is all a window ever needs of it.
        struct IconPixels final
        {
            std::int32_t Width = 0;
            std::int32_t Height = 0;
            std::vector<std::uint8_t> Rgba;
        };

        // The lists and reports the thumbnail host passes about as opaque
        // references. They are owned here for the length of the process, as
        // the managed objects they stand for are owned by the collector.
        struct ThumbnailRooms final
        {
            std::vector<std::string> Rooms;
        };

        struct ThumbnailReport final
        {
            std::function<void(const std::string&)> Write;
        };

        struct ThumbnailTask final
        {
            std::shared_ptr<HostTask> Task;
            std::int32_t Result = 0;
        };

        template <typename T>
        [[nodiscard]] T* Own(std::shared_ptr<T> value)
        {
            static std::deque<std::shared_ptr<T>> kept;
            kept.push_back(std::move(value));
            return kept.back().get();
        }

        class ThumbnailHost final : public MphRead::Mods::ThumbnailHostAdapter
        {
        public:
            MphRead::Mods::ThumbnailRoomsRef
                ThumbnailGeneratorMissingThumbnails() override
            {
                auto rooms = std::make_shared<ThumbnailRooms>();
                rooms->Rooms
                    = MphRead::Mods::ThumbnailGenerator::MissingThumbnails();
                return MphRead::Mods::ThumbnailRoomsRef{Own(std::move(rooms))};
            }

            [[nodiscard]] std::size_t ThumbnailRoomsCount(
                MphRead::Mods::ThumbnailRoomsRef rooms) const override
            {
                const auto* const value
                    = static_cast<const ThumbnailRooms*>(rooms.Native);
                return value == nullptr ? 0 : value->Rooms.size();
            }

            [[nodiscard]] bool ThumbnailBatchCanRun() const override
            {
                return MphRead::Mods::ThumbnailBatch::CanRun();
            }

            [[nodiscard]] MphRead::Mods::ThumbnailTaskIntRef CompletedTask(
                int result) noexcept override
            {
                auto task = std::make_shared<ThumbnailTask>();
                task->Task = HostTask::Completed();
                task->Result = result;
                return MphRead::Mods::ThumbnailTaskIntRef{Own(std::move(task))};
            }

            [[nodiscard]] MphRead::Mods::ThumbnailTaskIntRef FaultedTask(
                std::exception_ptr error) noexcept override
            {
                auto task = std::make_shared<ThumbnailTask>();
                task->Task = HostTask::Faulted(std::move(error));
                return MphRead::Mods::ThumbnailTaskIntRef{Own(std::move(task))};
            }

            [[nodiscard]] MphRead::Mods::ThumbnailTaskIntRef AwaitTask(
                MphRead::Mods::ThumbnailTaskIntRef task) override
            {
                // `return await task` with nothing after it is the same task.
                return task;
            }

            [[nodiscard]] MphRead::Mods::ThumbnailTaskIntRef RunThumbnailBatchAsync(
                MphRead::Mods::ThumbnailRoomsRef rooms,
                MphRead::Mods::ThumbnailReportRef report) override
            {
                auto task = std::make_shared<ThumbnailTask>();
                ThumbnailTask* const raw = Own(task);
                const auto* const names
                    = static_cast<const ThumbnailRooms*>(rooms.Native);
                const auto* const writer
                    = static_cast<const ThumbnailReport*>(report.Native);
                const std::vector<std::string> list
                    = names != nullptr ? names->Rooms : std::vector<std::string>{};
                const std::function<void(const std::string&)> write
                    = writer != nullptr ? writer->Write
                                        : std::function<void(const std::string&)>{};
                raw->Task = HostTask::Run(
                    [raw, list, write]()
                    {
                        raw->Result = MphRead::Mods::ThumbnailBatch::Run(list,
                            MphRead::Mods::ThumbnailBatch::DefaultParallelism(),
                            MphRead::Mods::ThumbnailGenerator::ThumbnailWidth,
                            MphRead::Mods::ThumbnailGenerator::ThumbnailHeight,
                            write);
                    });
                return MphRead::Mods::ThumbnailTaskIntRef{raw};
            }
        };
    }
}

namespace MphRead::Mods
{
    ThumbnailHostAdapter& GetThumbnailHostAdapter() noexcept
    {
        static ::MphRead::NativeRuntime::Avalonia::ThumbnailHost adapter;
        return adapter;
    }
}

namespace MphRead::Mods::Launcher::Gui::Detail
{
    std::optional<GuiWindowIcon> GuiThemeLoadWindowIcon(std::string_view uri)
    {
        // An avares: URI names a file the published build carries beside the
        // executable, which is where the native build keeps it.
        std::string name(uri);
        const std::size_t slash = name.rfind('/');
        if (slash != std::string::npos)
        {
            name = name.substr(slash + 1);
        }
        std::error_code error;
        const std::filesystem::path directory
            = std::filesystem::current_path(error);
        if (error)
        {
            return std::nullopt;
        }
        std::ifstream file(directory / name, std::ios::binary);
        if (!file)
        {
            return std::nullopt;
        }
        const std::istreambuf_iterator<char> first(file);
        const std::istreambuf_iterator<char> last;
        const std::vector<std::uint8_t> bytes(first, last);
        const ::MphRead::NativeRuntime::Image image
            = ::MphRead::NativeRuntime::LoadPng(bytes, 4);
        if (image.Width <= 0 || image.Height <= 0)
        {
            return std::nullopt;
        }
        auto pixels
            = std::make_shared<::MphRead::NativeRuntime::Avalonia::IconPixels>();
        pixels->Width = image.Width;
        pixels->Height = image.Height;
        pixels->Rgba = image.Pixels;
        return GuiWindowIcon{std::move(pixels)};
    }
}
