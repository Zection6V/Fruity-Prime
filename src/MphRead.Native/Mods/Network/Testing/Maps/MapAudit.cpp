#include "Mods/Network/Testing/Maps/MapAudit.hpp"
#include "Mods/Network/Testing/Maps/MapAuditCleanup.hpp"
#include "Mods/Network/Testing/Maps/MapItemPickupProbe.hpp"
#include "NativeRuntime/Rhi/SceneBackend.hpp"
#include "NativeRuntime/OpenTK/GL.hpp"
#include "Entities/BombEntity.hpp"

#include "Mods/Network/Integration/Launch/NetLaunch.hpp"
#include "Mods/Network/Testing/Harness/NetTestScript.hpp"
#include "Mods/DebugLog.hpp"
#include "Mods/ScreenCapture.hpp"
#include "Mods/WorldEvents.hpp"
#include "Entities/BeamProjectileEntity.hpp"
#include "Entities/ItemInstanceEntity.hpp"
#include "Entities/ItemSpawnEntity.hpp"
#include "Entities/JumpPadEntity.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "Entities/RoomEntity.hpp"
#include "Formats/Collision.hpp"
#include "Formats/Entity.hpp"
#include "GameState.hpp"
#include "Metadata/FrontendMeta.hpp"
#include "Metadata/Metadata.hpp"
#include "Scene.hpp"
#include "NativeRuntime/System/ExceptionText.hpp"
#include "NativeRuntime/System/Managed.hpp"
#include "NativeRuntime/System/IO.hpp"
#include "NativeRuntime/OpenTK/Mathematics.hpp"
#include "NativeRuntime/Rhi/BackendFactory.hpp"
#include "Formats/Types.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeinfo>
#include <type_traits>
#include <utility>

using ::MphRead::NativeRuntime::ExceptionTypeName;
using ::MphRead::NativeRuntime::ConvertToInt32Net9;
using ::MphRead::NativeRuntime::IncrementInPlace;
using ::MphRead::NativeRuntime::MathMax;
using ::MphRead::NativeRuntime::MathMin;
using ::MphRead::NativeRuntime::PathCombine;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedMultiply;
using ::MphRead::NativeRuntime::UncheckedSubtract;
using ::MphRead::TestFlag;
using ::OpenTK::Mathematics::AddY;
using ::OpenTK::Mathematics::IsZero;
using ::OpenTK::Mathematics::Length;
using ::OpenTK::Mathematics::LengthSquared;
using ::OpenTK::Mathematics::Negate;
using ::OpenTK::Mathematics::Scale;

namespace MphRead::Mods::Network
{
    using Entities::LoadFlags;
    using OpenTK::Mathematics::Vector2i;
    using OpenTK::Mathematics::Vector3;

    namespace
    {
        template <typename TEnum>
        [[nodiscard]] TEnum RemoveFlag(TEnum value, TEnum flag) noexcept
        {
            using Underlying = std::underlying_type_t<TEnum>;
            return static_cast<TEnum>(
                static_cast<Underlying>(value) & ~static_cast<Underlying>(flag));
        }

        [[nodiscard]] std::string ReplaceRoomCharacters(std::string value)
        {
            std::replace(value.begin(), value.end(), ' ', '_');
            std::replace(value.begin(), value.end(), '-', '_');
            return value;
        }

        [[nodiscard]] std::string TwoDigits(std::int32_t value)
        {
            std::ostringstream stream;
            stream << std::setw(2) << std::setfill('0') << value;
            return stream.str();
        }

        [[nodiscard]] std::string AfflictionName(Affliction value)
        {
            if (value == Affliction::None)
            {
                return "None";
            }
            std::string result;
            auto append = [&result](std::string_view name)
            {
                if (!result.empty())
                {
                    result += ", ";
                }
                result += name;
            };
            const std::uint8_t bits = static_cast<std::uint8_t>(value);
            if ((bits & static_cast<std::uint8_t>(Affliction::Freeze)) != 0)
            {
                append("Freeze");
            }
            if ((bits & static_cast<std::uint8_t>(Affliction::Disrupt)) != 0)
            {
                append("Disrupt");
            }
            if ((bits & static_cast<std::uint8_t>(Affliction::Burn)) != 0)
            {
                append("Burn");
            }
            const std::uint8_t known = static_cast<std::uint8_t>(Affliction::Freeze)
                | static_cast<std::uint8_t>(Affliction::Disrupt)
                | static_cast<std::uint8_t>(Affliction::Burn);
            if ((bits & static_cast<std::uint8_t>(~known)) != 0)
            {
                append(std::to_string(bits));
            }
            return result;
        }

    }

    bool MapAudit::_showWindow = false;
    std::int32_t MapAudit::_drawRate = 1;
    std::optional<Vector2i> MapAudit::_windowSize{};
    bool MapAudit::_forceEveryone = false;
    bool MapAudit::_diagnostic = false;
    std::optional<Hunter> MapAudit::_mainHunter{};

    const std::array<std::pair<Hunter, std::string_view>, 3> MapAudit::_afflictions =
    {{
        {Hunter::Noxus, "freeze"},
        {Hunter::Spire, "burn"},
        {Hunter::Kanden, "disrupt"}
    }};

    RendererPlatform::WindowSettings MapAudit::GameSettings()
    {
        RendererPlatform::WindowSettings settings{};
        settings.UpdateFrequency = 60;
        return settings;
    }

    RendererPlatform::WindowSettings MapAudit::WindowSettings()
    {
        RendererPlatform::WindowSettings settings = GameSettings();
        if (_windowSize.has_value())
        {
            settings.ClientSize = *_windowSize;
        }
        else
        {
            settings.ClientSize = _showWindow ? Vector2i(1024, 576) : Vector2i(320, 180);
        }
        settings.Title = "MphRead map audit";
        settings.Profile = RendererPlatform::WindowSettings::ContextProfile::Compatability;
        settings.Flags = RendererPlatform::WindowSettings::ContextFlags::Default;
        settings.ApiMajor = 3;
        settings.ApiMinor = 2;
        settings.StartVisible = _showWindow;
        settings.GraphicsMode = RendererPlatform::GraphicsWindowMode::OpenGL;
        return settings;
    }

    bool MapAudit::ShowWindow() noexcept
    {
        return _showWindow;
    }

    void MapAudit::ShowWindow(bool value) noexcept
    {
        _showWindow = value;
    }

    std::int32_t MapAudit::DrawRate() noexcept
    {
        return _drawRate;
    }

    void MapAudit::DrawRate(std::int32_t value) noexcept
    {
        _drawRate = value;
    }

    std::optional<Vector2i> MapAudit::WindowSize() noexcept
    {
        return _windowSize;
    }

    void MapAudit::WindowSize(std::optional<Vector2i> value) noexcept
    {
        _windowSize = value;
    }

    MphRead::Scene& MapAudit::Scene() noexcept
    {
        return *_scene;
    }

    const MphRead::Scene& MapAudit::Scene() const noexcept
    {
        return *_scene;
    }

    bool MapAudit::ForceEveryone() noexcept
    {
        return _forceEveryone;
    }

    void MapAudit::ForceEveryone(bool value) noexcept
    {
        _forceEveryone = value;
    }

    bool MapAudit::Diagnostic() noexcept
    {
        return _diagnostic;
    }

    void MapAudit::Diagnostic(bool value) noexcept
    {
        _diagnostic = value;
    }

    std::optional<Hunter> MapAudit::MainHunter() noexcept
    {
        return _mainHunter;
    }

    void MapAudit::MainHunter(std::optional<Hunter> value) noexcept
    {
        _mainHunter = value;
    }

    void MapAudit::Run()
    {
        _window->Run(*this);
    }

    void MapAudit::Dispose()
    {
        _itemPickupAudit.reset();
        std::exception_ptr cleanupFailure{};
        if (_scene)
        {
            const std::array cleanupSteps{
                MapAuditCleanupStep<MapAudit>{"scene-stop", +[](MapAudit& audit)
                {
                    if (audit._cleanupExceptionProbe)
                    {
                        audit._cleanupExceptionProbe = false;
                        Mods::DebugLog::Line("map-audit-cleanup",
                            "injecting Scene::DoCleanup exception room=" + audit._room);
                        audit._scene->ModInjectCleanupFailureForDiagnostics();
                    }
                    audit._scene->DoCleanup();
                }},
                MapAuditCleanupStep<MapAudit>{"gpu-release", +[](MapAudit& audit)
                {
                    audit._scene->ReleaseGpuResources();
                }},
                MapAuditCleanupStep<MapAudit>{"scene-destroy", +[](MapAudit& audit)
                {
                    audit._scene.reset();
                }}
            };
            cleanupFailure = RunMapAuditCleanup(cleanupSteps, *this,
                +[](MapAudit&, std::string_view operation, std::exception_ptr failure)
                {
                    Mods::DebugLog::Exception("map-audit-cleanup", failure);
                    Mods::DebugLog::Line("map-audit-cleanup",
                        "cleanup-error operation=" + std::string(operation));
                });
        }
        _swapchain.reset();
        _window.reset();
        if (_movieRoomTransitionProbe)
        {
            GameState::TransitionRoomId(-1);
            GameState::TransitionState(TransitionState::None);
        }
        if (cleanupFailure)
        {
            std::rethrow_exception(cleanupFailure);
        }
    }

