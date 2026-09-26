#include "PreviewPass.hpp"

#include "../DebugLog.hpp"
#include "../../Scene.hpp"
#include "../../Shaders.hpp"

#include "../EndScreen.hpp"
#include "HunterPreview.hpp"

#include <cmath>
#include "../../NativeRuntime/OpenTK/GL.hpp"
#include "../../NativeRuntime/System/Console.hpp"

#include <cstdint>
#include <exception>
#include <limits>
#include <string>
#include <memory>
#include "../../NativeRuntime/System/IO.hpp"
#include "../../Formats/Types.hpp"
#include "../../NativeRuntime/System/ExceptionText.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

using ::MphRead::NativeRuntime::ConvertToInt32Net9;
using ::MphRead::NativeRuntime::ExceptionMessage;
using ::MphRead::NativeRuntime::RoundToEven;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;

namespace
{
    namespace GL = ::OpenTK::Graphics::OpenGL::GL;

}

namespace
{
    using ::OpenTK::Mathematics::Matrix4;
    using ::OpenTK::Mathematics::Vector3;
    using ::OpenTK::Mathematics::Vector4;

    constexpr float Pi = 3.14159265358979323846F;

    constexpr std::int32_t ScissorTest = 0x0C11;
    constexpr std::int32_t DepthTest = 0x0B71;
    constexpr std::int32_t StencilTest = 0x0B90;
    constexpr std::int32_t Blend = 0x0BE2;
    constexpr std::int32_t AlphaTest = 0x0BC0;
    constexpr std::uint32_t ColorBufferBit = 0x00004000U;
    constexpr std::uint32_t DepthBufferBit = 0x00000100U;
    constexpr std::int32_t Less = 0x0201;
    constexpr std::int32_t SrcAlpha = 0x0302;
    constexpr std::int32_t OneMinusSrcAlpha = 0x0303;
    constexpr std::int32_t FrontAndBack = 0x0408;
    constexpr std::int32_t Fill = 0x1B02;

    [[nodiscard]] std::int32_t RoundPixel(float value) noexcept
    {
        return ConvertToInt32Net9(RoundToEven(value));
    }

    class PreviewCollectFinally final
    {
    public:
        explicit PreviewCollectFinally(bool& collecting) noexcept
            : _collecting(collecting)
        {
        }

        PreviewCollectFinally(const PreviewCollectFinally&) = delete;
        PreviewCollectFinally& operator=(const PreviewCollectFinally&) = delete;

        ~PreviewCollectFinally()
        {
            _collecting = false;
        }

    private:
        bool& _collecting;
    };
}

namespace MphRead
{
    float Scene::_previewLeft = 0.0F;
    float Scene::_previewTop = 0.0F;
    float Scene::_previewRight = 0.0F;
    float Scene::_previewBottom = 0.0F;
    bool Scene::_previewWanted = false;

    const OpenTK::Mathematics::Vector3 Scene::_previewEye(0.0F, 1.05F, 3.15F);
    const OpenTK::Mathematics::Vector3 Scene::_previewTarget(0.0F, 0.95F, 0.0F);
    const OpenTK::Mathematics::Vector4 Scene::_previewBack(0.05F, 0.055F, 0.07F, 1.0F);

    float Scene::PreviewLeft() noexcept
    {
        return _previewLeft;
    }

    void Scene::PreviewLeft(float value) noexcept
    {
        _previewLeft = value;
    }

    float Scene::PreviewTop() noexcept
    {
        return _previewTop;
    }

    void Scene::PreviewTop(float value) noexcept
    {
        _previewTop = value;
    }

    float Scene::PreviewRight() noexcept
    {
        return _previewRight;
    }

    void Scene::PreviewRight(float value) noexcept
    {
        _previewRight = value;
    }

    float Scene::PreviewBottom() noexcept
    {
        return _previewBottom;
    }

    void Scene::PreviewBottom(float value) noexcept
    {
        _previewBottom = value;
    }

    bool Scene::PreviewWanted() noexcept
    {
        return _previewWanted;
    }

    void Scene::PreviewWanted(bool value) noexcept
    {
        _previewWanted = value;
    }

    bool Scene::PreviewAsked()
    {
        return Mods::EndScreen::Available() || LauncherPreview;
    }

    void Scene::ModStepPreview()
    {
        if (!PreviewAsked())
        {
            if (_preview)
            {
                _preview->Reset();
            }
            _previewWanted = false;
            _previewLeft = _previewRight = _previewTop = _previewBottom = 0.0F;
            return;
        }
        if (!_preview)
        {
            _preview = std::make_shared<Mods::Render::HunterPreviewEntity>(this);
        }
        const std::shared_ptr<Mods::Render::HunterPreviewEntity> preview = _preview;
        const MphRead::Hunter want = LauncherPreview ? LauncherHunter : Mods::EndScreen::Hunter();
        const std::int32_t suit = LauncherPreview ? LauncherSuit : Mods::EndScreen::Suit();
        preview->SetUp(want, suit);
        // Textures and display lists, which nobody else is going to make on
        // the launcher: there is no player standing in a room to have made them.
        if (_previewInited != want)
        {
            _previewInited = want;
            if (_preview->Ready())
            {
                InitEntity(_preview);
            }
        }
        _preview->Step();
    }

