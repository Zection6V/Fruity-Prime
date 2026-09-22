#pragma once

#include "../../Formats/Types.hpp"

#include <memory>
#include <vector>

namespace MphRead
{
    class RenderItem;

    namespace Mods::Render
    {
        class HunterPreviewEntity;
    }
}

// Scene members contributed by the PreviewPass.cs partial. The render state it
// draws with (_targetSize, _shaderLocations, the matrices, _hasFog, FogOn and
// RenderItem) belongs to the Renderer.cs partial.
#define MPHREAD_SCENE_PREVIEW_PASS_MEMBERS \
public: \
    [[nodiscard]] static float PreviewLeft() noexcept; \
    static void PreviewLeft(float value) noexcept; \
    [[nodiscard]] static float PreviewTop() noexcept; \
    static void PreviewTop(float value) noexcept; \
    [[nodiscard]] static float PreviewRight() noexcept; \
    static void PreviewRight(float value) noexcept; \
    [[nodiscard]] static float PreviewBottom() noexcept; \
    static void PreviewBottom(float value) noexcept; \
    [[nodiscard]] static bool PreviewWanted() noexcept; \
    static void PreviewWanted(bool value) noexcept; \
    void ModStepPreview(); \
    [[nodiscard]] bool ModPreviewDrawn() const noexcept; \
private: \
    void ModCollectPreview(); \
    void ModDrawPreview(); \
    std::vector<std::shared_ptr<::MphRead::RenderItem>> _previewItems{}; \
    std::shared_ptr<::MphRead::Mods::Render::HunterPreviewEntity> _preview{}; \
    bool _collectingPreview = false; \
    static float _previewLeft; \
    static float _previewTop; \
    static float _previewRight; \
    static float _previewBottom; \
    static bool _previewWanted; \
    static const ::OpenTK::Mathematics::Vector3 _previewEye; \
    static const ::OpenTK::Mathematics::Vector3 _previewTarget; \
    static constexpr float PreviewFov = 40.0F; \
    static const ::OpenTK::Mathematics::Vector4 _previewBack;
