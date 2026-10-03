#include "AndroidHunterShot.hpp"

#if !defined(__ANDROID__)
#error "AndroidHunterShot is only valid for the Android native target."
#endif

#include "AndroidGlContextGate.hpp"
#include "AndroidInput.hpp"
#include "MainActivity.hpp"
#include "OffscreenGl.hpp"

#include "../MphRead.Native/Mods/DebugLog.hpp"
#include "../MphRead.Native/Mods/Render/EsBindings.hpp"
#include "../MphRead.Native/Mods/Render/GlEs.hpp"
#include "../MphRead.Native/NativeRuntime/OpenTK/GL.hpp"
#include "../MphRead.Native/NativeRuntime/System/Console.hpp"
#include "../MphRead.Native/NativeRuntime/System/ExceptionText.hpp"
#include "../MphRead.Native/Renderer.hpp"
#include "../MphRead.Native/Scene.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    namespace GL = ::OpenTK::Graphics::OpenGL::GL;

    std::mutex g_currentGate;
    std::shared_ptr<MphRead::Droid::AndroidHunterShot> g_current;

    [[nodiscard]] std::shared_future<
        std::optional<std::vector<std::uint8_t>>> NullImageTask()
    {
        std::promise<std::optional<std::vector<std::uint8_t>>> promise;
        promise.set_value(std::nullopt);
        return promise.get_future().share();
    }

}

namespace MphRead::Droid
{
    struct AndroidHunterShot::Job final
    {
        Hunter HunterValue = Hunter::Samus;
        std::int32_t Suit = 0;
        std::int32_t Width = 0;
        std::int32_t Height = 0;
        std::promise<std::optional<std::vector<std::uint8_t>>> Done;
    };

    struct AndroidHunterShot::Worker final
    {
        std::mutex Gate;
        std::condition_variable Finished;
        bool IsFinished = false;
    };

    struct AndroidHunterShot::State final
    {
        std::mutex Gate;
        std::condition_variable Work;
        std::shared_ptr<Job> Next;
        std::shared_ptr<Job> MatchPicture;
        std::shared_ptr<Worker> CurrentWorker;
        std::size_t WorkPermits = 0;
        bool Retire = false;
        bool Failed = false;
    };

    AndroidHunterShot::AndroidHunterShot()
        : _state(std::make_shared<State>())
    {
    }

    AndroidHunterShot::~AndroidHunterShot()
    {
        // Detached workers own State rather than this object. Retire asks them
        // to release their GL resources and bounds the wait just like the C#
        // Thread.Join(TimeSpan.FromSeconds(4)).
        Retire();
    }

    std::shared_ptr<AndroidHunterShot> AndroidHunterShot::Install()
    {
        auto instance = std::shared_ptr<AndroidHunterShot>(
            new AndroidHunterShot());
        {
            std::lock_guard lock(g_currentGate);
            g_current = instance;
        }
        Mods::Render::HunterShot::Current = instance;
        return instance;
    }

    std::shared_ptr<AndroidHunterShot> AndroidHunterShot::Current()
    {
        std::lock_guard lock(g_currentGate);
        return g_current;
    }

    void AndroidHunterShot::RetireCurrent()
    {
        std::shared_ptr<AndroidHunterShot> current = Current();
        if (current != nullptr)
        {
            current->Retire();
        }
    }

    void AndroidHunterShot::ResumeCurrent()
    {
        std::shared_ptr<AndroidHunterShot> current = Current();
        if (current == nullptr)
        {
            return;
        }
        std::lock_guard lock(current->_state->Gate);
        if (!current->_state->Failed)
        {
            current->_state->Retire = false;
        }
    }