    void Scene::ModCollectPreview()
    {
        _previewItems.clear();
        if (!PreviewAsked() || !_preview || !_preview->Ready())
        {
            return;
        }
        _collectingPreview = true;
        PreviewCollectFinally finally(_collectingPreview);
        try
        {
            _preview->GetDrawInfo();
        }
        catch (...)
        {
            const std::exception_ptr exception = std::current_exception();
            NativeRuntime::ConsoleWriteLine(
                "[endscreen] preview draw failed: "
                + ExceptionMessage(exception));
            _previewItems.clear();
        }
    }

    bool Scene::ModPreviewDrawn() const noexcept
    {
        return !_previewItems.empty()
            && _previewRight - _previewLeft > 0.001F
            && _previewBottom - _previewTop > 0.001F;
    }

    // The same hunter, in a frame with no match behind it: the launcher's own
    // screens. Straight into the back buffer. Returns whether anything was
    // drawn, which the screens read before leaving a hole for it.
    bool Scene::ModDrawPreviewAlone(OpenTK::Mathematics::Vector2i windowSize)
    {
        if (!LauncherPreview || windowSize.X <= 0 || windowSize.Y <= 0)
        {
            return false;
        }
        _targetSize = windowSize;
        try
        {
            ModStepPreview();
            ModCollectPreview();
            if (!ModPreviewDrawn())
            {
                return false;
            }
            GL::BindFramebuffer(GL::FramebufferTarget::Framebuffer, 0);
            GL::UseProgram(_shaderProgramId);
            ModDrawPreview();
            GL::UseProgram(0);
            return true;
        }
        catch (const std::exception& ex)
        {
            // A preview that will not draw is the launcher's boxes again, not
            // a dead launcher. Said once: this is a per-frame path.
            if (!_previewComplained)
            {
                _previewComplained = true;
                Mods::DebugLog::Line("ui", std::string("the hunter preview could not be drawn: ") + ex.what());
            }
            LauncherPreview = false;
            return false;
        }
    }

    void Scene::ModDrawPreview()
    {
        if (_previewItems.empty() || !_previewWanted)
        {
            _previewDrawnLastFrame = false;
            return;
        }
        // Not from inside the world's render while the deck panel is up: the
        // panel is opaque, so it would be a hunter behind a card.
        if (Mods::EndScreen::PanelUp() && !LauncherPreview)
        {
            return;
        }
        const OpenTK::Mathematics::Vector2i target = _targetSize;
        const std::int32_t x = RoundPixel(_previewLeft * static_cast<float>(target.X));
        const std::int32_t y = RoundPixel((1.0F - _previewBottom) * static_cast<float>(target.Y));
        const std::int32_t width = RoundPixel(
            (_previewRight - _previewLeft) * static_cast<float>(target.X));
        const std::int32_t height = RoundPixel(
            (_previewBottom - _previewTop) * static_cast<float>(target.Y));
        if (width < 4 || height < 4)
        {
            return;
        }

        GL::Enable(GL::EnableCap::ScissorTest);
        GL::Scissor(x, y, width, height);
        GL::ClearColor(_previewBack.X, _previewBack.Y, _previewBack.Z, _previewBack.W);
        GL::Clear(GL::ClearBufferMask::ColorBufferBit | GL::ClearBufferMask::DepthBufferBit);
        GL::ClearColor(0.0F, 0.0F, 0.0F, 0.0F);
        GL::Viewport(x, y, width, height);
        Matrix4 projection = Matrix4::CreatePerspectiveFieldOfView(
            DegreesToRadians(PreviewFov), width / static_cast<float>(height), 0.1F, 100.0F);
        Matrix4 view = Matrix4::LookAt(_previewEye, _previewTarget, Vector3(0.0F, 1.0F, 0.0F));
        if (!_shaderLocations)
        {
            throw System::NullReferenceException();
        }
        GL::UniformMatrix4(_shaderLocations->ProjectionMatrix, false, projection);
        GL::UniformMatrix4(_shaderLocations->ViewMatrix, false, view);
        GL::Uniform1(_shaderLocations->UseFog, 0);
        GL::Enable(GL::EnableCap::DepthTest);
        GL::DepthFunc(GL::DepthFunction::Less);
        GL::DepthMask(true);
        GL::Disable(GL::EnableCap::StencilTest);
        GL::Enable(GL::EnableCap::Blend);
        GL::BlendFunc(GL::BlendingFactor::SrcAlpha, GL::BlendingFactor::OneMinusSrcAlpha);
        GL::Disable(GL::EnableCap::AlphaTest);
        for (std::size_t i = 0; i < _previewItems.size(); ++i)
        {
            RenderItem(_previewItems[i]);
        }
        GL::Disable(GL::EnableCap::ScissorTest);
        GL::Viewport(0, 0, target.X, target.Y);
        GL::UniformMatrix4(_shaderLocations->ProjectionMatrix, false, _perspectiveMatrix);
        GL::UniformMatrix4(_shaderLocations->ViewMatrix, false, _viewMatrix);
        GL::Uniform1(_shaderLocations->UseFog, _hasFog && FogOn() ? 1 : 0);
        GL::PolygonMode(GL::TriangleFace::FrontAndBack, GL::PolygonMode::Fill);
        _previewDrawnLastFrame = true;
        _previewDrawnHunter = _preview != nullptr ? _preview->Shown() : MphRead::Hunter::Random;
        _previewDrawnSuit = _preview != nullptr ? _preview->ShownSuit() : -1;
    }
}
