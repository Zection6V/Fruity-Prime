#include "LauncherHunter.hpp"

#include "../DebugLog.hpp"
#include "../../Scene.hpp"
#include "../../Renderer.hpp"
#include "../../NativeRuntime/System/ExceptionText.hpp"
#include "../../NativeRuntime/System/Number.hpp"

#include <algorithm>
#include <exception>
#include <string>
#include <utility>

namespace MphRead::Mods::Render
{
    bool LauncherHunter::_wanted = false;
    ::MphRead::Hunter LauncherHunter::_hunter = ::MphRead::Hunter::Samus;
    std::int32_t LauncherHunter::_suit = 0;
    float LauncherHunter::_left = 0;
    float LauncherHunter::_top = 0;
    float LauncherHunter::_right = 0;
    float LauncherHunter::_bottom = 0;
    bool LauncherHunter::_drawn = false;
    bool LauncherHunter::_failed = false;
    bool LauncherHunter::_said = false;
    bool LauncherHunter::_glStale = false;
    std::shared_ptr<::MphRead::Scene> LauncherHunter::_scene;

    bool LauncherHunter::Wanted() noexcept
    {
        return _wanted;
    }

    void LauncherHunter::Wanted(bool value) noexcept
    {
        _wanted = value;
    }

    ::MphRead::Hunter LauncherHunter::Hunter() noexcept
    {
        return _hunter;
    }

    void LauncherHunter::Hunter(::MphRead::Hunter value) noexcept
    {
        _hunter = value;
    }

    std::int32_t LauncherHunter::Suit() noexcept
    {
        return _suit;
    }

    void LauncherHunter::Suit(std::int32_t value) noexcept
    {
        _suit = value;
    }

    float LauncherHunter::Left() noexcept
    {
        return _left;
    }

    void LauncherHunter::Left(float value) noexcept
    {
        _left = value;
    }

    float LauncherHunter::Top() noexcept
    {
        return _top;
    }

    void LauncherHunter::Top(float value) noexcept
    {
        _top = value;
    }

    float LauncherHunter::Right() noexcept
    {
        return _right;
    }

    void LauncherHunter::Right(float value) noexcept
    {
        _right = value;
    }

    float LauncherHunter::Bottom() noexcept
    {
        return _bottom;
    }

    void LauncherHunter::Bottom(float value) noexcept
    {
        _bottom = value;
    }

    bool LauncherHunter::Drawn() noexcept
    {
        return _drawn;
    }

    // Every scene numbers its own textures from one (no glGenTextures), so a
    // match loaded after the side scene wrote over its texture names -- the
    // toon table and the hunter's skin included -- and its UnloadGl deleted
    // them: a black silhouette. The side scene is rebuilt after a match.
    void LauncherHunter::NoteGlUnloaded() noexcept
    {
        _glStale = true;
    }

    void LauncherHunter::Reset()
    {
        _wanted = false;
        _drawn = false;
        ::MphRead::Scene::LauncherPreview = false;
    }

    void LauncherHunter::ReleaseGl() noexcept
    {
        std::shared_ptr<::MphRead::Scene> scene = std::move(_scene);
        _glStale = false;
        if (scene == nullptr)
        {
            return;
        }

        try
        {
            scene->UnloadGl();
        }
        catch (...)
        {
            // Destruction below is the final fallback. It must still happen
            // before RenderWindow destroys the owning desktop GL context.
        }

        scene.reset();
    }

    void LauncherHunter::Draw(::MphRead::RenderWindow& window, std::int32_t width, std::int32_t height)
    {
        _drawn = false;
        if (_failed || !_wanted || width <= 0 || height <= 0)
        {
            ::MphRead::Scene::LauncherPreview = false;
            return;
        }
        if (_right - _left <= 0.001F || _bottom - _top <= 0.001F)
        {
            ::MphRead::Scene::LauncherPreview = false;
            return;
        }

        try
        {
            if (_glStale && !window.HasScene())
            {
                _glStale = false;
                // Its render targets are its own and would be left behind.
                if (_scene)
                {
                    _scene->UnloadGl();
                }
                _scene.reset();
            }
            ::MphRead::Scene* scene = window.HasScene() ? &window.Scene() : _scene.get();
            if (scene == nullptr)
            {
                _scene = window.NewSideScene();
                _scene->OnLoad();
                _scene->OnResize();
                scene = _scene.get();
            }
            ::MphRead::Scene::LauncherPreview = true;
            ::MphRead::Scene::LauncherHunter = _hunter;
            ::MphRead::Scene::LauncherSuit = std::clamp(_suit, 0, 3);
            ::MphRead::Scene::PreviewWanted(true);
            ::MphRead::Scene::PreviewLeft(_left);
            ::MphRead::Scene::PreviewTop(_top);
            ::MphRead::Scene::PreviewRight(_right);
            ::MphRead::Scene::PreviewBottom(_bottom);
            _drawn = scene->ModDrawPreviewAlone(::OpenTK::Mathematics::Vector2i(width, height));
            if (_drawn && !_said)
            {
                _said = true;
                DebugLog::Line("ui", "the hunter preview is on, at ("
                    + ::MphRead::NativeRuntime::ToString(_left, "0.###") + ","
                    + ::MphRead::NativeRuntime::ToString(_top, "0.###") + ")-("
                    + ::MphRead::NativeRuntime::ToString(_right, "0.###") + ","
                    + ::MphRead::NativeRuntime::ToString(_bottom, "0.###") + ") of "
                    + ::MphRead::NativeRuntime::ToString(width) + "x"
                    + ::MphRead::NativeRuntime::ToString(height));
            }
        }
        catch (...)
        {
            _failed = true;
            _drawn = false;
            ::MphRead::Scene::LauncherPreview = false;
            DebugLog::Line("ui", "the hunter preview could not be set up: "
                + ::MphRead::NativeRuntime::ExceptionMessage(std::current_exception()));
        }
    }
}