    OpenTK::Mathematics::Vector2i MapAudit::ClientSize() const
    {
        return _window->Size();
    }

    void MapAudit::Close()
    {
        _window->Close();
    }

    void MapAudit::Present()
    {
        _swapchain->Present();
    }

    MapAudit::MapAudit(
        std::string room,
        std::int32_t players,
        double seconds,
        GameMode mode,
        bool bots,
        bool renderProbe,
        bool itemProbe,
        bool itemPickupProbe,
        bool playMovies,
        bool movieChainProbe,
        bool movieSkipProbe,
        bool movieCloseProbe,
        bool cleanupExceptionProbe,
        bool partialInitializationProbe,
        bool movieAudioCallProbe,
        bool forceMovieForAudioCallProbe,
        bool movieRoomTransitionProbe)
        : _window(RendererPlatform::CreateWindow(WindowSettings())),
          _room(std::move(room)),
          _players(players),
          _seconds(seconds),
          _renderProbe(renderProbe),
          _itemProbe(itemProbe),
          _playMovies(playMovies),
          _movieChainProbe(movieChainProbe),
          _movieSkipProbe(movieSkipProbe),
          _movieCloseProbe(movieCloseProbe),
          _movieRoomTransitionProbe(movieRoomTransitionProbe),
          _movieAudioCallProbe(movieAudioCallProbe),
          _forceMovieForAudioCallProbe(forceMovieForAudioCallProbe),
          _cleanupExceptionProbe(cleanupExceptionProbe),
          _scene(nullptr),
          _bots(bots)
    {
        struct ConstructionCleanup final
        {
            MapAudit& Audit;
            bool Complete = false;

            ~ConstructionCleanup() noexcept
            {
                if (Complete)
                {
                    return;
                }
                try
                {
                    Mods::DebugLog::Line("map-audit-cleanup",
                        "constructor-failure cleanup begin room=" + Audit._room);
                    Audit.Dispose();
                    Mods::DebugLog::Line("map-audit-cleanup",
                        "constructor-failure cleanup complete room=" + Audit._room);
                }
                catch (...)
                {
                    try
                    {
                        Mods::DebugLog::Exception("map-audit-construction-cleanup",
                            std::current_exception());
                    }
                    catch (...)
                    {
                    }
                }
            }
        } constructionCleanup{*this};

        NativeRuntime::Rhi::SwapchainDesc swapchainDesc{};
        const Vector2i framebufferSize = _window->Size();
        swapchainDesc.width = static_cast<std::uint32_t>(std::max(framebufferSize.X, 1));
        swapchainDesc.height = static_cast<std::uint32_t>(std::max(framebufferSize.Y, 1));
        _swapchain = NativeRuntime::Rhi::BackendFactory::CreateSwapchain(
            NativeRuntime::Rhi::GraphicsBackend::OpenGl, *_window, swapchainDesc);
        NetTestScript::SetPhaseSeconds(MathMax(1.5, seconds / NetTestScript::PhaseCount()));
        Entities::PlayerEntity::SetMaxPlayers(std::max(Entities::PlayerEntity::MaxPlayers(), players));
        _forceEveryone = true;
        Mods::WorldEvents::Watching(true);
        Mods::WorldEvents::Reset();

        _scene = std::make_unique<MphRead::Scene>(
            _window->Size(),
            _window->Keyboard(),
            _window->Mouse(),
            [](auto&&) { },
            [this]() { Close(); });
        _scene->ModSetMoviePlaybackPolicy(playMovies
            ? ::MphRead::Mods::MoviePlaybackPolicy::Normal
            : ::MphRead::Mods::MoviePlaybackPolicy::SuppressForAudit);
        if (_movieAudioCallProbe)
        {
            _scene->ModArmMovieAudioCallProbeForDiagnostics();
        }
        if (playMovies)
        {
            ::MphRead::Mods::DebugLog::Line(
                "movie", "map-audit movie_playback=normal room=" + _room);
        }

        for (std::int32_t i = 0; i < players; ++i)
        {
            Hunter hunter = i == 0 && _mainHunter.has_value()
                ? *_mainHunter
                : static_cast<Hunter>(i % 7);
            _scene->AddPlayer(hunter, 0, -1);
        }

        for (std::int32_t i = 0;
            i < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size());
            ++i)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players()[static_cast<std::size_t>(i)];
            player->SetIsBot(bots && i > 0);
            player->SetBotLevel(bots ? 1 : 0);
            if (i >= players)
            {
                player->SetLoadFlags(RemoveFlag(player->LoadFlags(), LoadFlags::Active));
            }
        }

        Entities::PlayerEntity::SetPlayerCount(players);
        Entities::PlayerEntity::SetMainPlayerIndex(0);
        _scene->AddRoom(_room, mode, NetLaunch::RoomPlayerCount());
        if (_movieRoomTransitionProbe)
        {
            const auto [firstTarget, firstTargetId] = Metadata::GetRoomByName("UNIT4_RM1");
            const auto [secondTarget, secondTargetId] = Metadata::GetRoomByName("UNIT4_RM2");
            if (firstTarget == nullptr || secondTarget == nullptr
                || firstTargetId < 0 || secondTargetId < 0)
            {
                throw std::runtime_error("The Story movie transition targets are unavailable.");
            }
            _movieRoomTransitionTargets = {firstTargetId, secondTargetId};
            _movieRoomTransitionObservedRoomId = _scene->RoomId();
            Mods::DebugLog::Line("movie", "map-audit movie_room_transition_waiting_for_start_game from="
                + _room + " to=UNIT4_RM1,UNIT4_RM2");
        }
        if (partialInitializationProbe)
        {
            Mods::DebugLog::Line("map-audit-cleanup",
                "injecting partial-construction exception room=" + _room);
            throw std::runtime_error("Injected MapAudit constructor failure after Scene::AddRoom.");
        }
        if (itemPickupProbe)
        {
            _itemPickupAudit = std::make_unique<MapItemPickupProbe>(*_scene, _room);
        }
        constructionCleanup.Complete = true;
    }

    MapAudit::~MapAudit() = default;

    void MapAudit::OnLoad()
    {
        _scene->Size(ClientSize());
        _scene->OnLoad();
        _window->BaseOnLoad();
        ::MphRead::NativeRuntime::Rhi::ResetWindowViewport(ClientSize().X, ClientSize().Y);
        _scene->OnResize();
    }

    void MapAudit::OnRenderFrame(const RendererPlatform::FrameEventArgs& args)
    {
        GameState::ApplyPause();
        if (_itemPickupAudit != nullptr)
        {
            _itemPickupAudit->Start();
        }
        if (_movieChainProbe && !_movieChainStarted)
        {
            _scene->StartMovies(::MphRead::Movie::BadEndingPart1, ::MphRead::Movie::BadEndingPart2,
                FadeType::FadeOutInWhite, 45.0F / 30.0F, FadeType::FadeOutBlack, 0.0F,
                ::MphRead::AfterMovie::StartGame);
            _movieChainStarted = true;
            ::MphRead::Mods::DebugLog::Line("movie", "map-audit movie_chain=32,35 room=" + _room);
        }
        _scene->OnSimulationFrame();
        if (_movieRoomTransitionProbe && !_movieRoomTransitionStarted
            && _scene->MoviePlaying())
        {
            _movieRoomTransitionInitialMovieSeen = true;
        }
        if (_forceMovieForAudioCallProbe && _room != "UNIT4_LAND"
            && !_movieAudioCallProbeMovieStarted
            && !_scene->MoviePlaying())
        {
            // The cycle checks task and resource isolation across real room
            // Scenes; it does not need to reproduce the fade trigger for each room.
            _scene->ModPlayMovieForDiagnostics(::MphRead::Movie::ArcterraLanding);
            _movieAudioCallProbeMovieStarted = true;
            ::MphRead::Mods::DebugLog::Line("movie", "map-audit movie_forced_for_scene_cycle room=" + _room
                + " movie_id=7");
        }
        if ((_movieSkipProbe || _movieRoomTransitionProbe)
            && !_movieSkipRequested && _scene->MoviePlaying())
        {
            ++_movieFramesObserved;
            if (_movieFramesObserved >= 30)
            {
                _scene->SkipMovie();
                _movieSkipRequested = true;
                ::MphRead::Mods::DebugLog::Line("movie", "map-audit movie_skip_requested room=" + _room
                    + " movie_frames=" + std::to_string(_movieFramesObserved));
            }
        }
        const std::uint64_t frameCountBefore = _scene->FrameCount();
        const std::int32_t draws = std::max<std::int32_t>(1, _drawRate);
        std::pair<std::uint64_t, std::int32_t> previousTrail{};

        for (std::int32_t i = 0; i < draws; ++i)
        {
            _scene->OnDrawFrame();
            if (!_scene->OnRenderFrame())
            {
                return;
            }
            if (_drawRate > 1)
            {
                const std::pair<std::uint64_t, std::int32_t> trail = _scene->ModLockjawTrailSignature();
                if (i > 0 && (trail.second > 0 || previousTrail.second > 0))
                {
                    IncrementInPlace(_lockjawTrailChecks);
                    if (trail != previousTrail)
                    {
                        IncrementInPlace(_lockjawTrailMismatches);
                    }
                }
                previousTrail = trail;
            }
            if (i < draws - 1)
            {
                Present();
                _scene->AfterRenderFrame();
            }
        }

        _lockjawDrawRngChanges = Entities::BombEntity::ModLockjawDrawRngChanges;
        if (_scene->FrameCount() != frameCountBefore)
        {
            IncrementInPlace(_drawAdvancedTheGame);
        }

        IncrementInPlace(_frame);

        if (_itemProbe)
        {
            if (!StepItemShots())
            {
                Present();
                _scene->AfterRenderFrame();
                _window->BaseOnRenderFrame(args);
                Close();
                return;
            }
            Present();
            _scene->AfterRenderFrame();
            _window->BaseOnRenderFrame(args);
            return;
        }

        if (_itemPickupAudit != nullptr)
        {
            if (!_itemPickupAudit->Step(Entities::PlayerEntity::Main()))
            {
                Present();
                _scene->AfterRenderFrame();
                _window->BaseOnRenderFrame(args);
                Close();
                return;
            }
            Present();
            _scene->AfterRenderFrame();
            _window->BaseOnRenderFrame(args);
            return;
        }

        if (_renderProbe)
        {
            if (!StepSpawnRender())
            {
                Present();
                _scene->AfterRenderFrame();
                _window->BaseOnRenderFrame(args);
                Close();
                return;
            }
            Present();
            _scene->AfterRenderFrame();
            _window->BaseOnRenderFrame(args);
            return;
        }

        Drive();
        StepScoreboard();
        Observe();
        SampleRender();
        Present();
        _scene->AfterRenderFrame();
        _window->BaseOnRenderFrame(args);

        if (_movieRoomTransitionProbe && _movieRoomTransitionInitialMovieSeen
            && !_movieRoomTransitionStarted && !_scene->MoviePlaying())
        {
            if (!_scene->ModMovieMuteStateBalancedForDiagnostics(1, "start-game"))
            {
                std::cout << "MAPMOVIETRANSITION FAIL mute_state=unbalanced phase=start-game\n";
                Close();
                return;
            }
            GameState::TransitionRoomId(
                _movieRoomTransitionTargets[_movieRoomTransitionIndex]);
            _movieFramesObserved = 0;
            _movieSkipRequested = false;
            _scene->StartMovie(::MphRead::Movie::ArcterraLanding,
                FadeType::FadeOutInBlack, 0.0F, FadeType::FadeOutWhite, 5.0F / 30.0F,
                AfterMovie::LoadRoom);
            _movieRoomTransitionStarted = true;
            Mods::DebugLog::Line("movie", "map-audit movie_room_transition_armed from="
                + _room + " to=UNIT4_RM1,UNIT4_RM2 after_action=load_room start_game=1");
        }

        if (_movieRoomTransitionProbe && !_movieRoomTransitionPassed
            && _scene->RoomId() != _movieRoomTransitionObservedRoomId)
        {
            const std::int32_t expectedRoomId
                = _movieRoomTransitionTargets[_movieRoomTransitionIndex];
            if (_scene->RoomId() != expectedRoomId || _scene->MoviePlaying())
            {
                Mods::DebugLog::Line("movie", "map-audit movie_room_transition_fail expected_id="
                    + std::to_string(expectedRoomId)
                    + " actual_id=" + std::to_string(_scene->RoomId())
                    + " movie_playing=" + std::to_string(_scene->MoviePlaying()));
                std::cout << "MAPMOVIETRANSITION FAIL expected_room_id=" << expectedRoomId
                    << " actual_room_id=" << _scene->RoomId()
                    << " movie_playing=" << _scene->MoviePlaying() << '\n';
                Close();
                return;
            }

            const std::uint32_t expectedMovieCount
                = static_cast<std::uint32_t>(_movieRoomTransitionIndex + 2);
            if (!_scene->ModMovieMuteStateBalancedForDiagnostics(
                    expectedMovieCount, _scene->Room()->Meta().Name))
            {
                std::cout << "MAPMOVIETRANSITION FAIL mute_state=unbalanced room="
                    << _scene->Room()->Meta().Name << '\n';
                Close();
                return;
            }

            _movieRoomTransitionObservedRoomId = _scene->RoomId();
            ++_movieRoomTransitionIndex;
            Mods::DebugLog::Line("movie", "map-audit movie_room_transition_complete room="
                + _scene->Room()->Meta().Name);
            _movieRoomTransitionFramesSinceLoad = 0;
            _movieRoomTransitionCapturePending = true;
        }

        if (_movieRoomTransitionProbe && _movieRoomTransitionCapturePending
            && ++_movieRoomTransitionFramesSinceLoad >= 30)
        {
            const std::string roomName = _scene->Room()->Meta().Name;
            if (_shotDirectory.has_value())
            {
                const std::string shotPath = PathCombine(
                    *_shotDirectory,
                    ReplaceRoomCharacters(roomName) + "-story-transition.png");
                const bool saved = Mods::ScreenCapture::SaveWindow(_scene.get(), shotPath);
                const std::string scenePath = PathCombine(
                    *_shotDirectory,
                    ReplaceRoomCharacters(roomName) + "-story-scene.png");
                const bool sceneSaved = Mods::ScreenCapture::Save(_scene.get(), scenePath);
                Mods::DebugLog::Line("movie", "map-audit movie_room_transition_screenshot room="
                    + roomName + " window_saved=" + std::to_string(saved)
                    + " scene_saved=" + std::to_string(sceneSaved));
            }

            _movieRoomTransitionCapturePending = false;
            if (_movieRoomTransitionIndex == _movieRoomTransitionTargets.size())
            {
                _movieRoomTransitionPassed = true;
                Mods::DebugLog::Line("movie",
                    "map-audit movie_room_transition_pass rooms=UNIT4_LAND,UNIT4_RM1,UNIT4_RM2");
                std::cout << "MAPMOVIETRANSITION PASS rooms=UNIT4_LAND,UNIT4_RM1,UNIT4_RM2\n";
                Close();
                return;
            }

            GameState::TransitionRoomId(
                _movieRoomTransitionTargets[_movieRoomTransitionIndex]);
            _movieFramesObserved = 0;
            _movieSkipRequested = false;
            _scene->StartMovie(::MphRead::Movie::ArcterraLanding,
                FadeType::FadeOutInBlack, 0.0F, FadeType::FadeOutWhite, 5.0F / 30.0F,
                AfterMovie::LoadRoom);
            Mods::DebugLog::Line("movie", "map-audit movie_room_transition_next room="
                + std::to_string(_movieRoomTransitionTargets[_movieRoomTransitionIndex])
                + " movie_id=7 after_action=load_room");
        }

        if (_movieAudioCallProbe && !_movieCloseRequested
            && _scene->ModMovieAudioCallProbeEntered())
        {
            _movieCloseRequested = true;
            ::MphRead::Mods::DebugLog::Line("movie",
                "map-audit movie_openal_call_probe_close_requested room=" + _room
                + " frame=" + std::to_string(_frame)
                + " operation=SourceQueueBuffers");
            Close();
        }
        else if (_movieCloseProbe && !_movieCloseRequested && _scene->MoviePlaying())
        {
            _movieCloseRequested = true;
            ::MphRead::Mods::DebugLog::Line("movie", "map-audit movie_close_requested room=" + _room
                + " frame=" + std::to_string(_frame));
            Close();
        }

        if (_frame >= _seconds * 60.0
            && (_bots || (!StepProbe() && !StepAfflictionProbe())))
        {
            Close();
        }
    }

    void MapAudit::Drive()
    {
        for (std::int32_t slot = 0;
            slot < _players
                && slot < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size());
            ++slot)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players()[static_cast<std::size_t>(slot)];
            if (!TestFlag(player->LoadFlags(), LoadFlags::Active))
            {
                continue;
            }
            if (!TestFlag(player->LoadFlags(), LoadFlags::Spawned) || player->Health() == 0)
            {
                continue;
            }

            if (_afflictIndex >= 0)
            {
                if (slot != _afflictShooter && slot != _afflictVictim)
                {
                    NetTestScript::Rest(player, false);
                }
                continue;
            }

            if (_bots)
            {
                continue;
            }

            if (_probeIndex >= 0 && slot == _probeSlot)
            {
                NetTestScript::Rest(player, true);
                continue;
            }

            NetTestScript::ApplyOffline(player, slot, _frame);
            player->ModApplyScriptAim(NetTestScript::AimDeltaX(), NetTestScript::AimDeltaY());
        }
    }

    bool MapAudit::StepProbe()
    {
        if (_probeIndex < 0)
        {
            CollectProbeTargets();
            _probeIndex = 0;
        }

        if (_probeIndex >= static_cast<std::int32_t>(_probeTargets.size())
            || _probeSlot >= static_cast<std::int32_t>(Entities::PlayerEntity::Players().size()))
        {
            return false;
        }

        std::shared_ptr<Entities::PlayerEntity> player
            = Entities::PlayerEntity::Players()[static_cast<std::size_t>(_probeSlot)];
        std::shared_ptr<Entities::EntityBase> target
            = _probeTargets[static_cast<std::size_t>(_probeIndex)];

        if (!_probePlaced)
        {
            if (!TestFlag(player->LoadFlags(), LoadFlags::Active)
                || !TestFlag(player->LoadFlags(), LoadFlags::Spawned)
                || player->Health() == 0)
            {
                NetTestScript::Rest(player, true);
                if (++_probeWait > 240)
                {
                    _probeIndex = static_cast<std::int32_t>(_probeTargets.size());
                }
                return true;
            }

            _probeWait = 0;
            _probeMarkPads = Mods::WorldEvents::JumpPadsFor(_probeSlot);
            _probeMarkTeleports = Mods::WorldEvents::TeleportsFor(_probeSlot);

            const float lift = _probeAttempt == 1 ? 1.6F : 0.5F;
            player->ModForceForm(_probeAttempt == 2);
            const Vector3 spot = AddY(TriggerPoint(*target), lift);
            player->Teleport(spot, target->FacingVector(), _scene->GetNodeRefByPosition(spot));
            _probePlaced = true;
            _probeFrames = 0;
            return true;
        }

        ++_probeFrames;

        const bool fired = target->Type == EntityType::JumpPad
            ? Mods::WorldEvents::JumpPadsFor(_probeSlot) > _probeMarkPads
                && Mods::WorldEvents::LastJumpPadId(_probeSlot) == target->Id
            : Mods::WorldEvents::TeleportsFor(_probeSlot) > _probeMarkTeleports
                && Mods::WorldEvents::LastTeleporterId(_probeSlot) == target->Id;

        const std::int32_t limit
            = _probeAttempt == 0 ? _probeFrameLimit : _probeRetryFrameLimit;
        if (!fired && _probeFrames < limit)
        {
            return true;
        }

        if (!fired && _probeAttempt < 2)
        {
            ++_probeAttempt;
            _probePlaced = false;
            return true;
        }

        _probeAttempt = 0;
        player->ModForceForm(false);

        if (target->Type == EntityType::JumpPad)
        {
            ++_padsProbed;
            if (fired)
            {
                ++_padsFired;
            }
        }
        else
        {
            ++_telesProbed;
            if (fired)
            {
                ++_telesFired;
            }
        }

        ++_probeIndex;
        _probePlaced = false;
        return true;
    }

    bool MapAudit::StepAfflictionProbe()
    {
        if (_afflictIndex < 0)
        {
            _afflictIndex = 0;
            _afflictFrames = 0;
            _afflictShooter = -1;
        }

        if (_afflictIndex >= static_cast<std::int32_t>(_afflictions.size()))
        {
            return false;
        }

        if (_afflictShooter < 0)
        {
            if (!SetUpAffliction(_afflictions[static_cast<std::size_t>(_afflictIndex)].first))
            {
                if (++_afflictSetUpWait > 150)
                {
                    _afflictSetUpWait = 0;
                    ++_afflictIndex;
                }
                return true;
            }

            _afflictSetUpWait = 0;
            _afflictTried[static_cast<std::size_t>(_afflictIndex)] = true;
            _afflictFrames = 0;
            _afflictVictimHealth
                = Entities::PlayerEntity::Players()[static_cast<std::size_t>(_afflictVictim)]->Health();
            return true;
        }

        std::shared_ptr<Entities::PlayerEntity> shooter
            = Entities::PlayerEntity::Players()[static_cast<std::size_t>(_afflictShooter)];
        std::shared_ptr<Entities::PlayerEntity> victim
            = Entities::PlayerEntity::Players()[static_cast<std::size_t>(_afflictVictim)];

        if (!Alive(*shooter) || !Alive(*victim))
        {
            NetTestScript::Rest(shooter, true);
            NetTestScript::Rest(victim, true);
            if (++_afflictWait > 240)
            {
                _afflictLanded[static_cast<std::size_t>(_afflictIndex)] = false;
                ++_afflictIndex;
                _afflictShooter = -1;
                _afflictVictim = -1;
                _afflictWait = 0;
            }
            return true;
        }

        _afflictWait = 0;

        if (shooter->IsAltForm() || shooter->IsMorphing() || shooter->IsUnmorphing())
        {
            NetTestScript::Rest(shooter, true);
            NetTestScript::Rest(victim, true);
            return true;
        }

        ++_afflictFrames;

        shooter->ModArmAffinityWeapon();

        const Vector3 shooterChest = AddY(static_cast<Vector3>(shooter->Position), 0.5F);
        const Vector3 victimChest = AddY(static_cast<Vector3>(victim->Position), 0.5F);
        const Vector3 toVictim = victimChest - shooterChest;
        if (LengthSquared(toVictim) > 0.001F)
        {
            shooter->ModSetAim(toVictim.Normalized());
        }

        NetTestScript::HoldFire(shooter, !shooter->ModChargeReady());
        NetTestScript::Rest(victim, true);

        {
            auto enumerator = _scene->Entities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                std::shared_ptr<Entities::EntityBase> entity = enumerator.Current();
                if (entity->Type != EntityType::BeamProjectile)
                {
                    continue;
                }

                auto shot = std::dynamic_pointer_cast<Entities::BeamProjectileEntity>(entity);
                if (shot && shot->Owner().get() == shooter.get())
                {
                    _afflictFired[static_cast<std::size_t>(_afflictIndex)] = true;
                    _afflictShotAfflictions = static_cast<Affliction>(
                        static_cast<std::uint8_t>(_afflictShotAfflictions)
                        | static_cast<std::uint8_t>(shot->Afflictions()));
                    break;
                }
            }
        }

        if (victim->Health() < _afflictVictimHealth)
        {
            _afflictHit[static_cast<std::size_t>(_afflictIndex)] = true;
        }
        _afflictVictimHealth = std::min(_afflictVictimHealth, victim->Health());
        _afflictMaxCharge = std::max(_afflictMaxCharge, shooter->ModChargeLevel());

        bool landed;
        switch (_afflictIndex)
        {
            case 0:
                landed = victim->ModFrozen();
                break;
            case 1:
                landed = victim->ModBurning();
                break;
            default:
                landed = victim->ModDisrupted();
                break;
        }

        if (landed || _afflictFrames >= _afflictFrameLimit)
        {
            if (_diagnostic)
            {
                std::cout
                    << "PROBE "
                    << _afflictions[static_cast<std::size_t>(_afflictIndex)].second
                    << ": shooter slot " << _afflictShooter << ' '
                    << ::MphRead::ToString(shooter->Hunter()) << ' '
                    << shooter->ModWeaponState()
                    << ", maxcharge=" << _afflictMaxCharge
                    << ", shots carried " << AfflictionName(_afflictShotAfflictions)
                    << ", landed=" << (landed ? "True" : "False")
                    << '\n';
            }

            _afflictLanded[static_cast<std::size_t>(_afflictIndex)] = landed;
            ++_afflictIndex;
            _afflictShooter = -1;
            _afflictVictim = -1;
            _afflictShotAfflictions = Affliction::None;
            _afflictMaxCharge = 0;
        }

        return true;
    }

    bool MapAudit::Alive(Entities::PlayerEntity& player)
    {
        return TestFlag(player.LoadFlags(), LoadFlags::Active)
            && TestFlag(player.LoadFlags(), LoadFlags::Spawned)
            && player.Health() > 0;
    }

    bool MapAudit::SetUpAffliction(Hunter hunter)
    {
        std::int32_t shooter = -1;
        std::int32_t victim = -1;

        for (std::int32_t slot = 0;
            slot < _players
                && slot < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size());
            ++slot)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players()[static_cast<std::size_t>(slot)];
            if (!Alive(*player))
            {
                continue;
            }
            if (shooter < 0 && player->Hunter() == hunter)
            {
                shooter = slot;
            }
            else if (victim < 0)
            {
                victim = slot;
            }
        }

        if (shooter < 0 || victim < 0)
        {
            return false;
        }

        std::shared_ptr<Entities::PlayerEntity> shooterPlayer
            = Entities::PlayerEntity::Players()[static_cast<std::size_t>(shooter)];
        std::shared_ptr<Entities::PlayerEntity> victimPlayer
            = Entities::PlayerEntity::Players()[static_cast<std::size_t>(victim)];

        Vector3 facing = victimPlayer->FacingVector();
        facing = Vector3(facing.X, 0.0F, facing.Z);
        if (LengthSquared(facing) < 0.001F)
        {
            facing = Vector3(0.0F, 0.0F, 1.0F);
        }
        facing = facing.Normalized();

        const Vector3 victimPosition = victimPlayer->Position;
        const Vector3 spot = victimPosition + Scale(facing, 2.2F);
        shooterPlayer->Teleport(spot, Negate(facing), _scene->GetNodeRefByPosition(spot));
        shooterPlayer->ModArmAffinityWeapon();
        _afflictShooter = shooter;
        _afflictVictim = victim;
        return true;
    }

    Vector3 MapAudit::VolumeCenter(const CollisionVolume& volume)
    {
        switch (volume.Type)
        {
            case VolumeType::Box:
            {
                const Vector3 v1 = Scale(volume.BoxVector1, volume.BoxDot1);
                const Vector3 v2 = Scale(volume.BoxVector2, volume.BoxDot2);
                const Vector3 v3 = Scale(volume.BoxVector3, volume.BoxDot3);
                return volume.BoxPosition + Scale(v1 + v2 + v3, 0.5F);
            }
            case VolumeType::Cylinder:
                return volume.CylinderPosition
                    + Scale(volume.CylinderVector, volume.CylinderDot / 2.0F);
            case VolumeType::Sphere:
                return volume.SpherePosition;
            default:
                return Vector3::Zero;
        }
    }

    Vector3 MapAudit::TriggerPoint(Entities::EntityBase& entity)
    {
        if (auto* pad = dynamic_cast<Entities::JumpPadEntity*>(&entity))
        {
            const Vector3 center = VolumeCenter(pad->ModVolume());
            if (!IsZero(center))
            {
                return center;
            }
        }
        return entity.Position;
    }

    void MapAudit::CollectProbeTargets()
    {
        auto enumerator = _scene->Entities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            if (_probeTargets.size() >= static_cast<std::size_t>(_probeMaxTargets))
            {
                break;
            }

            std::shared_ptr<Entities::EntityBase> entity = enumerator.Current();
            if (entity->Active
                && (entity->Type == EntityType::JumpPad
                    || entity->Type == EntityType::Teleporter))
            {
                _probeTargets.push_back(std::move(entity));
            }
        }
    }

    void MapAudit::StepScoreboard()
    {
        const std::int32_t total
            = MathMax(std::int32_t{60}, ConvertToInt32Net9(_seconds * 60.0));
        const bool show
            = (_frame > total / 6 && _frame < total / 6 + 90)
            || (_frame > UncheckedMultiply(total, 2) / 3
                && _frame < UncheckedMultiply(total, 2) / 3 + 90);
        Entities::PlayerEntity::SetModForceScoreboard(show);
        if (show)
        {
            IncrementInPlace(_scoreboardFrames);
        }
    }

    void MapAudit::Observe()
    {
        _spawned = 0;

        for (std::int32_t slot = 0;
            slot < _players
                && slot < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size());
            ++slot)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players()[static_cast<std::size_t>(slot)];
            if (!TestFlag(player->LoadFlags(), LoadFlags::Active)
                || !TestFlag(player->LoadFlags(), LoadFlags::Spawned))
            {
                continue;
            }

            IncrementInPlace(_spawned);
            _everSpawned[static_cast<std::size_t>(slot)] = true;
            SampleNodeLookup(*player);
            SamplePuppetNode(*player);

            if (player->IsAltForm())
            {
                _everAltForm[static_cast<std::size_t>(slot)] = true;
            }
            if (player->ModFrozen())
            {
                _everFrozen[static_cast<std::size_t>(slot)] = true;
            }
            if (player->ModBurning())
            {
                _everBurned[static_cast<std::size_t>(slot)] = true;
            }
            if (player->ModDisrupted())
            {
                _everDisrupted[static_cast<std::size_t>(slot)] = true;
            }

            if (_lastHealth[static_cast<std::size_t>(slot)] > 0 && player->Health() == 0)
            {
                IncrementInPlace(_deaths[static_cast<std::size_t>(slot)]);
            }
            _lastHealth[static_cast<std::size_t>(slot)] = player->Health();

            const Vector3 position = player->Position;
            _lowestY = MathMin(_lowestY, static_cast<double>(position.Y));

            if (_haveLastSeen[static_cast<std::size_t>(slot)])
            {
                const float step = Length(
                    position - _lastSeen[static_cast<std::size_t>(slot)]);
                if (step < 5.0F)
                {
                    _travelled[static_cast<std::size_t>(slot)] += step;
                }
            }

            _lastSeen[static_cast<std::size_t>(slot)] = position;
            _haveLastSeen[static_cast<std::size_t>(slot)] = true;
        }

        auto enumerator = _scene->Entities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            std::shared_ptr<Entities::EntityBase> entity = enumerator.Current();
            if (entity->Type != EntityType::BeamProjectile)
            {
                continue;
            }

            auto shot = std::dynamic_pointer_cast<Entities::BeamProjectileEntity>(entity);
            if (!shot)
            {
                continue;
            }

            auto owner = std::dynamic_pointer_cast<Entities::PlayerEntity>(shot->Owner());
            if (owner && owner->SlotIndex() >= 0
                && owner->SlotIndex() < static_cast<std::int32_t>(_everFired.size()))
            {
                _everFired[static_cast<std::size_t>(owner->SlotIndex())] = true;
            }
        }
    }

    void MapAudit::SampleNodeLookup(Entities::PlayerEntity& player)
    {
        const Formats::Culling::NodeRef walked = player.NodeRef;
        if (walked.PartIndex == -1)
        {
            return;
        }

        const Formats::Culling::NodeRef found
            = _scene->GetNodeRefByPosition(static_cast<Vector3>(player.Position));
        IncrementInPlace(_nodeLookupSamples);

        const bool walkedVisible = _scene->IsNodeRefVisible(walked);
        if (walkedVisible)
        {
            IncrementInPlace(_nodeLookupWalkedVisible);
        }

        if (found.PartIndex == -1)
        {
            IncrementInPlace(_nodeLookupNone);
            return;
        }

        if (found.PartIndex == walked.PartIndex)
        {
            return;
        }

        IncrementInPlace(_nodeLookupWrong);
        const bool foundVisible = _scene->IsNodeRefVisible(found);
        if (walkedVisible && !foundVisible)
        {
            IncrementInPlace(_nodeLookupHidden);
        }
        else if (!walkedVisible && foundVisible)
        {
            IncrementInPlace(_nodeLookupShown);
        }
    }

    void MapAudit::SamplePuppetNode(Entities::PlayerEntity& player)
    {
        const std::int32_t slot = player.SlotIndex();
        if (slot < 0 || slot >= static_cast<std::int32_t>(_puppetNode.size()))
        {
            return;
        }

        const std::size_t index = static_cast<std::size_t>(slot);
        const Vector3 position = player.Position;
        const Vector3 previous = _puppetPrev[index];
        _puppetPrev[index] = position;

        if (!_puppetSeeded[index])
        {
            _puppetSeeded[index] = true;
            _puppetNode[index] = Entities::PlayerEntity::ModSpawnNodeRef(*_scene, position);
            return;
        }

        Formats::Culling::NodeRef next = Entities::PlayerEntity::ModWalkNodeRef(
            *_scene, _puppetNode[index], previous, position);
        if (next == Formats::Culling::NodeRef::None)
        {
            next = _scene->GetNodeRefByPosition(position);
        }
        if (next != Formats::Culling::NodeRef::None)
        {
            _puppetNode[index] = next;
        }

        const Formats::Culling::NodeRef walked = player.NodeRef;
        if (walked.PartIndex == -1)
        {
            return;
        }

        IncrementInPlace(_puppetSamples);
        if (next.PartIndex == -1)
        {
            IncrementInPlace(_puppetNone);
            return;
        }

        if (next.PartIndex == walked.PartIndex)
        {
            return;
        }

        IncrementInPlace(_puppetWrong);
        if (_scene->IsNodeRefVisible(walked) && !_scene->IsNodeRefVisible(next))
        {
            IncrementInPlace(_puppetHidden);
        }
    }

    bool MapAudit::StepSpawnRender()
    {
        if (_spawnIndex < 0)
        {
            CollectSpawnSpots();
            _spawnIndex = 0;
            _spawnFrames = -1;
        }

        if (_spawnIndex >= static_cast<std::int32_t>(_spawnSpots.size()))
        {
            return false;
        }

        std::shared_ptr<Entities::PlayerEntity> player = Entities::PlayerEntity::Main();

        if (_spawnFrames < 0)
        {
            if (!TestFlag(player->LoadFlags(), LoadFlags::Spawned) || player->Health() == 0)
            {
                NetTestScript::Rest(player, true);
                return true;
            }

            const std::size_t index = static_cast<std::size_t>(_spawnIndex);
            const Vector3 spot = _spawnSpots[index];
            player->ModForceForm(false);
            player->Teleport(spot, _spawnFacings[index], _spawnNodeRefs[index]);
            _spawnFrames = 0;
            _spawnLitWorst = std::numeric_limits<double>::max();
            NetTestScript::Rest(player, true);
            return true;
        }

        ++_spawnFrames;

        if (_spawnFrames < _spawnSettleFrames)
        {
            NetTestScript::Rest(player, true);
            return true;
        }

        if (_spawnFrames == _spawnSettleFrames)
        {
            _spawnLitAtSpawn = Mods::ScreenCapture::NonBlackFraction(_scene.get());
            _spawnLitWorst = _spawnLitAtSpawn;
            SaveSpawnShot("spawn");
        }

        NetTestScript::WalkForward(player);

        if (_spawnFrames % 30 == 0)
        {
            const double lit = Mods::ScreenCapture::NonBlackFraction(_scene.get());
            if (lit < _spawnLitWorst)
            {
                _spawnLitWorst = lit;
            }
        }

        if (_spawnFrames < _spawnSettleFrames + _spawnWalkFrames)
        {
            return true;
        }

        SaveSpawnShot("walked");

        const Vector3 at = _spawnSpots[static_cast<std::size_t>(_spawnIndex)];
        const bool failed = _spawnLitWorst < _renderFloor;
        if (failed)
        {
            ++_spawnFailures;
        }

        std::shared_ptr<Entities::PlayerEntity> main = Entities::PlayerEntity::Main();
        std::cout
            << "RENDERSPAWN " << _room
            << " | spawn " << _spawnIndex
            << " at " << ::MphRead::NativeRuntime::ToString(at.X, "0.0") << ',' << ::MphRead::NativeRuntime::ToString(at.Y, "0.0") << ',' << ::MphRead::NativeRuntime::ToString(at.Z, "0.0")
            << " | at spawn " << ::MphRead::NativeRuntime::ToString(_spawnLitAtSpawn * 100.0, "0.0") << '%'
            << " | worst while walking " << ::MphRead::NativeRuntime::ToString(_spawnLitWorst * 100.0, "0.0") << '%'
            << " | part " << main->NodeRef.PartIndex
            << (failed ? " | FAIL" : "")
            << '\n';

        ++_spawnIndex;
        _spawnFrames = -1;
        return true;
    }

    bool MapAudit::StepItemShots()
    {
        if (_itemIndex < 0)
        {
            CollectItemSpots();
            _itemIndex = 0;
            _itemFrames = -1;
        }

        if (_itemIndex >= static_cast<std::int32_t>(_itemSpots.size()))
        {
            return false;
        }

        std::shared_ptr<Entities::PlayerEntity> player = Entities::PlayerEntity::Main();

        if (_itemFrames < 0)
        {
            if (!TestFlag(player->LoadFlags(), LoadFlags::Spawned) || player->Health() == 0)
            {
                NetTestScript::Rest(player, true);
                return true;
            }

            const Vector3 item = _itemSpots[static_cast<std::size_t>(_itemIndex)];
            const Vector3 stand = item + Vector3(0.0F, 0.4F, _itemStandOff);
            player->ModForceForm(false);
            player->Teleport(stand, Negate(Vector3(0.0F, 0.0F, 1.0F)), _scene->GetNodeRefByPosition(stand));
            _itemFrames = 0;
            NetTestScript::Rest(player, true);
            return true;
        }

        ++_itemFrames;
        NetTestScript::Rest(player, true);

        if (_itemFrames < _itemSettleFrames)
        {
            return true;
        }

        if (_shotDirectory.has_value())
        {
            const std::string name = ReplaceRoomCharacters(_room);
            Mods::ScreenCapture::Save(_scene.get(),
                PathCombine(
                    *_shotDirectory,
                    name + "-item" + TwoDigits(_itemIndex) + "-"
                        + _itemNames[static_cast<std::size_t>(_itemIndex)] + ".png"));
        }

        const Vector3 at = _itemSpots[static_cast<std::size_t>(_itemIndex)];
        std::cout
            << "ITEMSHOT " << _room
            << " | " << _itemIndex << ' '
            << _itemNames[static_cast<std::size_t>(_itemIndex)]
            << " at " << ::MphRead::NativeRuntime::ToString(at.X, "0.0") << ',' << ::MphRead::NativeRuntime::ToString(at.Y, "0.0") << ',' << ::MphRead::NativeRuntime::ToString(at.Z, "0.0")
            << '\n';

        ++_itemIndex;
        _itemFrames = -1;
        return true;
    }

    void MapAudit::CollectItemSpots()
    {
        auto enumerator = _scene->Entities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            std::shared_ptr<Entities::EntityBase> entity = enumerator.Current();
            std::string what;
            Vector3 spot;

            if (entity->Type == EntityType::ItemSpawn)
            {
                auto spawn = std::dynamic_pointer_cast<Entities::ItemSpawnEntity>(entity);
                what = ::MphRead::ToString(spawn->Data().ItemType);
                spot = AddY(static_cast<Vector3>(entity->Position), 0.65F);
            }
            else if (entity->Type == EntityType::ItemInstance)
            {
                auto item = std::dynamic_pointer_cast<Entities::ItemInstanceEntity>(entity);
                what = ::MphRead::ToString(item->ItemType());
                spot = entity->Position;
            }
            else
            {
                continue;
            }

            _itemSpots.push_back(spot);
            _itemNames.push_back(std::move(what));
        }
    }

    void MapAudit::SaveSpawnShot(std::string_view what)
    {
        if (!_shotDirectory.has_value())
        {
            return;
        }

        const std::string name = ReplaceRoomCharacters(_room);
        Mods::ScreenCapture::Save(_scene.get(),
            PathCombine(
                *_shotDirectory,
                name + "-spawn" + TwoDigits(_spawnIndex) + "-" + std::string(what) + ".png"));
    }

    void MapAudit::CollectSpawnSpots()
    {
        auto enumerator = _scene->Entities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            std::shared_ptr<Entities::EntityBase> entity = enumerator.Current();
            if (entity->Type != EntityType::PlayerSpawn)
            {
                continue;
            }

            _spawnSpots.push_back(AddY(static_cast<Vector3>(entity->Position), 0.5F));
            const Vector3 facing = entity->FacingVector();
            _spawnFacings.push_back(
                LengthSquared(facing) < 0.0001F ? Vector3(0.0F, 0.0F, 1.0F) : facing);
            _spawnNodeRefs.push_back(entity->NodeRef);
        }
    }

    void MapAudit::SampleRender()
    {
        if (_playMovies && _scene->MoviePlaying() && _shotDirectory.has_value()
            && _frame % 4 == 0)
        {
            // The room sampler runs once per second; save each 15 Hz movie image instead.
            const std::string name = ReplaceRoomCharacters(_room);
            const std::string path = PathCombine(
                *_shotDirectory,
                name + "-movie-" + TwoDigits(_movieShotsSaved) + ".png");
            // The movie is composited to the window after the scene target.
            // Read that presented frame so the capture contains the movie.
            if (Mods::ScreenCapture::SaveWindow(_scene.get(), path))
            {
                IncrementInPlace(_movieShotsSaved);
            }
            return;
        }
        if (_frame % _litSampleFrames != 1)
        {
            return;
        }

        const double lit = Mods::ScreenCapture::NonBlackFraction(_scene.get());
        IncrementInPlace(_litSamples);
        _litTotal += lit;

        if (_litFirst < 0.0)
        {
            _litFirst = lit;
        }
        if (lit < _litMin)
        {
            _litMin = lit;
        }
        if (lit > _litMax)
        {
            _litMax = lit;
        }

        if (_shotDirectory.has_value())
        {
            const std::string name = ReplaceRoomCharacters(_room);
            const std::string path = PathCombine(
                *_shotDirectory,
                name + "-" + TwoDigits(_shotsSaved) + ".png");
            const bool saved = _showWindow
                ? Mods::ScreenCapture::SaveWindow(_scene.get(), path)
                : Mods::ScreenCapture::Save(_scene.get(), path);
            if (saved)
            {
                IncrementInPlace(_shotsSaved);
            }
        }
    }

    void MapAudit::OnClosing()
    {
        _scene->DoCleanup();
        if (_movieAudioCallProbe)
        {
            _movieAudioCallProbePassed = _scene->ModMovieAudioCallProbeCompleted();
            if (_movieAudioCallProbePassed)
            {
                ::MphRead::Mods::DebugLog::Line("movie",
                    "map-audit movie_openal_call_probe_pass room=" + _room
                    + " operation=SourceQueueBuffers");
            }
            else
            {
                ::MphRead::Mods::DebugLog::Line("movie",
                    "map-audit movie_openal_call_probe_fail room=" + _room
                    + " operation=SourceQueueBuffers");
            }
        }
        _window->BaseOnClosing();
    }

    std::int32_t MapAudit::Report()
    {
        std::int32_t spawnPoints = 0;
        std::int32_t jumpPads = 0;
        std::int32_t teleporters = 0;
        std::int32_t doors = 0;
        std::int32_t forceFields = 0;
        std::int32_t itemSpawns = 0;
        std::int32_t items = 0;
        std::int32_t platforms = 0;
        std::int32_t morphCameras = 0;
        std::int32_t flagBases = 0;
        std::int32_t nodeDefenses = 0;
        std::int32_t artifacts = 0;
        std::int32_t triggers = 0;
        std::int32_t areaVolumes = 0;

        {
            auto enumerator = _scene->Entities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                const std::shared_ptr<Entities::EntityBase> entity = enumerator.Current();
                switch (entity->Type)
                {
                    case EntityType::PlayerSpawn:
                        ++spawnPoints;
                        break;
                    case EntityType::JumpPad:
                        ++jumpPads;
                        break;
                    case EntityType::Teleporter:
                        ++teleporters;
                        break;
                    case EntityType::Door:
                        ++doors;
                        break;
                    case EntityType::ForceField:
                        ++forceFields;
                        break;
                    case EntityType::ItemSpawn:
                        ++itemSpawns;
                        break;
                    case EntityType::ItemInstance:
                        ++items;
                        break;
                    case EntityType::Platform:
                        ++platforms;
                        break;
                    case EntityType::MorphCamera:
                        ++morphCameras;
                        break;
                    case EntityType::FlagBase:
                        ++flagBases;
                        break;
                    case EntityType::NodeDefense:
                        ++nodeDefenses;
                        break;
                    case EntityType::Artifact:
                        ++artifacts;
                        break;
                    case EntityType::TriggerVolume:
                        ++triggers;
                        break;
                    case EntityType::AreaVolume:
                        ++areaVolumes;
                        break;
                    default:
                        break;
                }
            }
        }

        std::int32_t spawnedEver = 0;
        std::int32_t altEver = 0;
        std::int32_t firedEver = 0;
        std::int32_t totalDeaths = 0;
        std::int32_t frozenEver = 0;
        std::int32_t burnedEver = 0;
        std::int32_t disruptedEver = 0;

        for (std::int32_t i = 0; i < _players; ++i)
        {
            const std::size_t index = static_cast<std::size_t>(i);
            if (_everSpawned[index])
            {
                ++spawnedEver;
            }
            if (_everAltForm[index])
            {
                ++altEver;
            }
            if (_everFired[index])
            {
                ++firedEver;
            }
            if (_everFrozen[index])
            {
                ++frozenEver;
            }
            if (_everBurned[index])
            {
                ++burnedEver;
            }
            if (_everDisrupted[index])
            {
                ++disruptedEver;
            }
            totalDeaths = UncheckedAdd(totalDeaths, _deaths[index]);
        }

        std::ostringstream line;
        line << "MAPTEST " << _room
             << " | players " << _players
             << " | frames " << _frame
             << " | spawned " << spawnedEver << '/' << _players;

        std::int32_t moved = 0;
        float furthest = 0.0F;
        for (std::int32_t i = 0;
            i < _players && i < static_cast<std::int32_t>(_travelled.size());
            ++i)
        {
            const float travelled = _travelled[static_cast<std::size_t>(i)];
            if (travelled > 20.0F)
            {
                ++moved;
            }
            furthest = MathMax(furthest, travelled);
        }

        line << " | moved " << moved << '/' << _players
             << " (furthest " << ::MphRead::NativeRuntime::ToString(furthest, "0") << " units)"
             << " | alt form " << altEver << '/' << _players
             << " | fired " << firedEver << '/' << _players
             << " | deaths " << totalDeaths
             << " | afflicted freeze " << frozenEver
             << " burn " << burnedEver
             << " disrupt " << disruptedEver;

        std::ostringstream probe;
        for (std::int32_t i = 0;
            i < static_cast<std::int32_t>(_afflictions.size());
            ++i)
        {
            const std::size_t index = static_cast<std::size_t>(i);
            probe << (i == 0 ? " (probe " : " ");
            probe << _afflictions[index].second << ' ';
            probe << (!_afflictTried[index] ? "n/a"
                : _afflictLanded[index] ? "ok"
                : !_afflictFired[index] ? "nofire"
                : _afflictHit[index] ? "FAIL"
                : "nohit");
        }
        probe << ')';
        line << probe.str();

        line << " | spawnpoints " << spawnPoints
             << " | jumppads " << jumpPads
             << " (" << _padsFired << '/' << _padsProbed << " launched)"
             << " teleporters " << teleporters
             << " (" << _telesFired << '/' << _telesProbed << " moved)"
             << " doors " << doors
             << " forcefields " << forceFields
             << " platforms " << platforms
             << " itemspawns " << itemSpawns
             << " items " << items
             << " morphcams " << morphCameras
             << " flagbases " << flagBases
             << " nodes " << nodeDefenses
             << " artifacts " << artifacts
             << " triggers " << triggers
             << " areas " << areaVolumes
             << " | lowest Y " << ::MphRead::NativeRuntime::ToString(_lowestY, "0.0");

        if (_nodeLookupSamples > 0)
        {
            const std::int32_t agreed
                = UncheckedSubtract(
                    UncheckedSubtract(_nodeLookupSamples, _nodeLookupWrong),
                    _nodeLookupNone);
            line << " | node lookup " << agreed << '/' << _nodeLookupSamples
                 << " agreed (" << _nodeLookupNone << " none, "
                 << _nodeLookupWrong << " wrong part, "
                 << _nodeLookupHidden << " would hide the player, "
                 << _nodeLookupShown << " would reveal one; "
                 << _nodeLookupWalkedVisible << " samples drawable)";
        }

        if (_puppetSamples > 0)
        {
            const std::int32_t agreed = UncheckedSubtract(
                UncheckedSubtract(_puppetSamples, _puppetWrong), _puppetNone);
            line << " | remote node " << agreed << '/' << _puppetSamples
                 << " agreed (" << _puppetNone << " none, "
                 << _puppetWrong << " wrong part, "
                 << _puppetHidden << " would hide the player)";
        }

        line << " | effect particles " << _scene->ModEffectParticles();

        if (_litSamples > 0)
        {
            line << " | lit first " << ::MphRead::NativeRuntime::ToString(_litFirst * 100.0, "0.0") << '%'
                 << " min " << ::MphRead::NativeRuntime::ToString(_litMin * 100.0, "0.0") << '%'
                 << " max " << ::MphRead::NativeRuntime::ToString(_litMax * 100.0, "0.0") << '%'
                 << " mean " << ::MphRead::NativeRuntime::ToString(_litTotal / _litSamples * 100.0, "0.0") << '%'
                 << " (" << _litSamples << " samples)";
        }

        std::cout << line.str() << '\n';

        if (_drawRate > 1)
        {
            std::cout
                << "FRAMETIMING " << _room
                << " | " << _drawRate << " draws per step"
                << " | " << _frame << " steps, " << _scene->FrameCount() << " counted"
                << " | draws advancing the game: " << _drawAdvancedTheGame
                << " | active Lockjaw trail checks: " << _lockjawTrailChecks
                << " | trail mismatches: " << _lockjawTrailMismatches
                << " | Lockjaw GetDrawInfo RNG changes: " << _lockjawDrawRngChanges
                << '\n';
            if (_lockjawTrailMismatches > 0)
            {
                std::cout << "MAPFAIL " << _room << " | Lockjaw trail geometry changed between"
                    << " draws of the same simulation step (" << _lockjawTrailMismatches << " mismatches)\n";
            }
            if (_lockjawDrawRngChanges > 0)
            {
                std::cout << "MAPFAIL " << _room << " | Lockjaw GetDrawInfo changed"
                    << " global RNG during " << _lockjawDrawRngChanges << " draw(s)\n";
            }
        }
        const std::int32_t lockjawFailures = (_lockjawTrailMismatches > 0 ? 1 : 0)
            + (_lockjawDrawRngChanges > 0 ? 1 : 0);

        if (_itemProbe)
        {
            std::cout
                << "ITEMSWEEP " << _room
                << " | " << _itemSpots.size() << " pickup(s) photographed"
                << '\n';
            return lockjawFailures;
        }

        if (_itemPickupAudit != nullptr)
        {
            const std::int32_t attempted = _itemPickupAudit->TargetCount();
            std::cout << "ITEMPICKUPSWEEP " << _room
                << " | " << attempted << " pickup(s) tested"
                << " | " << _itemPickupAudit->FailureCount() << " failed\n";
            return UncheckedAdd(_itemPickupAudit->FailureCount(), lockjawFailures);
        }

        if (_renderProbe)
        {
            std::cout
                << "RENDERSWEEP " << _room
                << " | " << _spawnSpots.size() << " spawn point(s) "
                << "| " << _spawnFailures << " drew nothing"
                << '\n';

            if (_spawnFailures > 0)
            {
                std::cout
                    << "MAPFAIL " << _room
                    << " | " << _spawnFailures << " of " << _spawnSpots.size()
                    << " spawn point(s) end in a frame with no room in it"
                    << '\n';
            }
            return UncheckedAdd(_spawnFailures, lockjawFailures);
        }

        std::vector<std::string> problems;

        if (_padsProbed > 0 && _padsFired == 0)
        {
            problems.push_back(
                "none of the " + std::to_string(_padsProbed)
                + " jump pad(s) launched a player standing on them");
        }

        if (_telesProbed > 0 && _telesFired == 0)
        {
            problems.push_back(
                "none of the " + std::to_string(_telesProbed)
                + " teleporter(s) moved a player standing on them");
        }

        if (spawnedEver < _players)
        {
            std::ostringstream missing;
            for (std::int32_t i = 0; i < _players; ++i)
            {
                const std::size_t index = static_cast<std::size_t>(i);
                if (!_everSpawned[index])
                {
                    if (missing.tellp() > 0)
                    {
                        missing << ", ";
                    }
                    const std::shared_ptr<Entities::PlayerEntity> player
                        = Entities::PlayerEntity::Players()[index];
                    missing
                        << "slot " << i << " (" << ::MphRead::ToString(player->Hunter())
                        << ", hp " << player->Health()
                        << ", respawn " << player->RespawnTimer() << ')';
                }
            }
            problems.push_back(
                "only " + std::to_string(spawnedEver) + " of "
                + std::to_string(_players)
                + " players ever reached the map -- missing " + missing.str());
        }

        if (spawnPoints == 0)
        {
            problems.emplace_back("no spawn points at all");
        }

        if (_litSamples > 0 && _litMax < _renderFloor)
        {
            problems.push_back(
                "the room never drew: at most " + ::MphRead::NativeRuntime::ToString(_litMax * 100.0, "0.0")
                + "% of the frame was lit across " + std::to_string(_litSamples)
                + " samples (first " + ::MphRead::NativeRuntime::ToString(_litFirst * 100.0, "0.0") + "%)");
        }
        else if (_litSamples > 1 && _litMin < _renderFloor)
        {
            problems.push_back(
                "the room stopped drawing: " + ::MphRead::NativeRuntime::ToString(_litMin * 100.0, "0.0")
                + "% of the frame lit at its worst against "
                + ::MphRead::NativeRuntime::ToString(_litMax * 100.0, "0.0") + "% at its best");
        }

        for (std::int32_t i = 0; i < _players; ++i)
        {
            const std::size_t index = static_cast<std::size_t>(i);
            std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players()[index];
            if (_everSpawned[index] && !player->ModCanBeHurt())
            {
                problems.push_back(
                    "slot " + std::to_string(i) + " (" + ::MphRead::ToString(player->Hunter())
                    + ") cannot be hurt by any beam");
            }
        }

        if (_scene->ModEffectParticles() == 0)
        {
            problems.emplace_back(
                "no effect particle was spawned all run: "
                "muzzle flashes, impacts and explosions are emitting nothing");
        }

        if (_scoreboardFrames == 0)
        {
            problems.emplace_back(
                "the scoreboard was never drawn: the check above it did not run");
        }

        if (_drawAdvancedTheGame > 0)
        {
            problems.push_back(
                "drawing advanced the simulation on "
                + std::to_string(_drawAdvancedTheGame)
                + " frame(s): a draw pass is writing back to the world");
        }

        for (const std::string& problem : problems)
        {
            std::cout << "MAPFAIL " << _room << " | " << problem << '\n';
        }

        return UncheckedAdd(static_cast<std::int32_t>(problems.size()), lockjawFailures);
    }

    std::int32_t MapAudit::Run(
        std::string room,
        std::int32_t players,
        double seconds,
        GameMode mode,
        bool bots,
        std::optional<std::string> shotDirectory,
        bool renderProbe,
        bool allNodes,
        bool itemProbe,
        bool itemPickupProbe,
        bool playMovies,
        bool movieChainProbe,
        bool movieSkipProbe,
        bool movieCloseProbe,
        bool cleanupExceptionProbe,
        bool partialInitializationProbe,
        bool movieAudioCallProbe,
        bool forceMovieForAudioCallProbe,
        bool movieRoomTransitionProbe)
    {
        std::unique_ptr<MapAudit> window;
        std::int32_t result = 1;
        Entities::BombEntity::ModLockjawDrawRngChanges = 0;
        Entities::BombEntity::ModAuditLockjawDrawRng = DrawRate() > 1;

        try
        {
            window = std::unique_ptr<MapAudit>(new MapAudit(
                room,
                std::clamp<std::int32_t>(players, 1, Entities::PlayerEntity::SlotCapacity),
                seconds,
                mode,
                bots,
                renderProbe,
                itemProbe,
                itemPickupProbe,
                playMovies,
                movieChainProbe,
                movieSkipProbe,
                movieCloseProbe,
                cleanupExceptionProbe,
                partialInitializationProbe,
                movieAudioCallProbe,
                forceMovieForAudioCallProbe,
                movieRoomTransitionProbe));

            window->_scene->ShowAllNodes(allNodes);

            if (shotDirectory.has_value())
            {
                MphRead::NativeRuntime::DirectoryCreateDirectory(*shotDirectory);
                window->_shotDirectory = *shotDirectory;
            }

            window->Run();
            if (window->_movieRoomTransitionProbe)
            {
                if (window->_movieRoomTransitionPassed)
                {
                    result = 0;
                }
                else
                {
                    std::cout << "MAPMOVIETRANSITION FAIL rooms=UNIT4_LAND,UNIT4_RM1,UNIT4_RM2\n";
                    result = 1;
                }
            }
            else if (window->_movieAudioCallProbe)
            {
                if (window->_movieAudioCallProbePassed)
                {
                    result = 0;
                }
                else
                {
                    std::cout << "MAPMOVIEAL FAIL " << room
                        << " | the OpenAL SourceQueueBuffers gate was not reached\n";
                    result = 1;
                }
            }
            else
            {
                result = window->Report();
            }
        }
        catch (const std::exception& ex)
        {
            Mods::DebugLog::Line("map-audit", "run-exception room=" + room + " error=" + ex.what());
            std::cout
                << "MAPCRASH " << room
                << " | " << ExceptionTypeName(ex) << ": " << ex.what()
                << '\n';
            std::cout << '\n';
            result = 1;
        }
        catch (...)
        {
            Mods::DebugLog::Line("map-audit", "run-exception room=" + room + " error=unknown");
            std::cout << "MAPCRASH " << room << " | Exception: unknown exception\n";
            std::cout << '\n';
            result = 1;
        }

        Entities::BombEntity::ModAuditLockjawDrawRng = false;
        if (window)
        {
            try
            {
                window->Dispose();
            }
            catch (const std::exception& ex)
            {
                Mods::DebugLog::Exception("map-audit-cleanup", std::current_exception());
                std::cout << "MAPCLEANUP " << room << " | " << ExceptionTypeName(ex)
                    << ": " << ex.what() << '\n';
                if (result == 0)
                {
                    result = 1;
                }
            }
            catch (...)
            {
                Mods::DebugLog::Exception("map-audit-cleanup", std::current_exception());
                std::cout << "MAPCLEANUP " << room << " | Exception: unknown cleanup failure\n";
                if (result == 0)
                {
                    result = 1;
                }
            }
        }
        return result;
    }

    std::int32_t MapAudit::RunStoryMovieCycle()
    {
        constexpr std::array<std::string_view, 3> rooms{
            "UNIT4_LAND", "UNIT4_RM1", "UNIT4_RM2"};
        for (const std::string_view room : rooms)
        {
            Mods::DebugLog::Line("movie", "map-audit story-movie-cycle-start room=" + std::string(room));
            const std::int32_t result = Run(std::string(room), 1, 5.0, GameMode::SinglePlayer,
                false, std::nullopt, false, false, false, false, true,
                false, false, false, false, false, true, true);
            if (result != 0)
            {
                Mods::DebugLog::Line("movie", "map-audit story-movie-cycle-failed room="
                    + std::string(room) + " result=" + std::to_string(result));
                return result;
            }
        }
        Mods::DebugLog::Line("movie", "map-audit story-movie-cycle-pass rooms=UNIT4_LAND,UNIT4_RM1,UNIT4_RM2");
        std::cout << "MAPMOVIECYCLE PASS rooms=UNIT4_LAND,UNIT4_RM1,UNIT4_RM2\n";
        return 0;
    }

    std::int32_t MapAudit::RunStoryMovieTransitionCycle()
    {
        ShowWindow(true);
        DrawRate(1);
        MainHunter(std::nullopt);
        const std::string shots = "manual-story-movie-transition-20261015-final";
        return Run("UNIT4_LAND", 1, 20.0, GameMode::SinglePlayer,
            false, shots, false, false, false, false, true,
            false, false, false, false, false, false, false, true);
    }
}