    std::shared_future<std::optional<std::vector<std::uint8_t>>>
        AndroidHunterShot::RenderAsync(
            Hunter hunter,
            std::int32_t suit,
            std::int32_t width,
            std::int32_t height)
    {
        {
            std::lock_guard lock(_state->Gate);
            if (_state->Failed)
            {
                return NullImageTask();
            }
        }
        if (width <= 0 || height <= 0)
        {
            return NullImageTask();
        }

        auto job = std::make_shared<Job>();
        job->HunterValue = hunter;
        job->Suit = std::clamp(suit, 0, 3);
        job->Width = width;
        job->Height = height;
        std::shared_future<std::optional<std::vector<std::uint8_t>>> result =
            job->Done.get_future().share();

        if (InMatch())
        {
            std::lock_guard lock(_state->Gate);
            if (_state->MatchPicture) _state->MatchPicture->Done.set_value(std::nullopt);
            _state->MatchPicture = std::move(job);
            return result;
        }

        std::shared_ptr<Job> dropped;
        {
            std::lock_guard lock(_state->Gate);
            // Retire is a handoff barrier. Do not revive or enqueue work for
            // this worker until its old GL context has completed teardown.
            if (_state->Failed || _state->Retire)
            {
                return NullImageTask();
            }
            // Only the newest is worth rendering: the picker is turned faster
            // than a render takes, and intermediate hunters are already stale.
            dropped = std::move(_state->Next);
            _state->Next = job;
            if (_state->CurrentWorker == nullptr)
            {
                auto worker = std::make_shared<Worker>();
                _state->CurrentWorker = worker;
                try
                {
                    std::thread([state = _state, worker]
                    {
                        Loop(state, worker);
                    }).detach();
                }
                catch (...)
                {
                    _state->CurrentWorker.reset();
                    _state->Next.reset();
                    throw;
                }
            }
        }
        if (dropped != nullptr)
        {
            dropped->Done.set_value(std::nullopt);
        }
        {
            std::lock_guard lock(_state->Gate);
            ++_state->WorkPermits;
        }
        _state->Work.notify_one();
        return result;
    }

    void AndroidHunterShot::Retire()
    {
        std::shared_ptr<Worker> worker;
        {
            std::lock_guard lock(_state->Gate);
            _state->Retire = true;
            if (_state->MatchPicture)
            {
                _state->MatchPicture->Done.set_value(std::nullopt);
                _state->MatchPicture.reset();
            }
            if (_state->Next != nullptr)
            {
                _state->Next->Done.set_value(std::nullopt);
                _state->Next.reset();
            }
            worker = _state->CurrentWorker;
            if (worker != nullptr)
            {
                ++_state->WorkPermits;
            }
        }
        if (worker == nullptr)
        {
            return;
        }
        _state->Work.notify_one();

        std::unique_lock lock(worker->Gate);
        const bool finished = worker->Finished.wait_for(
            lock,
            std::chrono::seconds(4),
            [&worker] { return worker->IsFinished; });
        if (!finished)
        {
            // This timeout is only a UI responsiveness bound. Retire remains
            // asserted, so no replacement hunter worker can start. The global
            // AndroidGlContextLease is the actual cross-context ownership
            // barrier until this worker completes teardown.
            ::MphRead::NativeRuntime::ConsoleWriteLine(
                "[hunter] preview retirement is still finishing GL teardown");
        }
    }

