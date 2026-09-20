#include "PreviewRun.hpp"

#include "AndroidInput.hpp"

#include "../MphRead.Native/Renderer.hpp"
#include "../MphRead.Native/Scene.hpp"
#include "../MphRead.Native/GameState.hpp"
#include "../MphRead.Native/Formats/Formats.hpp"
#include "../MphRead.Native/Formats/Types.hpp"
#include "../MphRead.Native/Mods/Render/EsBindings.hpp"
#include "../MphRead.Native/Mods/Render/GlEs.hpp"
#include "../MphRead.Native/Mods/ScreenCapture.hpp"
#include "../MphRead.Native/Mods/ThumbnailGenerator.hpp"
#include "../MphRead.Native/Mods/ThumbnailMode.hpp"
#include "../MphRead.Native/Sound/Music.hpp"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace
{
    void InvokeReport(
        const std::function<void(const std::string&)>& report,
        const std::string& message
    )
    {
        if (!report)
        {
            throw System::NullReferenceException();
        }
        report(message);
    }

    std::string FormatElapsedSeconds(double seconds)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(1) << seconds;
        return stream.str();
    }
}

namespace MphRead::Droid
{
    std::int32_t PreviewRun::Render(
        const std::vector<std::string>& rooms,
        std::int32_t width,
        std::int32_t height,
        const std::function<void(const std::string&)>& report,
        const std::function<bool()>& cancelled
    )
    {
        Mods::Render::EsBindings::Load();
        Mods::Render::GlEs::Reset();
        Mods::ThumbnailMode::Enter();

        AndroidInput input;
        std::int32_t written = 0;

        try
        {
            for (std::int32_t i = 0;
                 i < static_cast<std::int32_t>(rooms.size());
                 ++i)
            {
                if (cancelled && cancelled())
                {
                    break;
                }

                const std::string room =
                    rooms[static_cast<std::size_t>(i)];
                const auto clock = std::chrono::steady_clock::now();

                const bool saved =
                    RenderOne(room, input, width, height, report);
                if (saved)
                {
                    ++written;
                }

                std::string line =
                    "[thumbnails] " + std::to_string(i + 1)
                    + "/" + std::to_string(rooms.size())
                    + "  " + room;
                if (saved)
                {
                    const double seconds =
                        std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - clock
                        ).count();
                    line += "  " + FormatElapsedSeconds(seconds) + "s";
                }
                else
                {
                    line += "  -- nothing usable";
                }
                InvokeReport(report, line);
            }
        }
        catch (...)
        {
            Mods::ThumbnailMode::Exit();
            throw;
        }

        Mods::ThumbnailMode::Exit();
        return written;
    }

    bool PreviewRun::RenderOne(
        const std::string& room,
        AndroidInput& input,
        std::int32_t width,
        std::int32_t height,
        const std::function<void(const std::string&)>& report
    )
    {
        std::unique_ptr<Scene> scene;
        bool result = false;
        std::exception_ptr pending;

        try
        {
            try
            {
                scene = std::make_unique<Scene>(
                    OpenTK::Mathematics::Vector2i(width, height),
                    input.Keyboard(),
                    input.Mouse(),
                    [](std::string)
                    {
                    },
                    []()
                    {
                    }
                );

                scene->AddPlayer(
                    Hunter::Samus,
                    0,
                    -1
                );
                scene->AddRoom(
                    room,
                    GameMode::Battle,
                    1
                );
                scene->OnLoad();

                Silence(report);

                Mods::Render::GlEs::Viewport(
                    0,
                    0,
                    width,
                    height
                );
                scene->OnResize();

                for (std::int32_t frame = 0;
                     frame < SettleFrames;
                     ++frame)
                {
                    GameState::ApplyPause();
                    scene->OnUpdateFrame();
                }

                GameState::ApplyPause();
                scene->OnUpdateFrame();

                if (scene->OnRenderFrame())
                {
                    scene->AfterRenderFrame();
                    result = Mods::ScreenCapture::Save(
                        scene.get(),
                        Mods::ThumbnailGenerator::PathFor(room)
                    );
                }
            }
            catch (const std::exception& ex)
            {
                std::cout
                    << "[thumbnails] "
                    << room
                    << " failed: "
                    << ex.what()
                    << '\n';

                InvokeReport(
                    report,
                    "[thumbnails] " + room + ": " + ex.what()
                );
                result = false;
            }
        }
        catch (...)
        {
            pending = std::current_exception();
        }

        try
        {
            if (scene)
            {
                scene->DoCleanup();
            }
        }
        catch (const std::exception& ex)
        {
            InvokeReport(
                report,
                "[thumbnails] " + room
                + ": cleanup failed: " + ex.what()
            );
        }

        if (pending)
        {
            std::rethrow_exception(pending);
        }
        return result;
    }

    void PreviewRun::Silence(
        const std::function<void(const std::string&)>& report
    )
    {
        try
        {
            Music::Stop();
            MusicPlayer::Stop();
        }
        catch (const std::exception& ex)
        {
            InvokeReport(
                report,
                "[thumbnails] could not stop the music: "
                + std::string(ex.what())
            );
        }
    }
}
