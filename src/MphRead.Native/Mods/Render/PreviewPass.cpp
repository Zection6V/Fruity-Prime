#include "PreviewPass.hpp"

#include "../EndScreen.hpp"
#include "HunterPreview.hpp"

#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>

namespace MphRead::Mods::Render::PreviewPassInterop
{
    void Enable(std::int32_t capability);
    void Disable(std::int32_t capability);
    void Scissor(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height);
    void ClearColor(float red, float green, float blue, float alpha);
    void Clear(std::uint32_t mask);
    void Viewport(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height);
    void UniformMatrix4(std::int32_t location, bool transpose,
        const ::OpenTK::Mathematics::Matrix4& value);
    void Uniform1(std::int32_t location, std::int32_t value);
    void DepthFunc(std::int32_t function);
    void DepthMask(bool enabled);
    void BlendFunc(std::int32_t source, std::int32_t destination);
    void PolygonMode(std::int32_t face, std::int32_t mode);
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

    [[nodiscard]] constexpr float DegreesToRadians(float value) noexcept
    {
        return value * (Pi / 180.0F);
    }

    [[nodiscard]] Matrix4 CreatePerspectiveFieldOfView(
        float fov, float aspect, float nearClip, float farClip)
    {
        const float yScale = 1.0F / std::tan(fov * 0.5F);
        const float xScale = yScale / aspect;
        const float range = nearClip - farClip;
        return Matrix4(
            Vector4(xScale, 0.0F, 0.0F, 0.0F),
            Vector4(0.0F, yScale, 0.0F, 0.0F),
            Vector4(0.0F, 0.0F, (farClip + nearClip) / range, -1.0F),
            Vector4(0.0F, 0.0F, (2.0F * farClip * nearClip) / range, 0.0F));
    }

    [[nodiscard]] Matrix4 LookAt(Vector3 eye, Vector3 target, Vector3 up)
    {
        const Vector3 z = Vector3(eye.X - target.X, eye.Y - target.Y, eye.Z - target.Z).Normalized();
        const Vector3 x = Vector3::Cross(up, z).Normalized();
        const Vector3 y = Vector3::Cross(z, x);
        return Matrix4(
            Vector4(x.X, y.X, z.X, 0.0F),
            Vector4(x.Y, y.Y, z.Y, 0.0F),
            Vector4(x.Z, y.Z, z.Z, 0.0F),
            Vector4(-Vector3::Dot(x, eye), -Vector3::Dot(y, eye), -Vector3::Dot(z, eye), 1.0F));
    }

    [[nodiscard]] float RoundToEven(float value) noexcept
    {
        if (!std::isfinite(value) || std::fabs(value) >= 8388608.0F)
        {
            return value;
        }
        const float floorValue = std::floor(value);
        const float fraction = value - floorValue;
        if (fraction < 0.5F)
        {
            return floorValue;
        }
        if (fraction > 0.5F)
        {
            return floorValue + 1.0F;
        }
        return std::fmod(floorValue, 2.0F) == 0.0F
            ? floorValue
            : floorValue + 1.0F;
    }

    [[nodiscard]] std::int32_t FloatToInt32Unchecked(float value) noexcept
    {
        constexpr float Int32UpperExclusive = 2147483648.0F;
        constexpr float Int32LowerInclusive = -2147483648.0F;
        if (!std::isfinite(value) || value >= Int32UpperExclusive || value < Int32LowerInclusive)
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        return static_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::int32_t RoundPixel(float value) noexcept
    {
        return FloatToInt32Unchecked(RoundToEven(value));
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

    void Scene::ModStepPreview()
    {
        if (!Mods::EndScreen::Available())
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
        _preview->SetUp(Mods::EndScreen::Hunter(), Mods::EndScreen::Suit());
        _preview->Step();
    }

    void Scene::ModCollectPreview()
    {
        _previewItems.clear();
        if (!Mods::EndScreen::Available() || !_preview || !_preview->Ready())
        {
            return;
        }
        _collectingPreview = true;
        PreviewCollectFinally finally(_collectingPreview);
        try
        {
            _preview->GetDrawInfo();
        }
        catch (const std::exception& ex)
        {
            std::cout << "[endscreen] preview draw failed: " << ex.what() << '\n';
            _previewItems.clear();
        }
    }

    bool Scene::ModPreviewDrawn() const noexcept
    {
        return !_previewItems.empty()
            && _previewRight - _previewLeft > 0.001F
            && _previewBottom - _previewTop > 0.001F;
    }

    void Scene::ModDrawPreview()
    {
        if (_previewItems.empty() || !_previewWanted)
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

        using namespace Mods::Render::PreviewPassInterop;
        Enable(ScissorTest);
        Scissor(x, y, width, height);
        ClearColor(_previewBack.X, _previewBack.Y, _previewBack.Z, _previewBack.W);
        Clear(ColorBufferBit | DepthBufferBit);
        ClearColor(0.0F, 0.0F, 0.0F, 0.0F);
        Viewport(x, y, width, height);
        Matrix4 projection = CreatePerspectiveFieldOfView(
            DegreesToRadians(PreviewFov), width / static_cast<float>(height), 0.1F, 100.0F);
        Matrix4 view = LookAt(_previewEye, _previewTarget, Vector3(0.0F, 1.0F, 0.0F));
        if (!_shaderLocations)
        {
            throw System::NullReferenceException();
        }
        UniformMatrix4(_shaderLocations->ProjectionMatrix, false, projection);
        UniformMatrix4(_shaderLocations->ViewMatrix, false, view);
        Uniform1(_shaderLocations->UseFog, 0);
        Enable(DepthTest);
        DepthFunc(Less);
        DepthMask(true);
        Disable(StencilTest);
        Enable(Blend);
        BlendFunc(SrcAlpha, OneMinusSrcAlpha);
        Disable(AlphaTest);
        for (std::size_t i = 0; i < _previewItems.size(); ++i)
        {
            RenderItem(_previewItems[i]);
        }
        Disable(ScissorTest);
        Viewport(0, 0, target.X, target.Y);
        UniformMatrix4(_shaderLocations->ProjectionMatrix, false, _perspectiveMatrix);
        UniformMatrix4(_shaderLocations->ViewMatrix, false, _viewMatrix);
        Uniform1(_shaderLocations->UseFog, _hasFog && FogOn() ? 1 : 0);
        PolygonMode(FrontAndBack, Fill);
    }
}