    void AndroidHunterShot::Loop(
        const std::shared_ptr<State>& state,
        const std::shared_ptr<Worker>& worker)
    {
        (void)pthread_setname_np(pthread_self(), "hunter preview");
        AndroidGlContextLease glContextLease;
        std::shared_ptr<OffscreenGl> gl;
        std::shared_ptr<Scene> scene;
        std::unique_ptr<AndroidInput> input;
        std::int32_t width = 0;
        std::int32_t height = 0;

        const auto finishWorker = [&worker]
        {
            {
                std::lock_guard lock(worker->Gate);
                worker->IsFinished = true;
            }
            worker->Finished.notify_all();
        };

        const auto rememberCleanupError = [](
            std::exception_ptr& first,
            std::exception_ptr current) noexcept
        {
            if (first == nullptr)
            {
                first = std::move(current);
            }
        };
        const auto cleanupSceneWhileCurrent = [
            &scene,
            &input,
            &rememberCleanupError]() noexcept
            -> std::exception_ptr
        {
            std::exception_ptr first;
            if (scene != nullptr)
            {
                try
                {
                    scene->DoCleanup();
                }
                catch (...)
                {
                    rememberCleanupError(first, std::current_exception());
                }
                try
                {
                    // Phase 4 GPU mesh/transient buffers belong to Scene and
                    // must be deleted while this EGL context is still current.
                    scene->ReleaseGpuResources();
                }
                catch (...)
                {
                    rememberCleanupError(first, std::current_exception());
                }

                // Even when ReleaseGpuResources failed, destroy the Scene before the GLES
                // shim/context. GPU resource destructors therefore still run
                // while this worker owns the current EGL context.
                scene.reset();
            }
            input.reset();
            return first;
        };
        const auto cleanupContextWhileOwned = [
            &gl,
            &cleanupSceneWhileCurrent,
            &rememberCleanupError]() noexcept
            -> std::exception_ptr
        {
            std::exception_ptr first = cleanupSceneWhileCurrent();
            if (gl != nullptr)
            {
                try
                {
                    Mods::Render::GlEs::ReleaseContext();
                }
                catch (...)
                {
                    rememberCleanupError(first, std::current_exception());
                }
                try
                {
                    gl->Dispose();
                }
                catch (...)
                {
                    rememberCleanupError(first, std::current_exception());
                }
                gl.reset();
            }
            return first;
        };

        try
        {
            while (true)
            {
                std::shared_ptr<Job> job;
                {
                    std::unique_lock lock(state->Gate);
                    state->Work.wait(lock, [&state]
                    {
                        return state->Retire || state->WorkPermits != 0;
                    });
                    if (state->Retire)
                    {
                        break;
                    }
                    --state->WorkPermits;
                    job = std::move(state->Next);
                }

                if (job == nullptr)
                {
                    continue;
                }
                if (InMatch())
                {
                    job->Done.set_value(std::nullopt);
                    continue;
                }
                if (gl == nullptr)
                {
                    gl = OffscreenGl::Create(job->Width, job->Height);
                    Mods::Render::EsBindings::Load();
                    Mods::Render::GlEs::Reset();
                }
                if (scene == nullptr || width != job->Width || height != job->Height)
                {
                    if (width != 0)
                    {
                        const std::exception_ptr cleanupError
                            = cleanupContextWhileOwned();
                        if (cleanupError != nullptr)
                        {
                            std::rethrow_exception(cleanupError);
                        }
                        gl = OffscreenGl::Create(job->Width, job->Height);
                        Mods::Render::EsBindings::Load();
                        Mods::Render::GlEs::Reset();
                    }
                    width = job->Width;
                    height = job->Height;
                    input = std::make_unique<AndroidInput>();
                    scene = std::make_shared<Scene>(
                        ::OpenTK::Mathematics::Vector2i(width, height),
                        input->Keyboard(),
                        input->Mouse(),
                        [](std::string) {},
                        [] {});
                    scene->SideScene(true);
                    scene->OnLoad();
                    GL::Viewport(0, 0, width, height);
                    scene->OnResize();
                }
                job->Done.set_value(Draw(*scene, *job, width, height));
            }

            // Release Scene GPU resources, then the GLES shim and EGL context,
            // while this worker still owns the process-global Android GL lease.
            if (const std::exception_ptr cleanupError
                    = cleanupContextWhileOwned();
                cleanupError != nullptr)
            {
                ::MphRead::NativeRuntime::ConsoleWriteLine(
                    "[hunter] cleanup failed: "
                        + ::MphRead::NativeRuntime::ExceptionMessage(
                            cleanupError));
            }
            {
                std::lock_guard lock(state->Gate);
                if (state->CurrentWorker == worker)
                {
                    state->CurrentWorker.reset();
                }
                // Retire remains set until ResumeCurrent().
                if (state->Next != nullptr)
                {
                    state->Next->Done.set_value(std::nullopt);
                    state->Next.reset();
                }
            }
            // IsFinished is an ownership handoff signal: do not publish it
            // until the global GLES lease has actually been relinquished.
            glContextLease.Release();
            finishWorker();
        }
        catch (...)
        {
            const std::exception_ptr error = std::current_exception();
            {
                std::lock_guard lock(state->Gate);
                state->Failed = true;
            }
            ::MphRead::NativeRuntime::ConsoleWriteLine(
                "[hunter] the preview thread stopped: "
                    + ::MphRead::NativeRuntime::ExceptionToString(error));
            Mods::DebugLog::Line(
                "ui",
                "the hunter preview is off: "
                    + ::MphRead::NativeRuntime::ExceptionMessage(error));
            // Suppress secondary cleanup errors on the failure path, but keep
            // the same ownership order as terminal retirement.
            (void)cleanupContextWhileOwned();
            {
                std::lock_guard lock(state->Gate);
                if (state->CurrentWorker == worker)
                {
                    state->CurrentWorker.reset();
                }
                // Retire remains set until ResumeCurrent().
                if (state->Next != nullptr)
                {
                    state->Next->Done.set_value(std::nullopt);
                    state->Next.reset();
                }
            }
            glContextLease.Release();
            finishWorker();
        }
    }

