#pragma once

#include "../../Formats/Types.hpp"
#include "../../Shaders.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace MphRead
{
    class RenderItem;

    namespace Mods::Render
    {
        class HunterPreviewEntity;
    }

    // Transitional declaration surface for the Scene partial contributed by
    // PreviewPass.cs. The canonical Scene owner has not yet aggregated this
    // migration slice; only members consumed by this C# partial are declared.
    class Scene
    {
    public:
        [[nodiscard]] static float PreviewLeft() noexcept;
        static void PreviewLeft(float value) noexcept;
        [[nodiscard]] static float PreviewTop() noexcept;
        static void PreviewTop(float value) noexcept;
        [[nodiscard]] static float PreviewRight() noexcept;
        static void PreviewRight(float value) noexcept;
        [[nodiscard]] static float PreviewBottom() noexcept;
        static void PreviewBottom(float value) noexcept;
        [[nodiscard]] static bool PreviewWanted() noexcept;
        static void PreviewWanted(bool value) noexcept;

        void ModStepPreview();
        [[nodiscard]] bool ModPreviewDrawn() const noexcept;

    private:
        void ModCollectPreview();
        void ModDrawPreview();

        [[nodiscard]] bool FogOn() const;
        void RenderItem(const std::shared_ptr<MphRead::RenderItem>& item);

        std::vector<std::shared_ptr<MphRead::RenderItem>> _previewItems{};
        std::shared_ptr<Mods::Render::HunterPreviewEntity> _preview{};
        bool _collectingPreview = false;

        static float _previewLeft;
        static float _previewTop;
        static float _previewRight;
        static float _previewBottom;
        static bool _previewWanted;

        static const OpenTK::Mathematics::Vector3 _previewEye;
        static const OpenTK::Mathematics::Vector3 _previewTarget;
        static constexpr float PreviewFov = 40.0F;
        static const OpenTK::Mathematics::Vector4 _previewBack;

        OpenTK::Mathematics::Vector2i _targetSize{};
        std::shared_ptr<MphRead::ShaderLocations> _shaderLocations{};
        OpenTK::Mathematics::Matrix4 _perspectiveMatrix{};
        OpenTK::Mathematics::Matrix4 _viewMatrix{};
        bool _hasFog = false;
    };
}
