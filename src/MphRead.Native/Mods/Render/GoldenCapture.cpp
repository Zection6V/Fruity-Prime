#include "GoldenCapture.hpp"
#include "GoldenCaptureValidation.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../GameState.hpp"
#include "../../NativeRuntime/System/Buffers.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/ExceptionText.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"
#include "../../NativeRuntime/Rhi/BackendFactory.hpp"
#include "../../Scene.hpp"
#include "../Branding.hpp"
#include "../MapGen/CustomRooms.hpp"
#include "../RenderOptions.hpp"
#include "../ScreenCapture.hpp"
#include "DesktopGlContext.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    using ::OpenTK::Mathematics::Vector2i;
    using ::OpenTK::Mathematics::Vector3;
    using ::OpenTK::Mathematics::Vector4;

    constexpr std::int32_t GoldenWidth = 1600;
    constexpr std::int32_t GoldenHeight = 900;
    constexpr std::int32_t GoldenWarmupUpdates = 12;
    constexpr std::int32_t GoldenCaptureUpdate = GoldenWarmupUpdates + 1;
    constexpr float GoldenFovDegrees = 78.0F;
    constexpr std::string_view GoldenRoom = "TEST ARENA";
    constexpr std::string_view GoldenFixtureContract = "phase4-final-stage-v2";

    [[nodiscard]] Vector3 GoldenCameraPosition()
    {
        return Vector3(0.0F, 16.0F, 30.0F);
    }

    [[nodiscard]] Vector3 GoldenCameraTarget()
    {
        return Vector3(0.0F, 1.0F, 0.0F);
    }

    enum class GoldenCandidate : std::int32_t
    {
        TransparentObject,
        Decal,
        Particle,
        Trail,
        Hud,
        Fade,
        WhiteoutDisruption
    };

    [[nodiscard]] std::string LowerAscii(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return value;
    }

    [[nodiscard]] std::optional<GoldenCandidate> ParseCandidate(std::string value)
    {
        value = LowerAscii(std::move(value));
        if (value == "transparent" || value == "transparent-object")
        {
            return GoldenCandidate::TransparentObject;
        }
        if (value == "decal")
        {
            return GoldenCandidate::Decal;
        }
        if (value == "particle")
        {
            return GoldenCandidate::Particle;
        }
        if (value == "trail")
        {
            return GoldenCandidate::Trail;
        }
        if (value == "hud")
        {
            return GoldenCandidate::Hud;
        }
        if (value == "fade")
        {
            return GoldenCandidate::Fade;
        }
        if (value == "whiteout-disruption"
            || value == "disruption"
            || value == "whiteout")
        {
            return GoldenCandidate::WhiteoutDisruption;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::string_view CandidateName(GoldenCandidate candidate)
    {
        switch (candidate)
        {
        case GoldenCandidate::TransparentObject:
            return "transparent-object";
        case GoldenCandidate::Decal:
            return "decal";
        case GoldenCandidate::Particle:
            return "particle";
        case GoldenCandidate::Trail:
            return "trail";
        case GoldenCandidate::Hud:
            return "hud";
        case GoldenCandidate::Fade:
            return "fade";
        case GoldenCandidate::WhiteoutDisruption:
            return "whiteout-disruption";
        }
        return "unknown";
    }

    [[nodiscard]] bool UsesPlayerCamera(GoldenCandidate candidate) noexcept
    {
        return candidate == GoldenCandidate::Hud
            || candidate == GoldenCandidate::Fade
            || candidate == GoldenCandidate::WhiteoutDisruption;
    }

    [[nodiscard]] bool UsesSyntheticFixture(GoldenCandidate candidate) noexcept
    {
        return candidate == GoldenCandidate::TransparentObject
            || candidate == GoldenCandidate::Decal
            || candidate == GoldenCandidate::Particle
            || candidate == GoldenCandidate::Trail;
    }

    [[nodiscard]] MphRead::Mods::Render::GoldenFixture FixtureFor(
        GoldenCandidate candidate)
    {
        using MphRead::Mods::Render::GoldenFixture;
        switch (candidate)
        {
        case GoldenCandidate::TransparentObject:
            return GoldenFixture::TransparentObject;
        case GoldenCandidate::Decal:
            return GoldenFixture::Decal;
        case GoldenCandidate::Particle:
            return GoldenFixture::Particle;
        case GoldenCandidate::Trail:
            return GoldenFixture::Trail;
        default:
            throw std::invalid_argument(
                "The requested golden candidate has no geometry fixture.");
        }
    }

    void ApplyGoldenRenderOptions()
    {
        using ::MphRead::Mods::RenderOptions;
        RenderOptions::ResolutionScale(100);
        RenderOptions::FieldOfView(78);
        RenderOptions::Lighting(true);
        RenderOptions::CelShading(false);
        RenderOptions::ShowFps(false);
        RenderOptions::CelBands(8);
        RenderOptions::CelEdge(0.5F);
        RenderOptions::Fog(false);
        RenderOptions::TextureFiltering(false);
    }

#if defined(MPHREAD_SHELL)
    class GoldenCaptureWindow final
        : public MphRead::RendererPlatform::WindowEvents
    {
    public:
        GoldenCaptureWindow(
            GoldenCandidate candidate,
            std::string directory)
            : _candidate(candidate),
              _directory(std::move(directory))
        {
            MphRead::RendererPlatform::WindowSettings settings
                = MphRead::Mods::Render::DesktopGlContext::Settings(
                    false, MphRead::RendererPlatform::GraphicsWindowMode::OpenGL);
            settings.ClientSize = Vector2i(GoldenWidth, GoldenHeight);
            settings.Title = std::string(MphRead::Mods::Branding::Name)
                + " Phase 0 golden capture";
            settings.StartVisible = true;
            settings.UpdateFrequency = 0.0;

            _window = MphRead::RendererPlatform::CreateWindow(settings);
            MphRead::NativeRuntime::Rhi::SwapchainDesc swapchainDesc{};
            const Vector2i framebufferSize = _window->Size();
            swapchainDesc.width = static_cast<std::uint32_t>(std::max(framebufferSize.X, 1));
            swapchainDesc.height = static_cast<std::uint32_t>(std::max(framebufferSize.Y, 1));
            _swapchain = MphRead::NativeRuntime::Rhi::BackendFactory::CreateSwapchain(
                MphRead::NativeRuntime::Rhi::GraphicsBackend::OpenGl, *_window, swapchainDesc);
            _scene = std::make_shared<MphRead::Scene>(
                _window->Size(),
                _window->Keyboard(),
                _window->Mouse(),
                [](std::string) {},
                [this]() { Close(); });
            _scene->AddPlayer(MphRead::Hunter::Samus, 0, -1);
            _scene->AddRoom(
                std::string(GoldenRoom),
                MphRead::GameMode::Battle,
                1);

            const std::string base(CandidateName(_candidate));
            _imagePath = MphRead::NativeRuntime::PathCombine(
                _directory, base + ".png");
            _manifestPath = MphRead::NativeRuntime::PathCombine(
                _directory, base + ".txt");
        }

        [[nodiscard]] bool Succeeded() const noexcept
        {
            return _captured;
        }

        [[nodiscard]] std::optional<
            MphRead::Mods::Render::GoldenCaptureValidation::PixelSummary>
            CaptureSummary() const noexcept
        {
            return _captureSummary;
        }

        void Run()
        {
            try
            {
                _window->Run(*this);
            }
            catch (...)
            {
                if (_error.empty())
                {
                    _error = MphRead::NativeRuntime::ExceptionMessage(
                        std::current_exception());
                }
                try
                {
                    WriteManifest();
                }
                catch (...)
                {
                }
                if (!_cleaned)
                {
                    try
                    {
                        _scene->DoCleanup();
                    }
                    catch (...)
                    {
                    }
                    _cleaned = true;
                }
            }
        }

        void OnLoad() override
        {
            _scene->Size(_window->Size());
            _scene->OnLoad();
            _window->BaseOnLoad();
            _scene->OnResize();

            const std::vector<MphRead::ColorRgba> pixels{
                MphRead::ColorRgba(255, 255, 255, 255),
                MphRead::ColorRgba(40, 220, 255, 255),
                MphRead::ColorRgba(40, 220, 255, 255),
                MphRead::ColorRgba(255, 255, 255, 255)
            };
            _fixtureTexture = _scene->BindGetTexture(pixels, 2, 2);
        }

        void OnResize(
            const MphRead::RendererPlatform::ResizeEventArgs& e) override
        {
            if (e.Size.X > 0 && e.Size.Y > 0)
            {
                _scene->Size(e.Size);
                _scene->OnResize();
            }
            _window->BaseOnResize(e);
        }

        void OnRenderFrame(
            const MphRead::RendererPlatform::FrameEventArgs& args) override
        {
            try
            {
                MphRead::GameState::ApplyPause();
                _scene->OnSimulationFrame();
                ++_updateOrdinal;

                if (UsesPlayerCamera(_candidate))
                {
                    _scene->ModGoldenSetPlayerCamera(
                        GoldenCameraPosition(),
                        GoldenCameraTarget(),
                        GoldenFovDegrees);
                }
                else
                {
                    const auto main = MphRead::Entities::PlayerEntity::Main();
                    if (!main || !main->CameraInfo())
                    {
                        Fail("synthetic fixture requires the production player camera");
                        _window->BaseOnRenderFrame(args);
                        return;
                    }
                    const auto camera = main->CameraInfo();
                    _scene->ModGoldenSetPlayerCamera(
                        camera->Position, camera->Target, camera->Fov);
                }

                _scene->OnDrawFrame();

                if (_updateOrdinal <= GoldenWarmupUpdates)
                {
                    _window->BaseOnRenderFrame(args);
                    return;
                }

                if (_updateOrdinal != GoldenCaptureUpdate)
                {
                    Fail("capture update ordinal drifted");
                    _window->BaseOnRenderFrame(args);
                    return;
                }

                const Vector2i framebuffer = _window->Size();
                const Vector2i client = _window->ClientSize();
                if (framebuffer != Vector2i(GoldenWidth, GoldenHeight)
                    || client != Vector2i(GoldenWidth, GoldenHeight)
                    || _scene->Size()
                        != Vector2i(GoldenWidth, GoldenHeight))
                {
                    std::ostringstream reason;
                    reason
                        << "capture dimensions are not exactly "
                        << GoldenWidth << "x" << GoldenHeight
                        << " (framebuffer "
                        << framebuffer.X << "x" << framebuffer.Y
                        << ", client "
                        << client.X << "x" << client.Y
                        << ", scene "
                        << _scene->Size().X << "x" << _scene->Size().Y
                        << ")";
                    Fail(reason.str());
                    _window->BaseOnRenderFrame(args);
                    return;
                }

                std::optional<std::vector<std::uint8_t>> controlPixels;
                if (UsesSyntheticFixture(_candidate))
                {
                    _scene->ModGoldenInjectFixture(
                        FixtureFor(_candidate),
                        _fixtureTexture);
                }
                else
                {
                    _scene->ModGoldenResetFinalStageState();
                    _finalStageGateVerified
                        = _scene->ModGoldenFinalStageReady();
                    if (!_finalStageGateVerified)
                    {
                        Fail(
                            "production final-stage gate is not ready "
                            "(active main player + CameraMode::Player required)");
                        _window->BaseOnRenderFrame(args);
                        return;
                    }

                    if (_candidate == GoldenCandidate::Hud)
                    {
                        _controlDescription
                            = "same simulation update with HUD/fade final-stage gate disabled";
                        _scene->ModGoldenSetHudPassEnabled(false);
                    }
                    else
                    {
                        _controlDescription
                            = "same simulation update with clean production HUD and no fade/disruption";
                    }

                    if (!_scene->OnRenderFrame())
                    {
                        Fail(
                            "Scene::OnRenderFrame refused the fixed "
                            "control render");
                        _window->BaseOnRenderFrame(args);
                        return;
                    }
                    controlPixels = ReadValidatedWindow("control");
                    _controlSummary
                        = MphRead::Mods::Render::GoldenCaptureValidation::SummarizeRgb(
                            *controlPixels, GoldenWidth, GoldenHeight);

                    if (_candidate == GoldenCandidate::Hud)
                    {
                        _scene->ModGoldenSetHudPassEnabled(true);
                    }
                    else if (_candidate == GoldenCandidate::Fade)
                    {
                        _scene->ModGoldenArmFadeObservation();
                        _scene->ModGoldenSetFadeState(1.0F, 0.5F);
                        if (!_scene->ModGoldenFadeStateMatches(1.0F, 0.5F))
                        {
                            Fail("fade fixture state did not latch");
                            _window->BaseOnRenderFrame(args);
                            return;
                        }
                    }
                    else if (_candidate
                        == GoldenCandidate::WhiteoutDisruption)
                    {
                        _scene->ModGoldenSetElapsedTime(0.25F);
                        const auto main
                            = MphRead::Entities::PlayerEntity::Main();
                        if (!main)
                        {
                            Fail("main player is missing");
                            _window->BaseOnRenderFrame(args);
                            return;
                        }
                        main->ModGoldenSetHudShift(
                            2,
                            0.75F,
                            1,
                            1.0F,
                            48.0F);
                        if (!main->ModGoldenHudShiftStateMatches(
                                2,
                                0.75F,
                                1,
                                1.0F,
                                48.0F))
                        {
                            Fail(
                                "whiteout/disruption fixture state "
                                "did not latch");
                            _window->BaseOnRenderFrame(args);
                            return;
                        }
                    }

                    // The control OnRenderFrame finishes on framebuffer 0.
                    // Re-run draw preparation without advancing simulation so
                    // the intended pass gets the production scene framebuffer
                    // and render-item state for the same simulation update.
                    _scene->OnDrawFrame();

                    _finalStageGateVerified
                        = _scene->ModGoldenFinalStageReady();
                    if (!_finalStageGateVerified)
                    {
                        Fail(
                            "production final-stage gate was lost before "
                            "the intended render");
                        _window->BaseOnRenderFrame(args);
                        return;
                    }
                }

                if (!_scene->OnRenderFrame())
                {
                    Fail(
                        "Scene::OnRenderFrame refused the fixed "
                        "capture update");
                    _window->BaseOnRenderFrame(args);
                    return;
                }

                if (_candidate == GoldenCandidate::Fade)
                {
                    _fadeObservation = _scene->ModGoldenFadeObservation();
                    MphRead::Mods::Render::GoldenCaptureValidation::RequireExpectedFadeSequence(
                        *_fadeObservation,
                        static_cast<std::int32_t>(MphRead::FadeType::FadeOutWhite),
                        1.0F,
                        0.5F);
                }

                std::vector<std::uint8_t> capturedPixels
                    = ReadValidatedWindow("candidate");
                _captureSummary
                    = MphRead::Mods::Render::GoldenCaptureValidation::SummarizeRgb(
                        capturedPixels, GoldenWidth, GoldenHeight);
                if (controlPixels.has_value())
                {
                    _changedPixelCount
                        = MphRead::Mods::Render::GoldenCaptureValidation::RequireDistinctRgb(
                            *controlPixels,
                            capturedPixels,
                            GoldenWidth,
                            GoldenHeight);
                }

                _captured = MphRead::Mods::ScreenCapture::SaveWindow(
                    _scene.get(),
                    _imagePath);
                if (!_captured)
                {
                    _error
                        = "ScreenCapture::SaveWindow returned false";
                }

                WriteManifest();
                _swapchain->Present();
                _scene->AfterRenderFrame();
                _window->BaseOnRenderFrame(args);
                Close();
            }
            catch (...)
            {
                _error = MphRead::NativeRuntime::ExceptionMessage(
                    std::current_exception());
                try
                {
                    WriteManifest();
                }
                catch (...)
                {
                }
                Close();
            }
        }

        void OnClosing() override
        {
            if (!_cleaned)
            {
                _scene->DoCleanup();
                _cleaned = true;
            }
            _window->BaseOnClosing();
        }

    private:
        [[nodiscard]] std::vector<std::uint8_t> ReadValidatedWindow(
            std::string_view label)
        {
            std::int32_t width = 0;
            std::int32_t height = 0;
            std::optional<std::vector<std::uint8_t>> pixels
                = _scene->ReadWindowBuffer(width, height);
            if (!pixels.has_value())
            {
                throw std::runtime_error(
                    std::string(label) + " window RGB read returned no pixels");
            }
            if (width != GoldenWidth || height != GoldenHeight)
            {
                throw std::runtime_error(
                    std::string(label)
                    + " window RGB read did not match the fixed dimensions");
            }
            (void)MphRead::Mods::Render::GoldenCaptureValidation::RequireMeaningfulRgb(
                *pixels, width, height);
            return std::move(*pixels);
        }

        void Close()
        {
            _window->Close();
        }

        void Fail(std::string reason)
        {
            _error = std::move(reason);
            try
            {
                WriteManifest();
            }
            catch (...)
            {
            }
            Close();
        }

        [[nodiscard]] std::string FixtureDescription() const
        {
            switch (_candidate)
            {
            case GoldenCandidate::TransparentObject:
                return
                    "camera-relative translucent RenderItemType::Quad; "
                    "material alpha=0.45 and override alpha=0.45; "
                    "centered 2.5 units in front of the production camera";
            case GoldenCandidate::Decal:
                return
                    "camera-relative RenderMode::Decal quad; alpha=1.0; "
                    "centered 2.7 units in front of the production camera";
            case GoldenCandidate::Particle:
                return
                    "camera-relative RenderItemType::Particle using a "
                    "Scene-owned 2x2 cyan/white checker texture; alpha=0.85; "
                    "centered 2.2 units in front of the production camera";
            case GoldenCandidate::Trail:
                return
                    "camera-relative RenderItemType::TrailMulti using a "
                    "Scene-owned 2x2 cyan/white checker texture; "
                    "8 strip vertices centered 2.4 units in front of "
                    "the production camera";
            case GoldenCandidate::Hud:
                return
                    "real Samus production HUD; unrelated fade/disruption "
                    "state is reset after OnDrawFrame; output is required "
                    "to differ from a same-update HUD-disabled control";
            case GoldenCandidate::Fade:
                return
                    "real production FadeOutWhite pass color=1.0,percent=0.5 "
                    "over a clean production HUD baseline; output is required "
                    "to differ from that same-update control";
            case GoldenCandidate::WhiteoutDisruption:
                return
                    "real production shift/whiteout post-process; unrelated "
                    "fade state is reset; disruption state=2,factor=0.75,"
                    "whiteout state=1,factor=1.0,amount=48,"
                    "elapsedTime=0.25s; output must differ from clean HUD";
            }
            return "unknown";
        }

        void WriteManifest()
        {
            if (_manifestWritten)
            {
                return;
            }

            const Vector2i framebuffer
                = _window ? _window->Size() : Vector2i{};
            const Vector2i client
                = _window ? _window->ClientSize() : Vector2i{};
            const Vector2i sceneSize
                = _scene ? _scene->Size() : Vector2i{};

            std::ostringstream out;
            out << "phase=0\n";
            out << "candidate=" << CandidateName(_candidate) << "\n";
            out << "fixture_contract=" << GoldenFixtureContract << "\n";
            out << "plan_baseline="
                << "bb8f619da7abbe614ea60765006f290a60938f98\n";
            out << "map=" << GoldenRoom << "\n";
            out << "mode=Battle\n";
            out << "hunter=Samus\n";
            out << "recolor=0\n";
            out << "required_dimensions="
                << GoldenWidth << "x" << GoldenHeight << "\n";
            out << "actual_framebuffer="
                << framebuffer.X << "x" << framebuffer.Y << "\n";
            out << "actual_client="
                << client.X << "x" << client.Y << "\n";
            out << "actual_scene="
                << sceneSize.X << "x" << sceneSize.Y << "\n";
            out << "warmup_updates=" << GoldenWarmupUpdates << "\n";
            out << "capture_update_ordinal="
                << GoldenCaptureUpdate << "\n";
            out << "trigger=after OnDrawFrame on update ordinal "
                << GoldenCaptureUpdate
                << "; final-stage candidates render a control, re-run "
                   "OnDrawFrame without advancing simulation, then render "
                   "the intended production final-stage path\n";
            out << "camera_mode="
                << (UsesSyntheticFixture(_candidate)
                    ? "production-player-camera-after-warmup"
                    : "player-debug-override")
                << "\n";
            if (UsesSyntheticFixture(_candidate))
            {
                out << "camera_position=production-player-camera-after-warmup\n";
                out << "camera_target=production-player-camera-after-warmup\n";
                out << "fov_degrees=production-player-camera-after-warmup\n";
            }
            else
            {
                out << "camera_position=0,16,30\n";
                out << "camera_target=0,1,0\n";
                out << "fov_degrees=78\n";
            }
            out << "synthetic_fixture_space=camera-relative\n";
            out << "resolution_scale=100\n";
            out << "lighting=on\n";
            out << "cel=off\n";
            out << "cel_bands=8\n";
            out << "cel_edge=0.5\n";
            out << "fog=off\n";
            out << "texture_filtering=off\n";
            out << "fixture=" << FixtureDescription() << "\n";
            out << "final_stage_gate="
                << (_finalStageGateVerified ? "verified"
                    : "not-applicable-or-not-reached")
                << "\n";
            out << "control="
                << (_controlDescription.empty() ? "none" : _controlDescription)
                << "\n";
            if (_controlSummary.has_value())
            {
                out << "control_rgb_fnv1a64="
                    << _controlSummary->Fnv1a64 << "\n";
                out << "control_rgb_lit_pixels="
                    << _controlSummary->LitPixelCount << "\n";
            }
            if (_captureSummary.has_value())
            {
                out << "capture_rgb_fnv1a64="
                    << _captureSummary->Fnv1a64 << "\n";
                out << "capture_rgb_lit_pixels="
                    << _captureSummary->LitPixelCount << "\n";
                out << "capture_rgb_total_pixels="
                    << _captureSummary->PixelCount << "\n";
            }
            if (_changedPixelCount.has_value())
            {
                out << "control_changed_pixels="
                    << *_changedPixelCount << "\n";
            }
            if (_candidate == GoldenCandidate::Fade)
            {
                out << "fade_target_type=FadeOutWhite\n";
                out << "fade_target_color=1\n";
                out << "fade_target_percent=0.5\n";
                if (_fadeObservation.has_value())
                {
                    out << "fade_update_observed="
                        << (_fadeObservation->UpdateObserved ? "true" : "false")
                        << "\n";
                    if (_fadeObservation->UpdateObserved)
                    {
                        out << "fade_update_percent="
                            << _fadeObservation->UpdatePercent << "\n";
                    }
                    out << "fade_draw_observed="
                        << (_fadeObservation->DrawObserved ? "true" : "false")
                        << "\n";
                    if (_fadeObservation->DrawObserved)
                    {
                        out << "fade_draw_type_value="
                            << _fadeObservation->DrawType << "\n";
                        out << "fade_draw_color="
                            << _fadeObservation->DrawColor << "\n";
                        out << "fade_draw_percent="
                            << _fadeObservation->DrawPercent << "\n";
                    }
                }
                else
                {
                    out << "fade_update_observed=false\n";
                    out << "fade_draw_observed=false\n";
                }
            }
            out << "fixture_scope="
                << (UsesSyntheticFixture(_candidate)
                    ? "synthetic geometry only; validates production "
                      "RenderItem classification/pass/draw handling, "
                      "not entity/effect generation"
                    : "production final render stage with deterministic "
                      "debug-only state injection")
                << "\n";
            out << "captured="
                << (_captured ? "true" : "false") << "\n";
            out << "image=" << _imagePath << "\n";
            if (!_error.empty())
            {
                out << "error=" << _error << "\n";
            }

            MphRead::NativeRuntime::FileWriteAllText(
                _manifestPath,
                out.str());
            _manifestWritten = true;
        }

        GoldenCandidate _candidate;
        std::string _directory;
        std::string _imagePath;
        std::string _manifestPath;
        std::shared_ptr<MphRead::RendererPlatform::Window> _window{};
        std::unique_ptr<MphRead::NativeRuntime::Rhi::Swapchain> _swapchain{};
        std::shared_ptr<MphRead::Scene> _scene{};
        std::int32_t _fixtureTexture = 0;
        std::int32_t _updateOrdinal = 0;
        bool _captured = false;
        bool _cleaned = false;
        bool _manifestWritten = false;
        bool _finalStageGateVerified = false;
        std::string _controlDescription{};
        std::optional<MphRead::Mods::Render::GoldenCaptureValidation::PixelSummary>
            _controlSummary{};
        std::optional<MphRead::Mods::Render::GoldenCaptureValidation::PixelSummary>
            _captureSummary{};
        std::optional<std::size_t> _changedPixelCount{};
        std::optional<MphRead::Mods::Render::GoldenCaptureValidation::FadeObservation>
            _fadeObservation{};
        std::string _error{};
    };

    [[nodiscard]] std::int32_t RunOne(
        GoldenCandidate candidate,
        const std::string& directory,
        std::optional<
            MphRead::Mods::Render::GoldenCaptureValidation::PixelSummary>*
            captureSummary = nullptr)
    {
        GoldenCaptureWindow window(candidate, directory);
        window.Run();
        if (!window.Succeeded())
        {
            std::cerr
                << "[goldencapture] "
                << CandidateName(candidate)
                << " failed"
                << std::endl;
            return 1;
        }

        const auto summary = window.CaptureSummary();
        if (!summary.has_value())
        {
            std::cerr
                << "[goldencapture] "
                << CandidateName(candidate)
                << " captured without an RGB summary"
                << std::endl;
            return 1;
        }
        if (captureSummary != nullptr)
        {
            *captureSummary = summary;
        }

        std::cout
            << "[goldencapture] "
            << CandidateName(candidate)
            << " captured"
            << std::endl;
        return 0;
    }
#endif
}

namespace MphRead
{
    void Scene::ModGoldenInjectFixture(
        Mods::Render::GoldenFixture fixture,
        std::int32_t textureBindingId)
    {
        const auto makeItem = [this](
            RenderItemType type,
            RenderMode mode,
            float alpha,
            std::size_t pointCount,
            bool textured,
            std::int32_t binding)
        {
            std::shared_ptr<::MphRead::RenderItem> item = GetRenderItem();
            item->Type = type;
            item->PolygonId = GetNextPolygonId();
            item->Alpha = alpha;
            item->PolygonMode = PolygonMode::Modulate;
            item->RenderMode = mode;
            item->CullingMode = CullingMode::Neither;
            item->BillboardMode = BillboardMode::None;
            item->Wireframe = false;
            item->Lighting = false;
            item->NoLines = false;
            item->Diffuse = Vector3(1.0F, 1.0F, 1.0F);
            item->Ambient = Vector3{};
            item->Specular = Vector3{};
            item->Emission = Vector3{};
            item->LightInfo = LightInfo::Zero;
            item->TexgenMode = TexgenMode::None;
            item->XRepeat = RepeatMode::Clamp;
            item->YRepeat = RepeatMode::Clamp;
            item->HasTexture = textured;
            item->TextureBindingId = textured ? binding : 0;
            item->TexcoordMatrix = RendererDetail::IdentityMatrix();
            item->Transform = RendererDetail::IdentityMatrix();
            item->MeshModel.reset();
            item->MeshObject.reset();
            item->MatrixStackCount = 0;
            item->OverrideColor.reset();
            item->PaletteOverride.reset();
            item->Points = NativeRuntime::RentFromSharedArrayPool(
                static_cast<std::int32_t>(pointCount));
            item->ItemCount = static_cast<std::int32_t>(pointCount);
            item->ScaleS = 1.0F;
            item->ScaleT = 1.0F;
            return item;
        };

        if ((fixture == Mods::Render::GoldenFixture::Particle
                || fixture == Mods::Render::GoldenFixture::Trail)
            && textureBindingId <= 0)
        {
            throw std::invalid_argument(
                "Golden particle/trail fixture requires a valid "
                "texture binding.");
        }

        const auto cameraPoint = [this](float x, float y, float distance)
        {
            return _cameraPosition
                + Multiply(_cameraRight, x)
                + Multiply(_cameraUp, y)
                + Multiply(_cameraFacing, distance);
        };

        if (fixture
            == Mods::Render::GoldenFixture::TransparentObject)
        {
            auto item = makeItem(
                RenderItemType::Quad,
                RenderMode::Translucent,
                0.45F,
                4,
                false,
                0);
            item->OverrideColor
                = Vector4(0.10F, 0.85F, 1.0F, 0.45F);
            (*item->Points)[0] = cameraPoint(-0.8F, 0.8F, 2.5F);
            (*item->Points)[1] = cameraPoint(0.8F, 0.8F, 2.5F);
            (*item->Points)[2] = cameraPoint(0.8F, -0.8F, 2.5F);
            (*item->Points)[3] = cameraPoint(-0.8F, -0.8F, 2.5F);
            AddRenderItem(item);
            return;
        }

        if (fixture == Mods::Render::GoldenFixture::Decal)
        {
            auto item = makeItem(
                RenderItemType::Quad,
                RenderMode::Decal,
                1.0F,
                4,
                false,
                0);
            item->OverrideColor
                = Vector4(1.0F, 0.15F, 0.05F, 1.0F);
            (*item->Points)[0] = cameraPoint(-1.0F, 0.45F, 2.7F);
            (*item->Points)[1] = cameraPoint(1.0F, 0.45F, 2.7F);
            (*item->Points)[2] = cameraPoint(1.0F, -0.45F, 2.7F);
            (*item->Points)[3] = cameraPoint(-1.0F, -0.45F, 2.7F);
            AddRenderItem(item);
            return;
        }

        if (fixture == Mods::Render::GoldenFixture::Particle)
        {
            auto item = makeItem(
                RenderItemType::Particle,
                RenderMode::Translucent,
                0.85F,
                8,
                true,
                textureBindingId);
            (*item->Points)[0] = Vector3(0.0F, 0.0F, 0.0F);
            (*item->Points)[1] = cameraPoint(-0.7F, 0.7F, 2.2F);
            (*item->Points)[2] = Vector3(1.0F, 0.0F, 0.0F);
            (*item->Points)[3] = cameraPoint(0.7F, 0.7F, 2.2F);
            (*item->Points)[4] = Vector3(1.0F, 1.0F, 0.0F);
            (*item->Points)[5] = cameraPoint(0.7F, -0.7F, 2.2F);
            (*item->Points)[6] = Vector3(0.0F, 1.0F, 0.0F);
            (*item->Points)[7] = cameraPoint(-0.7F, -0.7F, 2.2F);
            AddRenderItem(item);
            return;
        }

        auto item = makeItem(
            RenderItemType::TrailMulti,
            RenderMode::Translucent,
            0.90F,
            16,
            true,
            textureBindingId);
        const std::array<Vector3, 8> trailVertices{
            cameraPoint(-1.4F, 0.35F, 2.4F),
            cameraPoint(-1.4F, -0.25F, 2.4F),
            cameraPoint(-0.5F, 0.50F, 2.4F),
            cameraPoint(-0.5F, -0.15F, 2.4F),
            cameraPoint(0.5F, 0.55F, 2.4F),
            cameraPoint(0.5F, -0.10F, 2.4F),
            cameraPoint(1.4F, 0.40F, 2.4F),
            cameraPoint(1.4F, -0.20F, 2.4F)
        };
        for (std::size_t i = 0;
            i < trailVertices.size();
            ++i)
        {
            const float u
                = static_cast<float>(i / 2U) / 3.0F;
            const float v
                = (i % 2U) == 0U ? 0.0F : 1.0F;
            (*item->Points)[i * 2U]
                = Vector3(u, v, 0.0F);
            (*item->Points)[i * 2U + 1U]
                = trailVertices[i];
        }
        item->ItemCount = 16;
        AddRenderItem(item);
    }

    void Scene::ModGoldenSetPlayerCamera(
        Vector3 position,
        Vector3 target,
        float fovDegrees)
    {
        const std::shared_ptr<Entities::PlayerEntity> main
            = Entities::PlayerEntity::Main();
        if (!main)
        {
            throw std::runtime_error(
                "Golden capture requires a main player.");
        }
        if ((main->LoadFlags() & Entities::LoadFlags::Active)
            != Entities::LoadFlags::Active)
        {
            throw std::runtime_error(
                "Golden capture main player is not active "
                "after warm-up.");
        }

        const std::shared_ptr<Entities::CameraInfo> camera
            = main->CameraInfo();
        if (!camera)
        {
            throw std::runtime_error(
                "Golden capture main player has no camera.");
        }

        camera->PrevPosition = position;
        camera->Position = position;
        camera->Target = target;
        camera->UpVector = Vector3(0.0F, 1.0F, 0.0F);
        camera->Fov = fovDegrees;
        camera->Update();

        _cameraMode = CameraMode::Player;
        _inputMode = InputMode::CameraOnly;
        _cameraPosition = position;
        _cameraFacing = camera->Facing;
        _cameraUp = camera->TrueUp;
        _cameraRight
            = Vector3::Cross(_cameraFacing, _cameraUp).Normalized();
    }

    void Scene::ModGoldenResetFinalStageState() noexcept
    {
        _fadeType = MphRead::FadeType::None;
        _fadeColor = 0.0F;
        _fadeIn = false;
        _fadeStart = 0.0F;
        _fadeLength = 0.0F;
        _fadePercent = 0.0F;
        _fadeDelay = 0.0F;
        _fadeEnded = false;
        _afterFade = AfterFade::None;
        _modGoldenFadeObservationArmed = false;
        _modGoldenFadeObservation = {};

        const auto main = Entities::PlayerEntity::Main();
        if (main)
        {
            main->ModGoldenSetHudShift(
                0,
                0.0F,
                -1,
                0.0F,
                0.0F);
        }
    }

    void Scene::ModGoldenSetHudPassEnabled(bool enabled) noexcept
    {
        _cameraMode = enabled
            ? MphRead::CameraMode::Player
            : MphRead::CameraMode::Roam;
    }

    bool Scene::ModGoldenFinalStageReady() const noexcept
    {
        const auto main = Entities::PlayerEntity::Main();
        return main
            && ((main->LoadFlags() & Entities::LoadFlags::Active)
                == Entities::LoadFlags::Active)
            && _cameraMode == MphRead::CameraMode::Player;
    }

    bool Scene::ModGoldenFadeStateMatches(
        float color,
        float percent) const noexcept
    {
        const MphRead::FadeType expectedType = color >= 0.5F
            ? MphRead::FadeType::FadeOutWhite
            : MphRead::FadeType::FadeOutBlack;
        const float expectedPercent = std::clamp(percent, 0.0F, 1.0F);
        const MphRead::Mods::Render::GoldenCaptureValidation::FadeFixtureTiming
            timing
            = MphRead::Mods::Render::GoldenCaptureValidation::MakeDeterministicFadeTiming(
                _globalElapsedTime,
                expectedPercent);
        return _fadeType == expectedType
            && _fadeColor == std::clamp(color, 0.0F, 1.0F)
            && !_fadeIn
            && MphRead::Mods::Render::GoldenCaptureValidation::FadeValueMatches(
                _fadePercent,
                expectedPercent)
            && MphRead::Mods::Render::GoldenCaptureValidation::FadeValueMatches(
                _fadeStart,
                timing.Start)
            && MphRead::Mods::Render::GoldenCaptureValidation::FadeValueMatches(
                _fadeLength,
                timing.Length)
            && _fadeDelay == 0.0F
            && !_fadeEnded
            && _afterFade == AfterFade::None;
    }

    void Scene::ModGoldenSetFadeState(
        float color,
        float percent) noexcept
    {
        _fadeType = color >= 0.5F
            ? MphRead::FadeType::FadeOutWhite
            : MphRead::FadeType::FadeOutBlack;
        _fadeColor = std::clamp(color, 0.0F, 1.0F);
        _fadeIn = false;
        _fadePercent = std::clamp(percent, 0.0F, 1.0F);
        const MphRead::Mods::Render::GoldenCaptureValidation::FadeFixtureTiming
            timing
            = MphRead::Mods::Render::GoldenCaptureValidation::MakeDeterministicFadeTiming(
                _globalElapsedTime,
                _fadePercent);
        _fadeStart = timing.Start;
        _fadeLength = timing.Length;
        _fadeDelay = 0.0F;
        _fadeEnded = false;
        _afterFade = AfterFade::None;
    }

    void Scene::ModGoldenArmFadeObservation() noexcept
    {
        _modGoldenFadeObservation = {};
        _modGoldenFadeObservationArmed = true;
    }

    void Scene::ModGoldenObserveFadeUpdate(float percent) noexcept
    {
        if (!_modGoldenFadeObservationArmed)
        {
            return;
        }
        _modGoldenFadeObservation.UpdateObserved = true;
        _modGoldenFadeObservation.UpdatePercent = percent;
    }

    void Scene::ModGoldenObserveFadeDraw(
        std::int32_t fadeType,
        float color,
        float percent) noexcept
    {
        if (!_modGoldenFadeObservationArmed)
        {
            return;
        }
        _modGoldenFadeObservation.DrawObserved = true;
        _modGoldenFadeObservation.DrawType = fadeType;
        _modGoldenFadeObservation.DrawColor = color;
        _modGoldenFadeObservation.DrawPercent = percent;
        _modGoldenFadeObservationArmed = false;
    }

    MphRead::Mods::Render::GoldenCaptureValidation::FadeObservation
        Scene::ModGoldenFadeObservation() const noexcept
    {
        return _modGoldenFadeObservation;
    }

    void Scene::ModGoldenSetElapsedTime(
        float elapsedTime) noexcept
    {
        _elapsedTime = elapsedTime;
    }
}

namespace MphRead::Entities
{
    void PlayerEntity::ModGoldenSetHudShift(
        std::uint8_t disruptionState,
        float disruptionFactor,
        std::int32_t whiteoutState,
        float whiteoutFactor,
        float whiteoutAmount) noexcept
    {
        _hudDisruptedState = disruptionState;
        _hudDisruptionFactor
            = std::clamp(disruptionFactor, 0.0F, 1.0F);
        _hudDisruptedTimer = 1;
        _hudWhiteoutState = whiteoutState;
        _hudWhiteoutFactor = whiteoutFactor;
        _whiteoutAmount = whiteoutAmount;
        if (whiteoutState != -1)
        {
            UpdateWhiteoutTable(whiteoutAmount);
        }
    }

    bool PlayerEntity::ModGoldenHudShiftStateMatches(
        std::uint8_t disruptionState,
        float disruptionFactor,
        std::int32_t whiteoutState,
        float whiteoutFactor,
        float whiteoutAmount) const noexcept
    {
        return _hudDisruptedState == disruptionState
            && _hudDisruptionFactor
                == std::clamp(disruptionFactor, 0.0F, 1.0F)
            && _hudWhiteoutState == whiteoutState
            && _hudWhiteoutFactor == whiteoutFactor
            && _whiteoutAmount == whiteoutAmount;
    }
}

namespace MphRead::Mods::Render
{
    std::int32_t GoldenCapture::Run(
        const std::string& candidate,
        const std::string& directory)
    {
#if defined(MPHREAD_SHELL)
        ApplyGoldenRenderOptions();
        NativeRuntime::DirectoryCreateDirectory(directory);
        MapGen::CustomRooms::GenerateMissing();

        const std::string lowered = LowerAscii(candidate);
        if (lowered == "all")
        {
            const std::array<GoldenCandidate, 7> candidates{
                GoldenCandidate::TransparentObject,
                GoldenCandidate::Decal,
                GoldenCandidate::Particle,
                GoldenCandidate::Trail,
                GoldenCandidate::Hud,
                GoldenCandidate::Fade,
                GoldenCandidate::WhiteoutDisruption
            };
            std::int32_t failures = 0;
            std::optional<
                MphRead::Mods::Render::GoldenCaptureValidation::PixelSummary>
                hudSummary{};
            std::optional<
                MphRead::Mods::Render::GoldenCaptureValidation::PixelSummary>
                fadeSummary{};
            for (GoldenCandidate item : candidates)
            {
                auto* summary = item == GoldenCandidate::Hud
                    ? &hudSummary
                    : item == GoldenCandidate::Fade
                        ? &fadeSummary
                        : nullptr;
                failures += RunOne(item, directory, summary);
            }
            if (failures != 0)
            {
                return 1;
            }
            if (!hudSummary.has_value() || !fadeSummary.has_value())
            {
                std::cerr
                    << "[goldencapture] HUD/fade batch identity guard "
                       "has no capture summary"
                    << std::endl;
                return 1;
            }
            try
            {
                MphRead::Mods::Render::GoldenCaptureValidation::
                    RequireDistinctFingerprints(
                        *hudSummary,
                        *fadeSummary);
            }
            catch (const std::exception& exception)
            {
                std::cerr
                    << "[goldencapture] HUD/fade batch identity guard "
                       "failed: "
                    << exception.what()
                    << std::endl;
                return 1;
            }
            std::cout
                << "[goldencapture] HUD/fade raw-RGB fingerprints "
                   "are distinct"
                << std::endl;
            return 0;
        }

        const std::optional<GoldenCandidate> parsed
            = ParseCandidate(candidate);
        if (!parsed.has_value())
        {
            std::cerr
                << "[goldencapture] unknown candidate '"
                << candidate
                << "'. Expected transparent-object, decal, "
                   "particle, trail, hud, fade, "
                   "whiteout-disruption, or all."
                << std::endl;
            return 2;
        }
        return RunOne(*parsed, directory);
#else
        (void)candidate;
        (void)directory;
        NativeRuntime::ConsoleErrorWriteLine(
            "[goldencapture] desktop OpenGL shell support "
            "is not present in this build.");
        return 2;
#endif
    }
}