    std::optional<std::vector<std::uint8_t>> AndroidHunterShot::Draw(
        Scene& scene,
        const Job& job,
        std::int32_t width,
        std::int32_t height)
    {
        Scene::LauncherPreview = true;
        Scene::LauncherHunter = job.HunterValue;
        Scene::LauncherSuit = job.Suit;
        Scene::PreviewWanted(true);
        Scene::PreviewLeft(0.0F);
        Scene::PreviewTop(0.0F);
        Scene::PreviewRight(1.0F);
        Scene::PreviewBottom(1.0F);

        bool drawn = false;
        for (std::int32_t i = 0; i < 3 && !drawn; ++i)
        {
            Scene::LauncherPreview = true;
            drawn = scene.ModDrawPreviewAlone(
                ::OpenTK::Mathematics::Vector2i(width, height));
        }
        Scene::LauncherPreview = false;
        Scene::PreviewWanted(false);
        if (!drawn)
        {
            return std::nullopt;
        }

        const std::size_t pixelWidth = static_cast<std::size_t>(width);
        const std::size_t pixelHeight = static_cast<std::size_t>(height);
        if (pixelWidth > std::numeric_limits<std::size_t>::max() / pixelHeight)
        {
            throw std::length_error("hunter preview dimensions are too large");
        }
        const std::size_t pixelCount = pixelWidth * pixelHeight;
        if (pixelCount > static_cast<std::size_t>(
                std::numeric_limits<std::int32_t>::max()) / 4U
            || pixelCount > static_cast<std::size_t>(
                std::numeric_limits<std::int32_t>::max()) / 3U)
        {
            throw std::length_error("hunter preview dimensions are too large");
        }
        std::vector<std::uint8_t> rgb(pixelCount * 3U);
        GL::BindFramebuffer(GL::FramebufferTarget::ReadFramebuffer, 0);
        GL::PixelStore(GL::PixelStoreParameter::PackAlignment, 1);
        GL::ReadPixels(
            0,
            0,
            width,
            height,
            GL::PixelFormat::Rgb,
            GL::PixelType::UnsignedByte,
            rgb.data());

        std::vector<std::uint8_t> bgra(pixelCount * 4U);
        for (std::int32_t y = 0; y < height; ++y)
        {
            // GL counts rows from the bottom, while a bitmap counts from top.
            std::size_t from = static_cast<std::size_t>(height - 1 - y)
                * static_cast<std::size_t>(width) * 3U;
            std::size_t to = static_cast<std::size_t>(y)
                * static_cast<std::size_t>(width) * 4U;
            for (std::int32_t x = 0; x < width; ++x)
            {
                bgra[to] = rgb[from + 2U];
                bgra[to + 1U] = rgb[from + 1U];
                bgra[to + 2U] = rgb[from];
                bgra[to + 3U] = 255;
                from += 3U;
                to += 4U;
            }
        }
        return bgra;
    }

    void AndroidHunterShot::RenderMatchPicture(Scene& scene)
    {
        const auto current = Current();
        if (!current) return;
        std::shared_ptr<Job> job;
        {
            std::lock_guard lock(current->_state->Gate);
            job = std::move(current->_state->MatchPicture);
        }
        if (!job) return;
        const auto previousHunter = Scene::LauncherHunter;
        const auto previousSuit = Scene::LauncherSuit;
        try
        {
            Scene::LauncherHunter = job->HunterValue;
            Scene::LauncherSuit = job->Suit;
            const auto rgb = scene.ModPreviewPixels(job->Width, job->Height);
            std::optional<std::vector<std::uint8_t>> pixels;
            if (rgb)
            {
                pixels.emplace(static_cast<std::size_t>(job->Width) * job->Height * 4);
                for (int y = 0; y < job->Height; ++y)
                    for (int x = 0; x < job->Width; ++x)
                    {
                        const auto from = (static_cast<std::size_t>(job->Height - 1 - y) * job->Width + x) * 3;
                        const auto to = (static_cast<std::size_t>(y) * job->Width + x) * 4;
                        (*pixels)[to] = (*rgb)[from + 2]; (*pixels)[to + 1] = (*rgb)[from + 1];
                        (*pixels)[to + 2] = (*rgb)[from]; (*pixels)[to + 3] = 255;
                    }
            }
            job->Done.set_value(std::move(pixels));
        }
        catch (...) { job->Done.set_exception(std::current_exception()); }
        Scene::LauncherHunter = previousHunter;
        Scene::LauncherSuit = previousSuit;
    }

    bool AndroidHunterShot::InMatch() noexcept
    {
        MainActivity* activity = MainActivity::Instance();
        return activity != nullptr && activity->InMatch();
    }
}
